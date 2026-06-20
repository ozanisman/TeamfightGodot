#include "sim_targeting.hpp"

#include "sim_constants.hpp"
#include "sim_stats.hpp"
#include "sim_targeting_internal.hpp"
#include <godot_cpp/variant/utility_functions.hpp>

#include <limits>
#include <vector>

namespace sim {
namespace targeting {

using namespace internal;

namespace {

UnitState *try_keep_current_enemy_target_early(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks *host,
		const CoordinatorTargetingState &state) {
	const bool profile_active = state.profile_targeting_active;
	auto reject = [&]() -> UnitState * {
		if (profile_active && state.tgt_enemy_early_keep_rejects != nullptr) {
			*state.tgt_enemy_early_keep_rejects += 1;
		}
		return nullptr;
	};

	if (unit.retarget_timer <= 0.0) {
		return reject();
	}
	if (unit.forced_target_id != 0 && unit.forced_target_remaining > 0.0) {
		return reject();
	}
	if (unit.taunt_target_id != 0 && unit.taunt_remaining > 0.0) {
		return reject();
	}

	const int64_t current_target_index = target_index_for_unit(world, unit);
	if (current_target_index < 0 || current_target_index >= int64_t(world.targeting_frame.size())) {
		return reject();
	}

	TargetingFrameEntry &current_frame = world.targeting_frame[static_cast<size_t>(current_target_index)];
	if (!current_frame.alive || current_frame.is_player_team == (unit.team == player_team_name()) || current_frame.stealth_remaining > 0.0) {
		return reject();
	}

	if (unit.is_assassin_role && (current_frame.is_tank_role || current_frame.is_fighter_role)) {
		const bool unit_is_player = unit.team == player_team_name();
		const int backliners_alive = unit_is_player ? world.tick_ctx.player_backliner_alive_count : world.tick_ctx.enemy_backliner_alive_count;
		if (backliners_alive > 0) {
			return reject();
		}
	}

	UnitState &current_target = world.units[static_cast<size_t>(current_target_index)];
	if (profile_active) {
		if (state.tgt_retarget_keeps != nullptr) {
			*state.tgt_retarget_keeps += 1;
		}
		if (state.tgt_enemy_early_keeps != nullptr) {
			*state.tgt_enemy_early_keeps += 1;
		}
	}
	set_current_target(world, unit, current_target, host);
	return &current_target;
}

} // namespace

UnitState *select_enemy_target(
		SimWorld &world,
		UnitState &unit,
		const UnitStrategy &strategy,
		const TargetScoreContext &score_ctx,
		const SimHostCallbacks *host,
		const TargetingProfileCounters *profile,
		const ScoreEnemyProfileCounters *score_profile,
		const TargetingDebugHooks *debug) {
	const bool profile_active = profile != nullptr && profile->active;

	int64_t forced_target_id = unit.forced_target_id;
	if (forced_target_id != 0 && unit.forced_target_remaining > 0.0) {
		UnitState *forced_target = unit_by_id(world, forced_target_id);
		if (forced_target != nullptr && forced_target->alive && forced_target->team != unit.team) {
			unit.retarget_timer = 0.0;
			unit.current_target_score = -1000.0;
			set_current_target(world, unit, *forced_target, host);
			return forced_target;
		}
	}

	int64_t taunt_id = unit.taunt_target_id;
	if (taunt_id != 0 && unit.taunt_remaining > 0.0) {
		UnitState *taunt_target = unit_by_id(world, taunt_id);
		if (taunt_target != nullptr && taunt_target->alive && taunt_target->team != unit.team) {
			unit.retarget_timer = 0.0;
			unit.current_target_score = -1000.0;
			set_current_target(world, unit, *taunt_target, host);
			return taunt_target;
		}
	}

	const int64_t current_target_index = target_index_for_unit(world, unit);
	UnitState *current_target_live = current_target_index >= 0 ? &world.units[static_cast<size_t>(current_target_index)] : nullptr;
	const TargetingFrameEntry *current_target = current_target_index >= 0 && current_target_index < int64_t(world.targeting_frame.size())
			? &world.targeting_frame[static_cast<size_t>(current_target_index)]
			: nullptr;
	bool forced_reason = true;
	if (current_target != nullptr && current_target->alive && current_target->is_player_team != (unit.team == player_team_name()) && current_target->stealth_remaining <= 0.0) {
		forced_reason = false;
	}

	const TickContext &ctx = world.tick_ctx;
	const bool unit_is_player = unit.team == player_team_name();
	const bool unit_is_assassin_role = unit.is_assassin_role;

	bool assassin_pressuring_frontline = false;
	if (!forced_reason && unit_is_assassin_role && current_target != nullptr) {
		if (current_target->is_tank_role || current_target->is_fighter_role) {
			const int bl_alive = unit_is_player ? ctx.player_backliner_alive_count : ctx.enemy_backliner_alive_count;
			assassin_pressuring_frontline = bl_alive > 0;
		}
	}

	if (!forced_reason && unit.retarget_timer > 0.0 && !assassin_pressuring_frontline) {
		if (profile_active && profile->retarget_keeps != nullptr) {
			*profile->retarget_keeps += 1;
		}
		if (current_target_live != nullptr) {
			set_current_target(world, unit, *current_target_live, host);
		}
		return current_target_live;
	}

	const int64_t unit_current_ally_target_id = unit.current_ally_target_id;
	const std::vector<int64_t> &enemy_indices = unit_is_player ? world.alive_enemy_indices : world.alive_player_indices;

	const TargetingFrameEntry *ally_for_peel = nullptr;
	if (unit_current_ally_target_id != 0) {
		int64_t ally_index = unit_index_by_id(world, unit_current_ally_target_id);
		if (ally_index >= 0 && ally_index < int64_t(world.targeting_frame.size())) {
			ally_for_peel = &world.targeting_frame[static_cast<size_t>(ally_index)];
		}
	}

	unit.retarget_timer = RETARGET_INTERVAL;

	bool current_target_raw_valid = false;
	double current_target_raw = 0.0;
	double current_target_dist_for_switch = -1.0;
	const double unit_x = unit.pos_x;
	const double unit_y = unit.pos_y;
	UnitState *best_live = nullptr;
	int64_t best_index = -1;
	double best_raw = std::numeric_limits<double>::infinity();
	double best_dist = std::numeric_limits<double>::infinity();

	const bool debug_scoring = debug != nullptr && debug->enabled;
	std::vector<std::pair<int64_t, double>> candidate_scores;
	if (debug_scoring) {
		candidate_scores.reserve(enemy_indices.size());
	}

	if (profile_active && profile->enemy_scans != nullptr) {
		*profile->enemy_scans += 1;
	}
	for (int64_t enemy_index : enemy_indices) {
		const TargetingFrameEntry &candidate = world.targeting_frame[static_cast<size_t>(enemy_index)];
		if (candidate.stealth_remaining > 0.0) {
			continue;
		}
		double dx = candidate.pos_x - unit_x;
		double dy = candidate.pos_y - unit_y;
		if (profile_active && profile->candidates_scored != nullptr) {
			*profile->candidates_scored += 1;
		}
		double dist = Math::sqrt(dx * dx + dy * dy);

		ScoreBreakdown local_breakdown;
		ScoreBreakdown *breakdown_ptr = debug_scoring ? &local_breakdown : nullptr;

		double raw = score_enemy_target(
				world,
				unit,
				candidate,
				ally_for_peel,
				strategy,
				ctx,
				score_ctx,
				dist,
				score_profile,
				enemy_index,
				breakdown_ptr);

		if (debug_scoring) {
			candidate_scores.push_back({candidate.instance_id, raw});
		}

		if (current_target_index >= 0 && enemy_index == current_target_index) {
			current_target_raw = raw;
			current_target_raw_valid = true;
			current_target_dist_for_switch = dist;
		}
		bool is_better = best_live == nullptr;
		if (!is_better) {
			const int64_t best_instance_id = world.targeting_frame[static_cast<size_t>(best_index)].instance_id;
			if (profile_active) {
				if (raw == best_raw) {
					if (profile->ties_adjusted != nullptr) {
						*profile->ties_adjusted += 1;
					}
					if (dist == best_dist) {
						if (profile->ties_distance != nullptr) {
							*profile->ties_distance += 1;
						}
						if (profile->ties_instance != nullptr) {
							*profile->ties_instance += 1;
						}
					}
				}
			}
			is_better = raw < best_raw
					|| (raw == best_raw && dist < best_dist)
					|| (raw == best_raw && dist == best_dist && candidate.instance_id < best_instance_id);
		}
		if (is_better) {
			best_live = &world.units[static_cast<size_t>(enemy_index)];
			best_index = enemy_index;
			best_raw = raw;
			best_dist = dist;
		}
	}
	if (best_live == nullptr) {
		unit.target_id = 0;
		unit.target_index = -1;
		unit.current_target_score = 0.0;
		sync_frame_unit(world, unit, host);
		return nullptr;
	}

	const TargetingFrameEntry *best_frame = &world.targeting_frame[static_cast<size_t>(best_index)];

	if (debug_scoring && debug->print_line != nullptr && debug->archetype_for_unit != nullptr) {
		String attacker_archetype = String(debug->archetype_for_unit(debug->user_data, unit));
		String msg = "[TARGET EVAL] Unit " + String::num_int64(unit.instance_id) + " (" + attacker_archetype + ") current target " + String::num_int64(unit.target_id) + " best " + String::num_int64(best_live->instance_id) + " (score " + String::num_real(best_raw) + "). All candidates:";
		debug->print_line(debug->user_data, msg);
		for (const auto &pair : candidate_scores) {
			const UnitState *candidate_unit = unit_by_id(world, pair.first);
			if (candidate_unit == nullptr) {
				continue;
			}
			String candidate_archetype = String(debug->archetype_for_unit(debug->user_data, *candidate_unit));
			String candidate_msg = "  - " + String::num_int64(pair.first) + " (" + candidate_archetype + "): " + String::num_real(pair.second);
			debug->print_line(debug->user_data, candidate_msg);
		}
	}

	if (debug != nullptr && debug->enabled && debug->print_score_breakdown != nullptr && debug->archetype_for_unit != nullptr) {
		ScoreBreakdown best_breakdown;
		double best_dist_debug = distance_between_coords(unit.pos_x, unit.pos_y, best_frame->pos_x, best_frame->pos_y);
		score_enemy_target(
				world,
				unit,
				*best_frame,
				ally_for_peel,
				strategy,
				ctx,
				score_ctx,
				best_dist_debug,
				score_profile,
				best_index,
				&best_breakdown);
		best_breakdown.total = best_raw;
		debug->print_score_breakdown(
				debug->user_data,
				best_breakdown,
				debug->archetype_for_unit(debug->user_data, unit),
				debug->archetype_for_unit(debug->user_data, *best_live));
	}

	if (forced_reason) {
		set_current_target(world, unit, *best_live, host);
		unit.current_target_score = best_raw;
		return best_live;
	}

	if (current_target != nullptr && best_frame->instance_id == current_target->instance_id) {
		unit.current_target_score = best_raw;
		if (current_target_live != nullptr) {
			set_current_target(world, unit, *current_target_live, host);
		}
		return current_target_live;
	}
	double current_score = 0.0;
	if (current_target_raw_valid) {
		current_score = current_target_raw;
	} else if (current_target != nullptr && current_target_live != nullptr) {
		double current_dist = distance_between_coords(unit.pos_x, unit.pos_y, current_target->pos_x, current_target->pos_y);
		current_score = score_enemy_target(
				world,
				unit,
				*current_target,
				ally_for_peel,
				strategy,
				ctx,
				score_ctx,
				current_dist,
				score_profile,
				current_target_index,
				nullptr);
	}

	if (assassin_pressuring_frontline) {
		bool best_is_backliner = (best_frame->is_marksman_role || best_frame->is_mage_role || best_frame->is_support_role);
		if (best_is_backliner) {
			set_current_target(world, unit, *best_live, host);
			unit.target_switch_lock_timer = 0.0;
			unit.current_target_score = best_raw;
			return best_live;
		}
	}

	if (current_target_live != nullptr && current_score <= best_raw + TARGET_STICKINESS_THRESHOLD) {
		unit.retarget_timer = RETARGET_INTERVAL + STICKINESS_RETARGET_BONUS;
		unit.current_target_score = current_score;
		set_current_target(world, unit, *current_target_live, host);
		return current_target_live;
	}

	if (current_target_live == nullptr || !should_switch(world, unit, current_score, best_raw, strategy, current_target_dist_for_switch)) {
		unit.current_target_score = current_score;
		if (current_target_live != nullptr) {
			set_current_target(world, unit, *current_target_live, host);
		}
		return current_target_live;
	}

	set_current_target(world, unit, *best_live, host);
	unit.target_switch_lock_timer = TARGET_SWITCH_LOCK_DURATION;
	unit.current_target_score = best_raw;
	return best_live;
}

UnitState *select_enemy_target_coordinator(
		SimWorld &world,
		UnitState &unit,
	const UnitStrategy &strategy,
	SimHostCallbacks *host,
	const CoordinatorTargetingState &state) {
	if (UnitState *kept = try_keep_current_enemy_target_early(world, unit, host, state)) {
		return kept;
	}

	TargetScoreContext score_ctx;
	score_ctx.attack_range = attack_range(unit);
	score_ctx.effective_range = effective_attack_range(unit);

	TargetingProfileCounters profile;
	profile.active = state.profile_targeting_active;
	if (profile.active) {
		profile.retarget_keeps = state.tgt_retarget_keeps;
		profile.enemy_scans = state.tgt_enemy_scans;
		profile.candidates_scored = state.tgt_candidates_scored;
		profile.candidates_prefix_pruned = state.tgt_candidates_prefix_pruned;
		profile.ties_adjusted = state.tgt_ties_adjusted;
		profile.ties_distance = state.tgt_ties_distance;
		profile.ties_instance = state.tgt_ties_instance;
	}

	ScoreEnemyProfileCounters score_profile;
	score_profile.active = state.profile_score_enemy;
	if (score_profile.active) {
		score_profile.se_base = state.profile_se_base;
		score_profile.se_calls = state.profile_se_calls;
	}

	TargetingDebugHooks debug;
	debug.enabled = state.debug_targeting_scoring;
	if (debug.enabled) {
		debug.user_data = state.debug_user_data;
		debug.archetype_for_unit = state.debug_archetype_for_unit;
		debug.print_line = state.debug_print_line;
		debug.print_score_breakdown = state.debug_print_score_breakdown;
	}

	return select_enemy_target(
			world,
			unit,
			strategy,
			score_ctx,
			host,
			profile.active ? &profile : nullptr,
			score_profile.active ? &score_profile : nullptr,
			debug.enabled ? &debug : nullptr);
}


} // namespace targeting
} // namespace sim
