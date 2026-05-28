#include "sim_targeting.hpp"

#include "sim_constants.hpp"
#include "sim_stats.hpp"
#include "sim_targeting_internal.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <vector>

namespace sim {
namespace targeting {

using namespace internal;

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
