#include "sim_stats.hpp"

#include "sim_constants.hpp"
#include "sim_unit_tick_internal.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim {

double movement_speed_multiplier(const SimWorld &world, const UnitState &unit) {
	const UnitStateCold &cold = uc(world, unit);
	if (cold.slow_buffs.empty()) {
		return 1.0;
	}
	double max_slow_percentage = 0.0;
	for (const UnitStateCold::SlowBuff &buff : cold.slow_buffs) {
		max_slow_percentage = Math::max(max_slow_percentage, buff.slow_percentage);
	}
	const double mult = Math::clamp(1.0 - max_slow_percentage, SLOW_MOVEMENT_MULTIPLIER_MIN, 1.0);
	return mult;
}

} // namespace sim
