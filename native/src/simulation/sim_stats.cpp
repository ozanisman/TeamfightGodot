#include "sim_stats.hpp"

#include "sim_constants.hpp"

namespace sim {

double movement_speed_multiplier(const UnitState &unit) {
	return Math::clamp(unit.slow_move_mult, SLOW_MOVEMENT_MULTIPLIER_MIN, 1.0);
}

} // namespace sim
