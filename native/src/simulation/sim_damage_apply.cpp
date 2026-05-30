#include "sim_damage.hpp"
#include "sim_damage_internal.hpp"

#include "sim_constants.hpp"
#include "sim_spatial.hpp"
#include "sim_stats.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim {
namespace damage {

using namespace internal;

double apply_damage(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double damage,
		const StringName &damage_type,
		const StringName &action_kind,
		const EffectContext &context) {
	if (damage <= 0.0) {
		return 0.0;
	}

	const bool action_is_auto = action_kind == sn_auto();
	const bool action_is_ability = action_kind == sn_ability();
	const bool action_is_ultimate = action_kind == sn_ultimate();
	const bool action_is_passive = action_kind == sn_passive();
	const bool damage_is_physical = damage_type == sn_physical();
	const bool damage_is_magic = damage_type == sn_magic();
	double pre_res = damage;

	if (damage_type != sn_true()) {
		pre_res *= defense_multiplier(world, host, target, source, damage, action_kind);
	}

	if (action_is_auto) {
		pre_res *= auto_dodge_multiplier(world, host, target, source, damage);
	}

	double ally_defense_reduction = trigger_ally_defense_effects(world, host, target, source, pre_res, damage_type, action_kind, context);
	pre_res -= ally_defense_reduction;
	if (pre_res < 0.0) {
		pre_res = 0.0;
	}

	double final_damage = pre_res;
	if (damage_is_physical) {
		const double armor = get_effective_armor(target);
		final_damage *= (1.0 - armor);
	} else if (damage_is_magic) {
		const double mr = get_effective_magic_resist(target);
		final_damage *= (1.0 - mr);
	}

	if (final_damage <= 0.0) {
		return 0.0;
	}
	
	const double shield_before = target.shield;
	const double absorbed = Math::min(shield_before, final_damage);
	target.shield = Math::max(0.0, shield_before - absorbed);
	const double hp_loss = Math::max(0.0, final_damage - absorbed);
	target.hp = Math::max(0.0, target.hp - hp_loss);
	uc(world, target).damage_received += hp_loss;
	uc(world, target).damage_mitigated += Math::max(0.0, pre_res - final_damage);
	const double total_damage = absorbed + hp_loss;
	const double max_hp = get_effective_max_hp(target);
	if (max_hp > 0.0 && hp_loss > max_hp * THREAT_BURST_THRESHOLD) {
		target.perceived_threat += (hp_loss / max_hp) * THREAT_BURST_MULTIPLIER;
	}
	if (host.sync_targeting_frame_unit != nullptr) {
		host.sync_targeting_frame_unit(host.user_data, target);
	}
	if (source.instance_id != target.instance_id) {
		uc(world, source).damage_dealt += total_damage;
		if (action_is_auto) {
			uc(world, source).damage_dealt_auto += total_damage;
		} else if (action_is_ability) {
			uc(world, source).damage_dealt_ability += total_damage;
		} else if (action_is_ultimate) {
			uc(world, source).damage_dealt_ultimate += total_damage;
		} else if (action_is_passive) {
			uc(world, source).damage_dealt_passive += total_damage;
		}
	}
	if (hp_loss > 0.0 || absorbed > 0.0) {
		touch_damage_source(world, target, source.instance_id, total_damage);
		uc(world, target).last_hit_time = world.time;
	}
	if (total_damage > 1e-9 && host.viewer_record_damage_fx != nullptr) {
		host.viewer_record_damage_fx(host.user_data, source, target, total_damage, action_kind, damage_type);
	}
	maybe_apply_reflect_damage(world, host, source, target, total_damage, damage_type, context);
	if (target.hp <= 0.0) {
		run_post_take_damage_passives(world, host, target, total_damage, context);
		if (host.handle_death != nullptr) {
			host.handle_death(host.user_data, source, target);
		}
		return total_damage;
	}
	run_post_take_damage_passives(world, host, target, total_damage, context);
	return total_damage;
}

void maybe_apply_reflect_damage(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &attacker,
		UnitState &defender,
		double total_damage_applied,
		const StringName &damage_type,
		const EffectContext &context) {
	if (context.suppress_reflect_chain) {
		return;
	}
	if (!attacker.alive || attacker.instance_id == defender.instance_id) {
		return;
	}
	if (total_damage_applied <= 1e-9) {
		return;
	}

	for (const PassiveReflectEntry &entry : uc(world, defender).passive_reflect_entries) {
		bool applies = false;
		if (entry.damage_type == StringName("all")) {
			applies = true;
		} else if (entry.damage_type == StringName("physical") && damage_type == sn_physical()) {
			applies = true;
		}

		if (!applies) {
			continue;
		}

		const double reflected = total_damage_applied * entry.percentage;
		if (reflected > 1e-9) {
			EffectContext bounce = context;
			bounce.suppress_reflect_chain = true;
			bounce.source = &defender;
			bounce.target = &attacker;
			apply_damage(world, host, defender, attacker, reflected, damage_type, entry.action_kind, bounce);
		}
	}

	for (const UnitStateCold::ReflectBuff &buff : uc(world, defender).reflect_buffs) {
		if (buff.remaining_duration <= 0.0) {
			continue;
		}

		bool applies = false;
		if (buff.damage_type == StringName("all")) {
			applies = true;
		} else if (buff.damage_type == StringName("physical") && damage_type == sn_physical()) {
			applies = true;
		}

		if (!applies) {
			continue;
		}

		const double reflected = total_damage_applied * buff.percentage;
		if (reflected <= 1e-9) {
			continue;
		}

		UnitState *damage_source = unit_by_id(world, buff.source_instance_id);
		if (damage_source == nullptr) {
			UtilityFunctions::push_error(vformat(
					"[REFLECT ERROR] Missing reflect source; falling back to defender. stored_source_id=%d defender_id=%d attacker_id=%d action_kind=%s damage_type=%s reflected=%.3f",
					buff.source_instance_id,
					defender.instance_id,
					attacker.instance_id,
					String(buff.action_kind),
					String(damage_type),
					reflected));
			damage_source = &defender;
		}

		EffectContext bounce = context;
		bounce.suppress_reflect_chain = true;
		bounce.source = damage_source;
		bounce.target = &attacker;
		apply_damage(world, host, *damage_source, attacker, reflected, damage_type, buff.action_kind, bounce);
	}
}

void touch_damage_source(SimWorld &world, UnitState &target, int64_t source_id, double incoming_damage) {
	if (source_id <= 0 || incoming_damage <= 0.0) {
		return;
	}
	if (source_id == target.instance_id) {
		return;
	}
	UnitStateCold::DamageSourceEntry &entry = uc(world, target).damage_sources[source_id];
	entry.damage += incoming_damage;
	entry.last_time = world.time;
}

double trigger_ally_defense_effects(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &target,
		UnitState &source,
		double damage,
		const StringName &damage_type,
		const StringName &action_kind,
		const EffectContext &context) {
	if (context.suppress_reflect_chain) {
		return 0.0;
	}
	if (source.instance_id == target.instance_id) {
		return 0.0;
	}

	double total_redirected = 0.0;
	const StringName target_team = target.team;
	const std::vector<int64_t> &ally_indices = alive_indices_for_team(world, target_team);

	for (int64_t ally_index : ally_indices) {
		UnitState &ally = world.units[static_cast<size_t>(ally_index)];
		if (!ally.alive || ally.instance_id == target.instance_id) {
			continue;
		}

		const std::vector<EffectRecord> &ally_defense_effects = uc(world, ally).passive_effects[EFFECT_BUCKET_ON_ALLY_DEFENSE];
		if (ally_defense_effects.empty()) {
			continue;
		}

		const double ally_radius = uc(world, ally).on_ally_defense_radius;
		if (ally_radius <= 0.0) {
			continue;
		}
		const double dx = ally.pos_x - target.pos_x;
		const double dy = ally.pos_y - target.pos_y;
		const double dist_sq = dx * dx + dy * dy;
		const double radius_sq = ally_radius * ally_radius;

		if (dist_sq > radius_sq) {
			continue;
		}

		EffectContext ally_context = context;
		ally_context.source = &ally;
		ally_context.target = &target;
		ally_context.target_ally = nullptr;
		ally_context.damage = damage;
		ally_context.action_kind = action_kind;
		ally_context.distance = distance_between_coords(ally.pos_x, ally.pos_y, target.pos_x, target.pos_y);

		for (const EffectRecord &effect : ally_defense_effects) {
			if (effect.opcode == EFFECT_OPCODE_REDIRECT_DAMAGE) {
				const double redirect_ratio = effect.scalar0;
				const double reduction_ratio = effect.scalar1;
				const double redirect_cap = effect.scalar2;

				double redirected_damage = damage * redirect_ratio;
				if (redirect_cap > 0.0) {
					const double max_redirect = redirect_cap;
					if (redirected_damage > max_redirect) {
						redirected_damage = max_redirect;
					}
				}

				total_redirected += redirected_damage;

				const double mitigated_damage = redirected_damage * (1.0 - reduction_ratio);

				if (mitigated_damage > 1e-9) {
					const double mitigated_amount = redirected_damage - mitigated_damage;
					uc(world, ally).damage_mitigated += mitigated_amount;

					EffectContext redirect_context = context;
					redirect_context.source = &source;
					redirect_context.target = &ally;
					redirect_context.damage = mitigated_damage;
					redirect_context.action_kind = action_kind;
					redirect_context.suppress_reflect_chain = true;
					redirect_context.distance = distance_between_coords(source.pos_x, source.pos_y, ally.pos_x, ally.pos_y);
					apply_damage(world, host, source, ally, mitigated_damage, damage_type, redirect_context.action_kind, redirect_context);
				}
			} else if (host.execute_effect != nullptr) {
				host.execute_effect(host, effect, ally_context);
			}
		}
	}

	return total_redirected;
}

} // namespace damage
} // namespace sim
