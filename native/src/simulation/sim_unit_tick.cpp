#include "sim_unit_tick.hpp"

#include "sim_combat.hpp"
#include "sim_constants.hpp"
#include "sim_movement.hpp"
#include "sim_periodic.hpp"
#include "sim_spatial.hpp"
#include "sim_stats.hpp"
#include "sim_stats_modifiers.hpp"
#include "sim_targeting.hpp"

#include "../stat_definitions.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <chrono>
#include <cstdint>

namespace sim {
namespace unit_tick {
namespace {

uint64_t profile_elapsed_ns(std::chrono::steady_clock::time_point t0) {
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now() - t0).count();
}

struct SimProfileAccScope {
	bool enabled = false;
	uint64_t *accum = nullptr;
	std::chrono::steady_clock::time_point t0{};
	explicit SimProfileAccScope(bool profile_sim, uint64_t *out_accum)
			: enabled(profile_sim && out_accum != nullptr), accum(out_accum) {
		if (enabled) {
			t0 = std::chrono::steady_clock::now();
		}
	}
	~SimProfileAccScope() {
		if (enabled && accum != nullptr) {
			*accum += profile_elapsed_ns(t0);
		}
	}
	SimProfileAccScope(const SimProfileAccScope &) = delete;
	SimProfileAccScope &operator=(const SimProfileAccScope &) = delete;
};

inline const StringName &sn_player() {
	static const StringName s("player");
	return s;
}

inline const StringName &sn_on_tick() {
	static const StringName s("on_tick");
	return s;
}

inline const StringName &sn_passive() {
	static const StringName s("passive");
	return s;
}

void sync_targeting_frame_unit(SimHostCallbacks &host, const UnitState &unit) {
	if (host.sync_targeting_frame_unit != nullptr) {
		host.sync_targeting_frame_unit(host.user_data, unit);
	}
}

void execute_effect(SimHostCallbacks &host, const EffectRecord &effect, EffectContext &context) {
	if (host.execute_effect != nullptr) {
		host.execute_effect(host.user_data, effect, context);
	}
}

} // namespace

void cooldowns_and_cc(
		SimWorld &world,
		UnitState &unit,
		const UnitTickHostHooks &tick_hooks,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;
	const double tick_rate = world.tick_rate;

	SimProfileAccScope _uu_cc(profile_sim, profile.uu_cooldowns_cc);

	if (unit.attack_cooldown > 0.0) {
		SimProfileAccScope _ucc_acd(profile_sim, profile.ucc_attack_cd);
		if (!uc(world, unit).is_channeling && !(unit.casting_remaining >= 0.0 && unit.has_casting_effect)) {
			unit.attack_cooldown -= tick_rate;
			if (unit.attack_cooldown < 0.0) {
				unit.attack_cooldown = 0.0;
			}
		}
	}
	if (unit.ability_cooldown > 0.0) {
		SimProfileAccScope _ucc_abcd(profile_sim, profile.ucc_ability_cd);
		unit.ability_cooldown -= tick_rate;
		if (unit.ability_cooldown < 0.0) {
			unit.ability_cooldown = 0.0;
		}
	}
	if (unit.retarget_timer > 0.0) {
		SimProfileAccScope _ucc_ret(profile_sim, profile.ucc_retarget);
		unit.retarget_timer -= tick_rate;
		if (unit.retarget_timer < 0.0) {
			unit.retarget_timer = 0.0;
		}
	}
	if (unit.target_switch_lock_timer > 0.0) {
		SimProfileAccScope _ucc_tsw(profile_sim, profile.ucc_target_switch);
		unit.target_switch_lock_timer -= tick_rate;
		if (unit.target_switch_lock_timer < 0.0) {
			unit.target_switch_lock_timer = 0.0;
		}
	}
	if (unit.stun_remaining > 0.0) {
		SimProfileAccScope _ucc_stun(profile_sim, profile.ucc_stun);
		unit.hard_cc_seconds += Math::min(unit.stun_remaining, tick_rate);
		unit.stun_remaining -= tick_rate;
		if (unit.stun_remaining < 0.0) {
			unit.stun_remaining = 0.0;
		}
	}
	if (unit.slow_remaining > 0.0) {
		SimProfileAccScope _ucc_slow(profile_sim, profile.ucc_slow);
		unit.slow_remaining -= tick_rate;
		if (unit.slow_remaining <= 0.0) {
			unit.slow_remaining = 0.0;
			unit.slow_move_mult = 1.0;
		}
	}
	if (unit.root_remaining > 0.0) {
		SimProfileAccScope _ucc_root(profile_sim, profile.ucc_root);
		unit.root_remaining -= tick_rate;
		if (unit.root_remaining < 0.0) {
			unit.root_remaining = 0.0;
		}
	}
	if (unit.silence_ability_remaining > 0.0 || unit.silence_ultimate_remaining > 0.0) {
		SimProfileAccScope _ucc_sil(profile_sim, profile.ucc_silence);
		unit.silence_ability_remaining -= tick_rate;
		if (unit.silence_ability_remaining < 0.0) {
			unit.silence_ability_remaining = 0.0;
		}
	}
	unit.silence_ultimate_remaining -= tick_rate;
	if (unit.silence_ultimate_remaining < 0.0) {
		unit.silence_ultimate_remaining = 0.0;
	}
	unit.silence_remaining = Math::max(unit.silence_ability_remaining, unit.silence_ultimate_remaining);
	unit.silence_blocks_abilities = unit.silence_ability_remaining > 0.0;
	unit.silence_blocks_ultimates = unit.silence_ultimate_remaining > 0.0;
	if (unit.disarm_remaining > 0.0) {
		SimProfileAccScope _ucc_dis(profile_sim, profile.ucc_disarm);
		unit.disarm_remaining -= tick_rate;
		if (unit.disarm_remaining < 0.0) {
			unit.disarm_remaining = 0.0;
		}
	}
	if (unit.stealth_remaining > 0.0) {
		SimProfileAccScope _ucc_ste(profile_sim, profile.ucc_stealth);
		unit.stealth_remaining -= tick_rate;
		if (unit.stealth_remaining <= 0.0) {
			unit.stealth_remaining = 0.0;
			unit.stealth_break_on_attack = false;
			unit.stealth_break_on_ability = false;
			unit.stealth_break_on_damage_taken = false;
		}
	}
	if (unit.shield > 0.0) {
		SimProfileAccScope _ucc_shi(profile_sim, profile.ucc_shield);
		unit.shield *= (1.0 - SHIELD_DECAY_RATE * tick_rate);
		if (unit.shield < 0.01) {
			unit.shield = 0.0;
		}
	}
	if (!uc(world, unit).reflect_buffs.empty()) {
		SimProfileAccScope _ucc_ref(profile_sim, profile.ucc_reflect);
		auto &reflect_buffs = uc(world, unit).reflect_buffs;
		size_t index = 0;
		while (index < reflect_buffs.size()) {
			reflect_buffs[index].remaining_duration = Math::max(0.0, reflect_buffs[index].remaining_duration - tick_rate);
			if (reflect_buffs[index].remaining_duration <= 0.0) {
				reflect_buffs.erase(reflect_buffs.begin() + static_cast<std::ptrdiff_t>(index));
			} else {
				++index;
			}
		}
	}
	if (unit.taunt_remaining > 0.0) {
		SimProfileAccScope _ucc_tau(profile_sim, profile.ucc_taunt);
		unit.taunt_remaining = Math::max(0.0, unit.taunt_remaining - tick_rate);
		if (unit.taunt_remaining <= 0.0) {
			unit.taunt_remaining = 0.0;
			unit.taunt_target_id = 0;
		}
	}
	if (unit.forced_target_remaining > 0.0) {
		SimProfileAccScope _ucc_ft(profile_sim, profile.ucc_forced_target);
		unit.forced_target_remaining = Math::max(0.0, unit.forced_target_remaining - tick_rate);
	}
	if (unit.last_kite_timer > 0.0) {
		unit.last_kite_timer = Math::max(0.0, unit.last_kite_timer - tick_rate);
	}

	bool has_temporary_stat_modifiers = !unit.stat_modifiers.is_empty()
#define X(name, def, min_val, max_val) || unit.stat_temp_##name > 0.0
			STAT_LIST
#undef X
			;

	if (!unit.stat_stacks.is_empty()) {
		stats_modifiers::update_stacks(unit, tick_rate, world.time);
	}

	if (has_temporary_stat_modifiers) {
		stats_modifiers::update_stat_modifier_durations(unit, tick_rate);
		stats_modifiers::clear_expired_stat_modifiers(unit);
	}
	if (unit.forced_target_remaining <= 0.0) {
		unit.forced_target_remaining = 0.0;
		unit.forced_target_id = 0;
		uc(world, unit).forced_target_kind = StringName();
	}
}

void apply_sudden_death_overtime(SimWorld &world, const UnitTickMatchState &match) {
	if (match.sudden_death_ticks <= 0) {
		return;
	}

	const double damage_rate = OVERTIME_DAMAGE_BASE_RATE;

	for (UnitState &overtime_unit : world.units) {
		if (!overtime_unit.alive) {
			continue;
		}

		const double max_hp = get_effective_max_hp(overtime_unit);
		const double damage = 1 + max_hp * damage_rate;

		const double shield_before = overtime_unit.shield;
		const double absorbed = Math::min(shield_before, damage);
		overtime_unit.shield = Math::max(0.0, shield_before - absorbed);
		const double hp_loss = Math::max(0.0, damage - absorbed);
		overtime_unit.hp = Math::max(0.0, overtime_unit.hp - hp_loss);

		if (overtime_unit.hp <= 0.0 && overtime_unit.alive) {
			overtime_unit.alive = false;
			if (overtime_unit.team == sn_player()) {
				if (match.enemy_kills != nullptr) {
					++(*match.enemy_kills);
				}
			} else if (match.player_kills != nullptr) {
				++(*match.player_kills);
			}
		}
	}
}

void separation(SimWorld &world, UnitState &unit, UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;
	const double tick_rate = world.tick_rate;

	SimProfileAccScope _uu_sep(profile_sim, profile.uu_separation);
	if (unit.stun_remaining <= 0.0 && unit.root_remaining <= 0.0) {
		const double move_speed = get_effective_move_speed(unit) * movement_speed_multiplier(unit);
		if (move_speed > 0.0) {
			const double attack_range = get_effective_attack_range(unit);
			const double radius = attack_range > SEPARATION_RANGE_THRESHOLD ? SEPARATION_RADIUS_RANGED : SEPARATION_RADIUS_MELEE;
			const double r2 = radius * radius;
			const double threshold_r2 = 4.0 * r2;
			const double ux = unit.pos_x;
			const double uy = unit.pos_y;
			double sep_x = 0.0;
			double sep_y = 0.0;
			const std::vector<int64_t> &ally_indices = alive_indices_for_team(world, unit.team);
			auto accumulate_separation_from_ally = [&](int64_t idx) {
				const UnitState &ally = world.units[idx];
				if (!ally.alive || ally.instance_id == unit.instance_id) {
					return;
				}
				const double ax = ally.pos_x;
				const double ay = ally.pos_y;
				const double dx = ux - ax;
				const double dy = uy - ay;
				const double d2 = dx * dx + dy * dy;
				if (d2 <= EPSILON || d2 >= threshold_r2) {
					return;
				}
				if (d2 >= r2) {
					return;
				}
				const double d = Math::sqrt(d2);
				const double force = (radius - d) / radius;
				sep_x += (dx / d) * force;
				sep_y += (dy / d) * force;
			};
			if (int64_t(ally_indices.size()) >= SPATIAL_SEPARATION_TEAM_THRESHOLD) {
				fill_buckets_for_indices(world, ally_indices);
				stamp_separation_candidates(world, ux, uy, radius, unit.team, unit.instance_id);
				for (int64_t idx : ally_indices) {
					if (!stamp_has(world, idx)) {
						continue;
					}
					accumulate_separation_from_ally(idx);
				}
			} else {
				for (int64_t idx : ally_indices) {
					accumulate_separation_from_ally(idx);
				}
			}
			if (!Math::is_zero_approx(sep_x) || !Math::is_zero_approx(sep_y)) {
				const double nudge_speed = move_speed * NUDGE_SPEED_MODIFIER * tick_rate;
				const double nx = sep_x * nudge_speed;
				const double ny = sep_y * nudge_speed;
				unit.pos_x = Math::clamp(ux + nx, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
				unit.pos_y = Math::clamp(uy + ny, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			}
		}
	}
}

void threat_and_assist(
		SimWorld &world,
		UnitState &unit,
		const UnitStrategy &strategy,
		SimHostCallbacks &host,
		const UnitTickHostHooks &tick_hooks,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;

	SimProfileAccScope _uu_ta(profile_sim, profile.uu_threat_and_assist);
	const double old_threat = unit.perceived_threat;
	unit.perceived_threat = Math::max(0.0, unit.perceived_threat - strategy.threat_decay_rate * world.tick_rate);
	if (Math::abs(unit.perceived_threat - old_threat) >= 0.001) {
		sync_targeting_frame_unit(host, unit);
	}

	if (tick_hooks.prune_assist_window != nullptr) {
		tick_hooks.prune_assist_window(tick_hooks.user_data, unit);
	}
}

bool regen_and_periodic(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks &host,
		const channel::ChannelHostHooks &channel_hooks,
		const UnitTickHostHooks &tick_hooks,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;
	const double tick_rate = world.tick_rate;

	SimProfileAccScope _uu_regen(profile_sim, profile.uu_regen_on_tick);
	UnitStateCold &cold = uc(world, unit);
	const std::vector<EffectRecord> &effects = combat::collect_effects(world, unit, sn_on_tick());

	const bool has_regen_work = !effects.empty() || uc(world, unit).is_channeling;
	if (has_regen_work) {
		{
			SimProfileAccScope _ur_eff(profile_sim, profile.ur_effects);
			if (cold.on_tick_effect_accumulators.size() < effects.size()) {
				cold.on_tick_effect_accumulators.resize(effects.size(), 0.0);
			}
			for (size_t effect_index = 0; effect_index < effects.size(); ++effect_index) {
				const EffectRecord &effect = effects[effect_index];
				double &accumulator = cold.on_tick_effect_accumulators[effect_index];
				accumulator += tick_rate;
				if (accumulator >= effect.on_tick_interval) {
					accumulator -= effect.on_tick_interval;
					EffectContext context = combat::build_context(unit, nullptr, nullptr, 0.0, sn_passive());
					execute_effect(host, effect, context);
				}
			}
		}

		if (uc(world, unit).is_channeling) {
			SimProfileAccScope _ur_chn(profile_sim, profile.ur_channel);
			channel::process_channel_tick(world, host, channel_hooks, unit, tick_rate);
		}
	}

	if (!cold.periodic_effects.empty()) {
		SimProfileAccScope _ur_per(profile_sim, profile.ur_periodic);
		periodic::tick_periodic_effects(world, host, unit, tick_rate);
	}

	return unit.stun_remaining > 0.0;
}

bool casting(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks &host,
		const combat::CombatHostHooks &combat_hooks,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;

	bool should_return_casting = false;
	if (unit.casting_remaining >= 0.0 && unit.has_casting_effect) {
		SimProfileAccScope _uu_cast(profile_sim, profile.uu_casting);
		unit.casting_remaining = Math::max(0.0, unit.casting_remaining - world.tick_rate);
		if (unit.casting_remaining <= 0.0) {
			combat::resolve_cast(world, host, combat_hooks, unit);
			unit.cast_resolved_this_tick = true;
		}
		should_return_casting = true;
	}
	return should_return_casting;
}

TargetingResult targeting(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks &host,
		const UnitTickHostHooks &tick_hooks,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;
	TargetingResult result;

	SimProfileAccScope _uu_tgt(profile_sim, profile.uu_targeting);
	if (tick_hooks.select_ally_target != nullptr) {
		result.target_ally = tick_hooks.select_ally_target(tick_hooks.user_data, unit);
	}
	unit.current_ally_target_id = result.target_ally == nullptr ? 0 : result.target_ally->instance_id;
	if (tick_hooks.select_enemy_target != nullptr) {
		result.target = tick_hooks.select_enemy_target(tick_hooks.user_data, unit, profile_sim);
	}
	if (result.target == nullptr) {
		unit.target_id = 0;
		unit.target_index = -1;
		sync_targeting_frame_unit(host, unit);
		result.stop = true;
	}
	return result;
}

bool combat_actions(
		SimWorld &world,
		UnitState &unit,
		UnitState &target,
		SimHostCallbacks &host,
		const combat::CombatHostHooks &combat_hooks,
		std::vector<ProjectileState> &projectiles,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;

	SimProfileAccScope _uu_combat(profile_sim, profile.uu_combat);
	{
		SimProfileAccScope _uc_dc(profile_sim, profile.uc_distance_calc);
		const double effective_range = targeting::effective_attack_range(unit);
		const double dx = target.pos_x - unit.pos_x;
		const double dy = target.pos_y - unit.pos_y;
		const double dist_sq = dx * dx + dy * dy;
		const double range_sq = effective_range * effective_range;
		const bool in_contact = (dist_sq <= range_sq);
		const double distance = Math::sqrt(dist_sq);

		const bool can_cast_ultimate = in_contact || !unit.ultimate_requires_target_in_range;
		const bool can_cast_ability = in_contact || !unit.ability_requires_target_in_range;

		if (can_cast_ultimate) {
			SimProfileAccScope _uc_ab(profile_sim, profile.uc_ability);
			if (combat::try_cast_ultimate(world, host, combat_hooks, unit, target, distance)) {
				return true;
			}
		}
		if (can_cast_ability) {
			SimProfileAccScope _uc_ab2(profile_sim, profile.uc_ability);
			if (combat::try_cast_ability(world, host, combat_hooks, unit, target, distance)) {
				return true;
			}
		}
		if (in_contact) {
			if (unit.attack_cooldown <= 0.0) {
				if (!uc(world, unit).is_channeling) {
					if (unit.combat.attack_speed > 0.0) {
						SimProfileAccScope _uc_aa(profile_sim, profile.uc_auto_attack);
						combat::perform_auto_attack(world, host, combat_hooks, unit, target, distance, projectiles);
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool movement(
		SimWorld &world,
		UnitState &unit,
		UnitState &target,
		const UnitStrategy &strategy,
		SimHostCallbacks &host,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;

	bool should_return = false;
	SimProfileAccScope _uu_move(profile_sim, profile.uu_movement);
	if (unit.root_remaining > 0.0) {
		should_return = true;
	} else {
		if (strategy.prefers_kiting && (unit.attack_cooldown > 0.0 || unit.combat.attack_speed == 0.0) && unit.taunt_remaining <= 0.0) {
			SimProfileAccScope _um_kit(profile_sim, profile.um_kiting);
			movement::KiteProfileCounters kite_profile{};
			if (profile_sim) {
				kite_profile.active = true;
				kite_profile.kiting_spatial = profile.um_kiting_spatial;
				kite_profile.kiting_brute = profile.um_kiting_brute;
			}
			if (movement::kite_from_enemies(world, host, unit, kite_profile.active ? &kite_profile : nullptr)) {
				should_return = true;
			}
		}
		if (!should_return) {
			SimProfileAccScope _um_tow(profile_sim, profile.um_toward);
			const double distance = distance_between(unit, target);
			const double actual_attack_range = targeting::attack_range(unit);

			if (distance > actual_attack_range) {
				movement::move_toward_target_with_range(world, unit, target, actual_attack_range);
			}
		}
	}
	return should_return;
}

void update_unit(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks &host,
		const channel::ChannelHostHooks &channel_hooks,
		const combat::CombatHostHooks &combat_hooks,
		const UnitTickHostHooks &tick_hooks,
		const UnitTickMatchState &match,
		UnitTickProfileCounters &profile,
		std::vector<ProjectileState> &projectiles) {
	const bool profile_sim = profile.profile_sim;

	if (!unit.alive) {
		SimProfileAccScope _uu_dead(profile_sim, profile.uu_dead_respawn);
		unit.respawn_timer = Math::max(0.0, unit.respawn_timer - world.tick_rate);
		if (unit.respawn_timer <= 0.0 && tick_hooks.respawn_unit != nullptr) {
			tick_hooks.respawn_unit(tick_hooks.user_data, unit);
		}
		return;
	}

	if (tick_hooks.strategy_for_unit == nullptr) {
		return;
	}
	const UnitStrategy &strategy = tick_hooks.strategy_for_unit(tick_hooks.user_data, unit);

	cooldowns_and_cc(world, unit, tick_hooks, profile);
	apply_sudden_death_overtime(world, match);
	separation(world, unit, profile);
	threat_and_assist(world, unit, strategy, host, tick_hooks, profile);

	if (regen_and_periodic(world, unit, host, channel_hooks, tick_hooks, profile)) {
		return;
	}
	if (casting(world, unit, host, combat_hooks, profile)) {
		return;
	}

	const TargetingResult targets = targeting(world, unit, host, tick_hooks, profile);
	if (targets.stop || targets.target == nullptr) {
		return;
	}
	if (combat_actions(world, unit, *targets.target, host, combat_hooks, projectiles, profile)) {
		return;
	}
	if (movement(world, unit, *targets.target, strategy, host, profile)) {
		return;
	}
}

} // namespace unit_tick
} // namespace sim
