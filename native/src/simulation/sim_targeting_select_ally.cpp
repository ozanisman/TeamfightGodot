#include "sim_targeting.hpp"

#include "sim_constants.hpp"
#include "sim_stats.hpp"
#include "sim_targeting_internal.hpp"
#include <limits>
#include <vector>

namespace sim {
namespace targeting {

using namespace internal;

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
UnitState *select_ally_target_coordinator(
		SimWorld &world,
		UnitState &unit,
		const UnitStrategy &strategy,
		const CoordinatorTargetingState &state) {
	TargetingProfileCounters profile;
	profile.active = state.profile_targeting_active;
	if (profile.active) {
		profile.ally_scans = state.tgt_ally_scans;
	}
	return select_ally_target(world, unit, strategy, profile.active ? &profile : nullptr);
}

} // namespace targeting
} // namespace sim
