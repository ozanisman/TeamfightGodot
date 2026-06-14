#include "sim_match_lifecycle.hpp"

#include "sim_combat.hpp"
#include "sim_constants.hpp"
#include "sim_periodic.hpp"
#include "sim_stats.inl.hpp"
#include "sim_stats_modifiers.hpp"
#include "sim_targeting.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <set>
#include <unordered_map>
#include <vector>

namespace sim {
namespace match {
namespace {

constexpr int SPAWN_SLOT_COUNT = 9;

inline const StringName &sn_player() {
	static const StringName s("player");
	return s;
}

inline const StringName &sn_enemy() {
	static const StringName s("enemy");
	return s;
}

inline const StringName &sn_minion() {
	static const StringName s("minion");
	return s;
}

inline const StringName &sn_takedown() {
	static const StringName s("takedown");
	return s;
}

inline const StringName &sn_death() {
	static const StringName s("death");
	return s;
}

std::vector<bool> &slots_for_team(SpawnSlotState &slots, const StringName &team) {
	return team == sn_player() ? slots.player_slots_used : slots.enemy_slots_used;
}

const std::vector<double> &spawn_points() {
	static const std::vector<double> points = { 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0 };
	return points;
}

uint32_t random_uint32(SpawnSlotState &slots) {
	if (slots.rand_uint32 != nullptr) {
		return slots.rand_uint32(slots.rand_user_data);
	}
	return 0;
}

void sync_targeting_frame_index(SimHostCallbacks &host, int64_t index, const UnitState &unit) {
	if (host.sync_targeting_frame_index != nullptr) {
		host.sync_targeting_frame_index(host.user_data, index, unit);
	}
}

void emit_trace(SimHostCallbacks &host, const StringName &kind, int64_t src_id, int64_t tgt_id, double val) {
	if (host.emit_trace != nullptr) {
		host.emit_trace(host.user_data, kind, src_id, tgt_id, val);
	}
}

void record_passive_aoe_fx(ViewerHooks *viewer, const UnitState &unit, double radius, const StringName &passive_id) {
	if (viewer != nullptr && viewer->record_passive_aoe_fx != nullptr) {
		viewer->record_passive_aoe_fx(viewer->user_data, unit, radius, passive_id);
	}
}

double respawn_x_base(const UnitState &unit, const UnitStateCold &cold) {
	double x_base = (unit.team == sn_player()) ? PLAYER_SPAWN_X_BASE : ENEMY_SPAWN_X_BASE;
	const Dictionary stats = Dictionary(cold.stats);
	const double attack_range = double(stats.get("attack_range", 0.0));
	if (attack_range <= 1.0) {
		if (unit.team == sn_player()) {
			x_base += 0.5;
		} else {
			x_base -= 0.5;
		}
	}
	return x_base;
}

} // namespace

void clear_spawn_slots(SpawnSlotState &slots) {
	slots.player_slots_used.clear();
	slots.enemy_slots_used.clear();
}

int64_t assign_spawn_slot(SpawnSlotState &slots, const StringName &team) {
	std::vector<bool> &team_slots = slots_for_team(slots, team);
	if (team_slots.empty()) {
		team_slots.resize(SPAWN_SLOT_COUNT, false);
	}

	std::vector<int64_t> available_slots;
	available_slots.reserve(SPAWN_SLOT_COUNT);
	for (int64_t i = 0; i < SPAWN_SLOT_COUNT; ++i) {
		if (!team_slots[static_cast<size_t>(i)]) {
			available_slots.push_back(i);
		}
	}

	if (available_slots.empty()) {
		return int64_t(random_uint32(slots) % SPAWN_SLOT_COUNT);
	}

	const int64_t random_index = int64_t(random_uint32(slots) % available_slots.size());
	const int64_t selected_slot = available_slots[static_cast<size_t>(random_index)];
	team_slots[static_cast<size_t>(selected_slot)] = true;
	return selected_slot;
}

void release_spawn_slot(SpawnSlotState &slots, const StringName &team, int64_t slot_index) {
	std::vector<bool> &team_slots = slots_for_team(slots, team);
	if (slot_index >= 0 && slot_index < int64_t(team_slots.size())) {
		team_slots[static_cast<size_t>(slot_index)] = false;
	}
}

void handle_death(
		SimWorld &world,
		SimHostCallbacks &host,
		ViewerHooks *viewer,
		MatchScoreState &score,
		SpawnSlotState &slots,
		UnitState &killer,
		UnitState &target) {
	(void)killer;
	if (!target.alive) {
		return;
	}

	const int64_t target_id = target.instance_id;
	const int64_t target_index = targeting::unit_index_by_id(world, target_id);
	if (target_index >= 0 && score.roster != nullptr) {
		match_roster::remove_alive_index(world, *score.roster, target.team, target_index);
	}

	if (target.is_marksman_role || target.is_mage_role || target.is_support_role) {
		if (score.tick_ctx != nullptr) {
			if (target.team == sn_player()) {
				score.tick_ctx->player_backliner_alive_count =
						Math::max(0, score.tick_ctx->player_backliner_alive_count - 1);
			} else if (target.team == sn_enemy()) {
				score.tick_ctx->enemy_backliner_alive_count =
						Math::max(0, score.tick_ctx->enemy_backliner_alive_count - 1);
			}
		}
	}

	target.alive = false;
	target.respawn_timer = target.stats_dirty ? get_effective_respawn_time(target) : target.cached_respawn_time;
	ur(world, target).deaths += 1;
	sync_targeting_frame_index(host, target_index, target);

	periodic::clear_periodic_effects(world, target);

	const std::unordered_map<int64_t, UnitStateRare::DamageSourceEntry> &damage_sources = ur(world, target).damage_sources;
	int64_t killer_id = 0;
	double killer_damage = -1.0;
	for (const auto &entry : damage_sources) {
		const int64_t source_id = entry.first;
		const double dealt = entry.second.damage;
		const double hit_time = entry.second.last_time;
		if (score.time - hit_time > ASSIST_WINDOW) {
			continue;
		}
		bool better = false;
		if (killer_id == 0) {
			better = true;
		} else if (dealt > killer_damage + EPSILON) {
			better = true;
		} else if (Math::abs(dealt - killer_damage) <= EPSILON && source_id < killer_id) {
			better = true;
		}
		if (better) {
			killer_id = source_id;
			killer_damage = dealt;
		}
	}

	emit_trace(host, sn_death(), killer_id, target.instance_id, 0.0);

	UnitStateCold &target_cold = uc(world, target);

	record_unit_death_fx(viewer, target);

	UnitState *killer_unit = targeting::unit_by_id(world, killer_id);
	if (killer_unit != nullptr) {
		// Minion deaths do not count for KDA stats or trigger takedown effects
		static const StringName sn_minion("minion");
		if (uc(world, target).role_id != sn_minion) {
			ur(world, *killer_unit).kills += 1;
		}
		// Minion deaths do not count for team score
		if (score.player_kills != nullptr && killer_unit->team == sn_player() && uc(world, target).role_id != sn_minion) {
			*score.player_kills += 1;
		} else if (score.enemy_kills != nullptr && killer_unit->team == sn_enemy() && uc(world, target).role_id != sn_minion) {
			*score.enemy_kills += 1;
		}
		if (uc(world, target).role_id != sn_minion) {
			const EffectContext takedown_context =
					combat::build_context(*killer_unit, &target, nullptr, killer_damage, sn_takedown());
			combat::run_on_takedown_effects(world, host, *killer_unit, target, killer_damage, true, takedown_context);
		}
	}

	std::set<int64_t> assist_ids;
	std::unordered_map<int64_t, double> assist_damage_map;

	for (const auto &entry : damage_sources) {
		const int64_t source_id = entry.first;
		if (source_id == killer_id) {
			continue;
		}
		const double hit_time = entry.second.last_time;
		if (score.time - hit_time <= ASSIST_WINDOW) {
			UnitState *assist_unit = targeting::unit_by_id(world, source_id);
			if (assist_unit != nullptr) {
				// Minion deaths do not count for assist stats
				static const StringName sn_minion("minion");
				if (uc(world, target).role_id != sn_minion) {
					ur(world, *assist_unit).assists += 1;
				}
				assist_ids.insert(source_id);
				assist_damage_map[source_id] = entry.second.damage;
			}
		}
	}

	const std::unordered_map<int64_t, double> empty_benefactors;
	const std::unordered_map<int64_t, double> &recent_benefactors =
			killer_unit != nullptr ? ur(world, *killer_unit).recent_benefactors : empty_benefactors;
	for (const auto &entry : recent_benefactors) {
		const int64_t benefactor_id = entry.first;
		const double benefactor_time = entry.second;
		if (score.time - benefactor_time > ASSIST_WINDOW) {
			continue;
		}
		UnitState *assist_unit = targeting::unit_by_id(world, benefactor_id);
		if (assist_unit != nullptr) {
			// Minion deaths do not count for assist stats
			static const StringName sn_minion("minion");
			if (uc(world, target).role_id != sn_minion) {
				ur(world, *assist_unit).assists += 1;
			}
			assist_ids.insert(benefactor_id);
			if (assist_damage_map.find(benefactor_id) == assist_damage_map.end()) {
				assist_damage_map[benefactor_id] = 0.0;
			}
		}
	}

	for (const int64_t assist_id : assist_ids) {
		UnitState *assist_unit = targeting::unit_by_id(world, assist_id);
		if (assist_unit != nullptr) {
			// Minion deaths do not trigger takedown effects
			static const StringName sn_minion("minion");
			if (uc(world, target).role_id != sn_minion) {
				const double damage = assist_damage_map[assist_id];
				const EffectContext takedown_context =
						combat::build_context(*assist_unit, &target, nullptr, damage, sn_takedown());
				combat::run_on_takedown_effects(world, host, *assist_unit, target, damage, false, takedown_context);
			}
		}
	}

	if (target_cold.respawn_slot_index != -1) {
		release_spawn_slot(slots, target.team, target_cold.respawn_slot_index);
	}
	target_cold.respawn_slot_index = assign_spawn_slot(slots, target.team);
}

void respawn_unit(
		SimWorld &world,
		SimHostCallbacks &host,
		ViewerHooks *viewer,
		MatchScoreState &score,
		SpawnSlotState &slots,
		UnitState &unit) {
	(void)score;
	if (unit.alive) {
		return;
	}

	const int64_t unit_index = targeting::unit_index_by_id(world, unit.instance_id);
	UnitStateCold &cold = uc(world, unit);
	unit.alive = true;
	unit.hp = unit.stats_dirty ? get_effective_max_hp(unit) : unit.cached_max_hp;
	unit.mana = 0.0;
	unit.shield = 0.0;
	unit.perceived_threat = 0.0;
	unit.ability_cooldown = unit.stats_dirty ? get_effective_ability_cd(unit) : unit.cached_ability_cd;
	unit.stun_remaining = 0.0;
	unit.root_remaining = 0.0;
	unit.silence_remaining = 0.0;
	unit.silence_ability_remaining = 0.0;
	unit.silence_ultimate_remaining = 0.0;
	unit.silence_blocks_abilities = false;
	unit.silence_blocks_ultimates = false;
	unit.disarm_remaining = 0.0;
	unit.stealth_remaining = 0.0;
	unit.stealth_break_on_attack = false;
	unit.stealth_break_on_ability = false;
	unit.stealth_break_on_damage_taken = false;
	cold.reflect_buffs.clear();
	unit.taunt_remaining = 0.0;
	unit.forced_target_remaining = 0.0;
	unit.last_kite_timer = 0.0;
	unit.target_switch_lock_timer = 0.0;
	unit.respawn_timer = 0.0;

	stats_modifiers::clear_all_stat_modifiers(unit);

	ur(world, unit).damage_sources.clear();
	ur(world, unit).recent_benefactors.clear();
	ur(world, unit).last_hit_time = 0.0;
	std::fill(cold.on_tick_effect_accumulators.begin(), cold.on_tick_effect_accumulators.end(), 0.0);
	unit.respawned_this_tick = true;
	unit.cast_resolved_this_tick = false;
	unit.casting_remaining = 0.0;
	cold.casting_kind = StringName();
	cold.casting_effect = EffectRecord();

	for (const UnitStateCold::PassiveAoeInfo &aoe_info : cold.passive_aoe_info) {
		record_passive_aoe_fx(viewer, unit, aoe_info.radius, aoe_info.passive_id);
	}
	unit.casting_target_id = 0;
	unit.casting_ally_target_id = 0;
	unit.has_casting_effect = false;
	unit.taunt_target_id = 0;
	unit.forced_target_id = 0;
	cold.forced_target_kind = StringName();

	if (cold.respawn_slot_index == -1) {
		cold.respawn_slot_index = assign_spawn_slot(slots, unit.team);
	}

	const std::vector<double> &points = spawn_points();
	const double x_base = respawn_x_base(unit, cold);
	if (cold.respawn_slot_index >= 0 && cold.respawn_slot_index < int64_t(points.size())) {
		unit.pos_x = x_base;
		unit.pos_y = points[static_cast<size_t>(cold.respawn_slot_index)];
	} else if (slots.get_random_spawn_position != nullptr) {
		const Vector2 respawn_pos = slots.get_random_spawn_position(slots.rand_user_data, unit.team, true);
		unit.pos_x = respawn_pos.x;
		unit.pos_y = respawn_pos.y;
	}

	if (unit_index >= 0 && score.roster != nullptr) {
		match_roster::add_alive_index(world, *score.roster, unit.team, unit_index);
	}
	sync_targeting_frame_index(host, unit_index, unit);
}

} // namespace match
} // namespace sim
