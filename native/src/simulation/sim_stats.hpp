#ifndef SIM_STATS_HPP
#define SIM_STATS_HPP

#include "simulation_types.hpp"
#include "sim_stats.inl.hpp" // IWYU pragma: export

namespace sim {

inline double distance_between_coords(double x1, double y1, double x2, double y2) {
	return Math::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

inline double distance_between(const UnitState &left, const UnitState &right) {
	return distance_between_coords(left.pos_x, left.pos_y, right.pos_x, right.pos_y);
}

double movement_speed_multiplier(const UnitState &unit);

} // namespace sim

#endif // SIM_STATS_HPP
