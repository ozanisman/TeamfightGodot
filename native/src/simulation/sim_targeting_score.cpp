#include "sim_targeting.hpp"

#include "sim_constants.hpp"
#include "sim_stats.hpp"
#include "sim_targeting_internal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sim {
namespace targeting {

using namespace internal;

double score_ally_target(
		const UnitState &unit,
		const TargetingFrameEntry &ally,
		const UnitStrategy &strategy,
		double unit_ally_distance) {
	double dist = unit_ally_distance;
	if (dist < 0.0) {
		dist = distance_between_coords(unit.pos_x, unit.pos_y, ally.pos_x, ally.pos_y);
	}
	double hp_ratio = ally.hp / Math::max(0.0001, ally.max_hp);
	double score = dist * strategy.ally_distance_weight;
	score += hp_ratio * strategy.ally_hp_weight * SCORE_HP_WEIGHT_SCALE;
	score -= ally.perceived_threat * strategy.ally_threat_weight;
	score += strategy_role_prio(strategy.ally_role_priorities, ally.role_slot);
	return score;
}

double score_enemy_target(
		SimWorld &world,
		const UnitState &attacker,
		const TargetingFrameEntry &enemy,
		const TargetingFrameEntry *ally_for_peel,
		const UnitStrategy &strategy,
		const TickContext &ctx,
		const TargetScoreContext &score_ctx,
		double attacker_enemy_distance,
		const ScoreEnemyProfileCounters *profile,
		int64_t enemy_index,
		ScoreBreakdown *breakdown) {
	const bool profile_score = profile != nullptr && profile->active;
	if (profile_score && profile->se_calls != nullptr) {
		*profile->se_calls += 1;
	}
	uint64_t *se_base_accum = profile_score ? profile->se_base : nullptr;

	double score = 0.0;
	double dist = 0.0;
	double attack_range_val = score_ctx.attack_range;
	double effective_range = score_ctx.effective_range;
	{
		ProfileAccScope _se_base(profile_score, se_base_accum);
		if (attacker_enemy_distance >= 0.0) {
			dist = attacker_enemy_distance;
		} else {
			dist = distance_between_coords(attacker.pos_x, attacker.pos_y, enemy.pos_x, enemy.pos_y);
		}
		bool in_range = false;
		if (attack_range_val > RANGED_THRESHOLD) {
			in_range = dist <= attack_range_val;
		} else {
			in_range = (dist <= effective_range);
		}
		double hp_ratio = enemy.hp / Math::max(0.0001, enemy.max_hp);
		double range_gap = Math::max(0.0, dist - Math::max(effective_range, EPSILON));
		double norm_gap = range_gap / Math::max(effective_range, EPSILON);
		norm_gap = std::round(norm_gap * 1048576.0) / 1048576.0;
		double distance_score = Math::pow(norm_gap, DISTANCE_EXPONENT) * strategy.distance_weight * SCORE_DISTANCE_WEIGHT_SCALE;
		score += distance_score;
		double in_range_bonus = in_range ? strategy.in_range_bonus : 0.0;
		score -= in_range_bonus;
		double hp_weighted = hp_ratio * strategy.hp_weight * SCORE_HP_WEIGHT_SCALE;
		score += hp_weighted;
		double execute_bonus = 0.0;
		if (strategy.execute_bonus_weight > 0.0 && in_range) {
			if (attacker.is_assassin_role) {
				double missing_hp_factor = 1.0 - hp_ratio;
				execute_bonus = strategy.execute_bonus_weight * missing_hp_factor;
				score -= execute_bonus;
			} else if (hp_ratio <= TARGET_EXECUTE_HP_RATIO) {
				execute_bonus = strategy.execute_bonus_weight;
				score -= execute_bonus;
			}
		}
		if ((attacker.is_tank_role || attacker.is_support_role) && enemy.target_id != 0) {
			UnitState *targeted_ally = unit_by_id(world, enemy.target_id);
			if (targeted_ally != nullptr && targeted_ally->alive && targeted_ally->team == attacker.team) {
				double ally_hp_ratio = targeted_ally->hp / Math::max(0.0001, targeted_ally->stats_dirty ? get_effective_max_hp(*targeted_ally) : targeted_ally->cached_max_hp);
				double ally_missing_hp_factor = 1.0 - ally_hp_ratio;
				score -= REACTIVE_PEEL_BONUS * ally_missing_hp_factor;
			}
		}
		double role_prio = strategy_role_prio(strategy.role_priorities, enemy.role_slot);
		score += role_prio;
		if (enemy.is_tank_role && attacker.is_assassin_role) {
			int64_t enemy_self_idx = enemy_index >= 0 ? enemy_index : unit_index_by_id(world, enemy.instance_id);
			int n_alive = enemy.is_player_team ? ctx.player_backliner_alive_count : ctx.enemy_backliner_alive_count;
			const UnitState &enemy_unit = world.units[static_cast<size_t>(enemy_self_idx)];
			bool self_in_bl = enemy_unit.is_backliner;
			int subtract = (self_in_bl && enemy.alive) ? 1 : 0;
			if (n_alive - subtract > 0) {
				score += ASSASSIN_TANK_CONTEXT_PENALTY;
			}
		}
		double threat_response = 0.0;
		if (enemy.target_id == attacker.instance_id) {
			double falloff = 1.0 / (1.0 + Math::max(0.0, dist - attack_range_val) * THREAT_RESPONSE_RANGE_FALLOFF);
			threat_response = strategy.threat_response_weight * falloff;
			score -= threat_response;
		}
		double perceived_threat_weighted = 0.0;
		if (strategy.enemy_threat_weight > 0.0 && enemy.perceived_threat > 0.0) {
			perceived_threat_weighted = enemy.perceived_threat * strategy.enemy_threat_weight * SCORE_THREAT_WEIGHT_SCALE * PREY_PERCEIVED_THREAT_SCALE;
			score -= perceived_threat_weighted;
		}
		double support_peel = 0.0;
		if (attacker.is_support_role) {
			int64_t ally_target_id = attacker.current_ally_target_id;
			if (ally_target_id != 0 && ally_for_peel != nullptr && ally_for_peel->alive) {
				double ally_incoming = double(ally_for_peel->incoming_target_count);
				double ally_threat = ally_for_peel->perceived_threat;
				if ((ally_incoming >= SUPPORT_PEEL_THREAT_THRESHOLD || ally_threat >= SUPPORT_PEEL_THREAT_THRESHOLD) && enemy.target_id == ally_target_id) {
					support_peel = SUPPORT_PEEL_BOOST;
					score -= support_peel;
				}
			}
		}

		if (breakdown) {
			breakdown->distance = dist;
			breakdown->distance_weighted = distance_score;
			breakdown->hp_ratio = hp_ratio;
			breakdown->hp_weighted = hp_weighted;
			breakdown->role_priority = role_prio;
			breakdown->threat = enemy.perceived_threat;
			breakdown->threat_response_weighted = threat_response;
			breakdown->perceived_threat_weighted = perceived_threat_weighted;
			breakdown->in_range_bonus = in_range_bonus;
			breakdown->execute_bonus = execute_bonus;
			breakdown->support_peel = support_peel;
		}
	}

	{
		ProfileAccScope _se_base2(profile_score, se_base_accum);
		// TODO: spacing penalizes crowded targets (spread fire). Disabled via spacing_weight=0;
		// reconsider invert (focus-fire bonus) or role-specific tuning.
		double spacing_weight = strategy.spacing_weight;
		double spacing_score = 0.0;
		if (spacing_weight > 0.0) {
			spacing_score = Math::pow(double(enemy.incoming_target_count), SPACING_EXPONENT) * spacing_weight;
			score += spacing_score;
		}

		if (breakdown) {
			breakdown->spacing = spacing_score;
		}
	}

	return score;
}

bool should_switch(
		SimWorld &world,
		const UnitState &unit,
		double current_score,
		double new_score,
		const UnitStrategy &strategy,
		double current_target_distance) {
	if (unit.target_switch_lock_timer > 0.0) {
		return false;
	}
	const UnitState *current_target = unit_by_id(world, unit.target_id);
	if (current_target != nullptr && current_target->alive && current_target->team != unit.team) {
		double dist = current_target_distance >= 0.0 ? current_target_distance : distance_between(unit, *current_target);
		double attack_range_val = attack_range(unit);
		double effective_range_val = effective_attack_range(unit);
		bool in_range = false;
		if (attack_range_val > RANGED_THRESHOLD) {
			in_range = dist <= attack_range_val;
		} else {
			in_range = (dist <= effective_range_val);
		}
		if (in_range) {
			double attack_speed = Math::max(0.0001, unit.stats_dirty ? get_effective_attack_speed(unit) : unit.cached_attack_speed);
			double swing = 1.0 / attack_speed;
			double commit_window = Math::min(SWITCH_COMMIT_WINDOW_SECONDS, swing * SWITCH_COMMIT_WINDOW_SWING_FRACTION);
			if (unit.attack_cooldown <= commit_window) {
				return false;
			}
		}
	}
	double margin = strategy.switch_margin;
	return new_score <= (current_score - margin);
}

void adjust_target_pressure(SimWorld &world, int64_t old_target_id, int64_t new_target_id, const SimHostCallbacks *host) {
	if (old_target_id == new_target_id) {
		return;
	}
	if (old_target_id != 0) {
		int64_t old_index = unit_index_by_id(world, old_target_id);
		if (old_index >= 0) {
			UnitState &old_unit = world.units[static_cast<size_t>(old_index)];
			old_unit.incoming_target_count = std::max<int64_t>(0, old_unit.incoming_target_count - 1);
			sync_frame_index(world, old_index, old_unit, host);
		}
	}
	if (new_target_id != 0) {
		int64_t new_index = unit_index_by_id(world, new_target_id);
		if (new_index >= 0) {
			UnitState &new_unit = world.units[static_cast<size_t>(new_index)];
			new_unit.incoming_target_count += 1;
			sync_frame_index(world, new_index, new_unit, host);
		}
	}
}

void set_current_target(SimWorld &world, UnitState &unit, const UnitState &target, const SimHostCallbacks *host) {
	int64_t old_target_id = unit.target_id;
	int64_t new_target_id = target.instance_id;
	if (old_target_id == new_target_id) {
		return;
	}
	adjust_target_pressure(world, old_target_id, new_target_id, host);
	if (host != nullptr && host->emit_trace != nullptr) {
		host->emit_trace(host->user_data, StringName("target_switch"), unit.instance_id, new_target_id, double(old_target_id));
	}
	unit.target_id = new_target_id;
	unit.target_index = unit_index_by_id(world, new_target_id);
	sync_frame_unit(world, unit, host);
	if (target.team != unit.team) {
		UnitState *acquired_target = unit_by_id(world, new_target_id);
		if (acquired_target != nullptr) {
			request_immediate_retarget_eval(*acquired_target, true);
		}
	}
}

} // namespace targeting
} // namespace sim
