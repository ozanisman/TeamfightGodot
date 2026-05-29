#ifndef SIM_TARGETING_STRATEGIES_HPP
#define SIM_TARGETING_STRATEGIES_HPP

#include "simulation_types.hpp"

#include <array>

namespace sim {
namespace targeting {

int64_t role_slot_for_name(const StringName &role);

void build_role_strategy_cache(std::array<UnitStrategy, ROLE_SLOT_COUNT> &cache_by_slot, UnitStrategy &default_strategy);

const UnitStrategy &strategy_for_unit(
		const UnitState &unit,
		const std::array<UnitStrategy, ROLE_SLOT_COUNT> &cache_by_slot,
		const UnitStrategy &default_strategy);

} // namespace targeting
} // namespace sim

#endif // SIM_TARGETING_STRATEGIES_HPP
