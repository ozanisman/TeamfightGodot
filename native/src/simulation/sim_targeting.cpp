#include "sim_targeting.hpp"

#include "sim_constants.hpp"
#include "sim_stats.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace sim {
namespace targeting {

namespace {

inline const StringName &player_team_name() {
	static const StringName s("player");
	return s;
}

inline const StringName &enemy_team_name() {
	static const StringName s("enemy");
	return s;
}

inline const StringName &ally_filter_name() {
	static const StringName s("ally");
	return s;
}

double random_unit(SimHostCallbacks &host) {
	if (host.randf != nullptr) {
		return host.randf(host.user_data);
	}
	return 0.0;
}

double strategy_role_prio(const std::array<double, ROLE_SLOT_COUNT> &slots, int64_t role_slot) {
	if (role_slot < 0 || role_slot >= ROLE_SLOT_COUNT) {
		return 0.0;
	}
	return slots[static_cast<size_t>(role_slot)];
}

uint64_t profile_elapsed_ns(std::chrono::steady_clock::time_point t0) {
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now() - t0).count();
}

struct ProfileAccScope {
	bool enabled = false;
	uint64_t *accum = nullptr;
	std::chrono::steady_clock::time_point t0{};
	explicit ProfileAccScope(bool profile, uint64_t *out_accum)
			: enabled(profile && out_accum != nullptr), accum(out_accum) {
		if (enabled) {
			t0 = std::chrono::steady_clock::now();
		}
	}
	~ProfileAccScope() {
		if (enabled && accum != nullptr) {
			*accum += profile_elapsed_ns(t0);
		}
	}
	ProfileAccScope(const ProfileAccScope &) = delete;
	ProfileAccScope &operator=(const ProfileAccScope &) = delete;
};

void sync_frame_index(SimWorld &world, int64_t index, const UnitState &unit, const SimHostCallbacks *host) {
	if (host != nullptr && host->sync_targeting_frame_index != nullptr) {
		host->sync_targeting_frame_index(host->user_data, index, unit);
		return;
	}
	sync_targeting_frame_index(world, index, unit);
}

void sync_frame_unit(SimWorld &world, const UnitState &unit, const SimHostCallbacks *host) {
	if (host != nullptr && host->sync_targeting_frame_unit != nullptr) {
		host->sync_targeting_frame_unit(host->user_data, unit);
		return;
	}
	sync_targeting_frame_unit(world, unit);
}

} // namespace

UnitState *unit_by_id(SimWorld &world, int64_t instance_id) {
	auto it = world.unit_index_map.find(instance_id);
	if (it == world.unit_index_map.end()) {
		return nullptr;
	}
	int64_t index = it->second;
	if (index < 0 || index >= int64_t(world.units.size())) {
		return nullptr;
	}
	return &world.units[static_cast<size_t>(index)];
}

const UnitState *unit_by_id(const SimWorld &world, int64_t instance_id) {
	auto it = world.unit_index_map.find(instance_id);
	if (it == world.unit_index_map.end()) {
		return nullptr;
	}
	int64_t index = it->second;
	if (index < 0 || index >= int64_t(world.units.size())) {
		return nullptr;
	}
	return &world.units[static_cast<size_t>(index)];
}

int64_t unit_index_by_id(const SimWorld &world, int64_t instance_id) {
	auto it = world.unit_index_map.find(instance_id);
	if (it != world.unit_index_map.end()) {
		return it->second;
	}
	return -1;
}

int64_t target_index_for_unit(SimWorld &world, UnitState &unit) {
	if (unit.target_id == 0) {
		unit.target_index = -1;
		return -1;
	}
	if (unit.target_index >= 0 && unit.target_index < int64_t(world.units.size())) {
		const UnitState &target = world.units[static_cast<size_t>(unit.target_index)];
		if (target.instance_id == unit.target_id) {
			return unit.target_index;
		}
	}
	unit.target_index = unit_index_by_id(world, unit.target_id);
	return unit.target_index;
}

double attack_range(const UnitState &unit) {
	return get_effective_attack_range(unit);
}

double effective_attack_range(const UnitState &unit) {
	double range = attack_range(unit);
	if (range <= RANGED_THRESHOLD) {
		return range + MELEE_CONTACT_BUFFER;
	}
	return range + RANGED_CONTACT_BUFFER;
}

TargetingFrameEntry make_targeting_frame_entry(const UnitState &unit) {
	TargetingFrameEntry frame;
	frame.instance_id = unit.instance_id;
	frame.is_player_team = unit.team == player_team_name();
	frame.role_slot = unit.role_slot;
	frame.is_tank_role = unit.is_tank_role;
	frame.is_fighter_role = unit.is_fighter_role;
	frame.is_assassin_role = unit.is_assassin_role;
	frame.is_marksman_role = unit.is_marksman_role;
	frame.is_mage_role = unit.is_mage_role;
	frame.is_support_role = unit.is_support_role;
	frame.pos_x = unit.pos_x;
	frame.pos_y = unit.pos_y;
	frame.hp = unit.hp;
	frame.max_hp = get_effective_max_hp(unit);
	frame.alive = unit.alive;
	frame.target_id = unit.target_id;
	frame.incoming_target_count = unit.incoming_target_count;
	frame.perceived_threat = unit.perceived_threat;
	frame.stealth_remaining = unit.stealth_remaining;
	return frame;
}

void sync_targeting_frame_index(SimWorld &world, int64_t index, const UnitState &unit) {
	if (index < 0 || index >= int64_t(world.targeting_frame.size())) {
		return;
	}
	TargetingFrameEntry &frame = world.targeting_frame[static_cast<size_t>(index)];
	frame.pos_x = unit.pos_x;
	frame.pos_y = unit.pos_y;
	frame.hp = unit.hp;
	frame.max_hp = get_effective_max_hp(unit);
	frame.alive = unit.alive;
	frame.target_id = unit.target_id;
	frame.incoming_target_count = unit.incoming_target_count;
	frame.perceived_threat = unit.perceived_threat;
	frame.stealth_remaining = unit.stealth_remaining;
}

void sync_targeting_frame_unit(SimWorld &world, const UnitState &unit) {
	sync_targeting_frame_index(world, unit_index_by_id(world, unit.instance_id), unit);
}

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
				double ally_hp_ratio = targeted_ally->hp / Math::max(0.0001, get_effective_max_hp(*targeted_ally));
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
			breakdown->hp_weighted = hp_ratio * strategy.hp_weight * SCORE_HP_WEIGHT_SCALE;
			breakdown->role_priority = role_prio;
			breakdown->threat = enemy.perceived_threat;
			breakdown->threat_weighted = threat_response;
			breakdown->in_range_bonus = in_range_bonus;
			breakdown->execute_bonus = execute_bonus;
			breakdown->support_peel = support_peel;
		}
	}

	{
		ProfileAccScope _se_base2(profile_score, se_base_accum);
		double spacing_weight = strategy.spacing_weight;
		double spacing_score = 0.0;
		if (spacing_weight > 0.0) {
			spacing_score = Math::pow(double(enemy.incoming_target_count), SPACING_EXPONENT) * spacing_weight;
			score += spacing_score;
		}

		double kiting_tempo_score = 0.0;
		if (strategy.prefers_kiting && score_ctx.has_kite_bounds) {
			double min_w = score_ctx.kite_min_w;
			double max_w = score_ctx.kite_max_w;
			if (dist >= min_w && dist <= max_w && max_w > min_w) {
				double kite_ratio = (dist - min_w) / (max_w - min_w);
				kiting_tempo_score = kite_ratio * SCORE_KITING_WEIGHT_SCALE;
				score -= kiting_tempo_score;
			}
		}

		if (breakdown) {
			breakdown->spacing = spacing_score;
			breakdown->kiting_tempo = kiting_tempo_score;
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
			double attack_speed = Math::max(0.0001, get_effective_attack_speed(unit));
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
}

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

	std::vector<std::pair<int64_t, double>> candidate_scores;

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
		const bool debug_scoring = debug != nullptr && debug->enabled;
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

		candidate_scores.push_back({candidate.instance_id, raw});

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

	if (debug != nullptr && debug->enabled && debug->print_line != nullptr && debug->archetype_for_unit != nullptr) {
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

UnitState *select_ally_target(SimWorld &world, UnitState &unit, const UnitStrategy &strategy, const TargetingProfileCounters *profile) {
	const bool profile_active = profile != nullptr && profile->active;

	bool allow_ally = strategy.uses_ally_targeting;
	if (!allow_ally) {
		allow_ally = unit.is_support_role || unit.is_tank_role;
	}
	if (!allow_ally) {
		unit.current_ally_target_id = 0;
		return nullptr;
	}

	const std::vector<int64_t> &ally_indices = alive_indices_for_team(world, unit.team);
	if (ally_indices.empty()) {
		unit.current_ally_target_id = 0;
		return nullptr;
	}
	if (profile_active && profile->ally_scans != nullptr) {
		*profile->ally_scans += 1;
	}

	std::vector<int64_t> critical_allies;
	for (int64_t ally_index : ally_indices) {
		const TargetingFrameEntry &candidate = world.targeting_frame[static_cast<size_t>(ally_index)];
		double hp_ratio = candidate.hp / Math::max(0.0001, candidate.max_hp);
		if (hp_ratio <= ALLY_CRITICAL_HP_RATIO) {
			critical_allies.push_back(ally_index);
		}
	}
	const std::vector<int64_t> &pool = critical_allies.empty() ? ally_indices : critical_allies;
	UnitState *best = nullptr;
	double best_score = std::numeric_limits<double>::infinity();
	double best_dist = std::numeric_limits<double>::infinity();
	double best_hp_ratio = std::numeric_limits<double>::infinity();
	for (int64_t ally_index : pool) {
		const TargetingFrameEntry &candidate = world.targeting_frame[static_cast<size_t>(ally_index)];
		double dist = distance_between_coords(unit.pos_x, unit.pos_y, candidate.pos_x, candidate.pos_y);
		double score = score_ally_target(unit, candidate, strategy, dist);
		double hp_ratio = candidate.hp / Math::max(0.0001, candidate.max_hp);
		if (critical_allies.empty()) {
			bool is_better = best == nullptr;
			if (!is_better) {
				is_better = score < best_score
						|| (score == best_score && dist < best_dist)
						|| (score == best_score && dist == best_dist && candidate.instance_id < best->instance_id);
			}
			if (is_better) {
				best = &world.units[static_cast<size_t>(ally_index)];
				best_score = score;
				best_dist = dist;
				best_hp_ratio = hp_ratio;
			}
			continue;
		}
		bool is_better = best == nullptr;
		if (!is_better) {
			is_better = hp_ratio < best_hp_ratio
					|| (hp_ratio == best_hp_ratio && score < best_score)
					|| (hp_ratio == best_hp_ratio && score == best_score && dist < best_dist)
					|| (hp_ratio == best_hp_ratio && score == best_score && dist == best_dist && candidate.instance_id < best->instance_id);
		}
		if (is_better) {
			best = &world.units[static_cast<size_t>(ally_index)];
			best_score = score;
			best_dist = dist;
			best_hp_ratio = hp_ratio;
		}
	}
	unit.current_ally_target_id = best == nullptr ? 0 : best->instance_id;
	return best;
}

void prepare_tick_context(SimWorld &world, const SimHostCallbacks &host) {
	const size_t n = world.units.size();
	if (world.tick_ctx.density_by_unit_index.size() != n) {
		world.tick_ctx.density_by_unit_index.assign(n, 0);
	}
	world.tick_ctx.player_backliner_indices.clear();
	world.tick_ctx.enemy_backliner_indices.clear();
	world.tick_ctx.player_frontline_indices.clear();
	world.tick_ctx.enemy_frontline_indices.clear();

	double pcx = 0.0;
	double pcy = 0.0;
	int pc = 0;
	for (int64_t idx : world.alive_player_indices) {
		UnitState &u = world.units[static_cast<size_t>(idx)];
		if (!u.alive) {
			continue;
		}
		sync_targeting_frame_index(world, idx, u);
		pcx += u.pos_x;
		pcy += u.pos_y;
		pc += 1;
		if (u.is_marksman_role || u.is_mage_role || u.is_support_role) {
			world.tick_ctx.player_backliner_indices.push_back(idx);
			u.is_backliner = true;
		} else {
			u.is_backliner = false;
		}
		if (u.is_tank_role || u.is_fighter_role) {
			world.tick_ctx.player_frontline_indices.push_back(idx);
		}
	}
	world.tick_ctx.has_player_center = pc > 0;
	if (pc > 0) {
		world.tick_ctx.player_team_center = Vector2(pcx / double(pc), pcy / double(pc));
	}

	double ecx = 0.0;
	double ecy = 0.0;
	int ec = 0;
	for (int64_t idx : world.alive_enemy_indices) {
		UnitState &u = world.units[static_cast<size_t>(idx)];
		if (!u.alive) {
			continue;
		}
		sync_targeting_frame_index(world, idx, u);
		ecx += u.pos_x;
		ecy += u.pos_y;
		ec += 1;
		if (u.is_marksman_role || u.is_mage_role || u.is_support_role) {
			world.tick_ctx.enemy_backliner_indices.push_back(idx);
			u.is_backliner = true;
		} else {
			u.is_backliner = false;
		}
		if (u.is_tank_role || u.is_fighter_role) {
			world.tick_ctx.enemy_frontline_indices.push_back(idx);
		}
	}
	world.tick_ctx.has_enemy_center = ec > 0;
	if (ec > 0) {
		world.tick_ctx.enemy_team_center = Vector2(ecx / double(ec), ecy / double(ec));
	}
	world.tick_ctx.player_backliner_alive_count = int(world.tick_ctx.player_backliner_indices.size());
	world.tick_ctx.enemy_backliner_alive_count = int(world.tick_ctx.enemy_backliner_indices.size());

	for (int64_t idx : world.alive_player_indices) {
		const UnitState &u = world.units[idx];
		if (!u.alive || u.target_id == 0) {
			continue;
		}
		UnitState &live_unit = world.units[static_cast<size_t>(idx)];
		int64_t target_index = target_index_for_unit(world, live_unit);
		if (target_index < 0) {
			continue;
		}
		UnitState &target = world.units[static_cast<size_t>(target_index)];
		if (!target.alive) {
			continue;
		}
		target.incoming_target_count += 1;
		sync_targeting_frame_index(world, target_index, target);
	}
	for (int64_t idx : world.alive_enemy_indices) {
		const UnitState &u = world.units[idx];
		if (!u.alive || u.target_id == 0) {
			continue;
		}
		UnitState &live_unit = world.units[static_cast<size_t>(idx)];
		int64_t target_index = target_index_for_unit(world, live_unit);
		if (target_index < 0) {
			continue;
		}
		UnitState &target = world.units[static_cast<size_t>(target_index)];
		if (!target.alive) {
			continue;
		}
		target.incoming_target_count += 1;
		sync_targeting_frame_index(world, target_index, target);
	}
	(void)host;
}

std::vector<UnitState *> select_targets(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		int64_t target_count,
		TargetSelectionStrategy strategy,
		bool include_source,
		ExcessTargetHandling excess_handling,
		const StringName &team_filter) {
	std::vector<UnitState *> selected;

	StringName target_team = team_filter;
	if (target_team.is_empty()) {
		target_team = (source.team == player_team_name()) ? enemy_team_name() : player_team_name();
	} else if (target_team == enemy_team_name()) {
		target_team = (source.team == player_team_name()) ? enemy_team_name() : player_team_name();
	} else if (target_team == ally_filter_name()) {
		target_team = source.team;
	} else {
		UtilityFunctions::push_error(vformat("Invalid team_filter '%s', must be 'ally' or 'enemy'", String(target_team)));
		return selected;
	}

	const std::vector<int64_t> &alive_indices =
			(target_team == player_team_name()) ? world.alive_player_indices : world.alive_enemy_indices;

	if (alive_indices.empty()) {
		return selected;
	}

	std::vector<UnitState *> candidates;
	candidates.reserve(alive_indices.size());
	for (int64_t idx : alive_indices) {
		if (idx < 0 || idx >= int64_t(world.units.size())) {
			continue;
		}
		UnitState *unit = &world.units[static_cast<size_t>(idx)];
		if (!unit->alive) {
			continue;
		}
		if (!include_source && unit->instance_id == source.instance_id) {
			continue;
		}
		candidates.push_back(unit);
	}

	if (candidates.empty()) {
		return selected;
	}

	switch (strategy) {
		case TARGET_SELECTION_CLOSEST: {
			std::sort(candidates.begin(), candidates.end(), [&](UnitState *a, UnitState *b) {
				return distance_between(source, *a) < distance_between(source, *b);
			});
			break;
		}
		case TARGET_SELECTION_RANDOM: {
			for (size_t i = candidates.size(); i > 1; --i) {
				const double rand_val = random_unit(host);
				const size_t j = static_cast<size_t>(rand_val * static_cast<double>(i));
				std::swap(candidates[i - 1], candidates[j]);
			}
			break;
		}
		case TARGET_SELECTION_LOWEST_HP: {
			std::sort(candidates.begin(), candidates.end(), [](UnitState *a, UnitState *b) {
				return a->hp < b->hp;
			});
			break;
		}
		case TARGET_SELECTION_HIGHEST_HP: {
			std::sort(candidates.begin(), candidates.end(), [](UnitState *a, UnitState *b) {
				return a->hp > b->hp;
			});
			break;
		}
		case TARGET_SELECTION_LOWEST_PERCENT_HP: {
			std::sort(candidates.begin(), candidates.end(), [](UnitState *a, UnitState *b) {
				const double max_a = get_effective_max_hp(*a);
				const double max_b = get_effective_max_hp(*b);
				const double pct_a = (max_a > EPSILON) ? (a->hp / max_a) : 0.0;
				const double pct_b = (max_b > EPSILON) ? (b->hp / max_b) : 0.0;
				return pct_a < pct_b;
			});
			break;
		}
		case TARGET_SELECTION_HIGHEST_PERCENT_HP: {
			std::sort(candidates.begin(), candidates.end(), [](UnitState *a, UnitState *b) {
				const double max_a = get_effective_max_hp(*a);
				const double max_b = get_effective_max_hp(*b);
				const double pct_a = (max_a > EPSILON) ? (a->hp / max_a) : 0.0;
				const double pct_b = (max_b > EPSILON) ? (b->hp / max_b) : 0.0;
				return pct_a > pct_b;
			});
			break;
		}
		case TARGET_SELECTION_CLOSEST_TO_TARGET: {
			if (target != nullptr) {
				std::sort(candidates.begin(), candidates.end(), [&](UnitState *a, UnitState *b) {
					return distance_between(*target, *a) < distance_between(*target, *b);
				});
			} else {
				std::sort(candidates.begin(), candidates.end(), [&](UnitState *a, UnitState *b) {
					return distance_between(source, *a) < distance_between(source, *b);
				});
			}
			break;
		}
	}

	int64_t num_to_select = target_count;
	if (target_count == -1) {
		num_to_select = int64_t(candidates.size());
	} else if (target_count > int64_t(candidates.size())) {
		if (excess_handling == EXCESS_TARGET_DROP) {
			num_to_select = int64_t(candidates.size());
		} else {
			for (int64_t i = 0; i < target_count; ++i) {
				selected.push_back(candidates[static_cast<size_t>(i % candidates.size())]);
			}
			return selected;
		}
	}

	for (int64_t i = 0; i < num_to_select && i < int64_t(candidates.size()); ++i) {
		selected.push_back(candidates[static_cast<size_t>(i)]);
	}

	return selected;
}

} // namespace targeting
} // namespace sim
