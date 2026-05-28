#ifndef SIM_MATCH_SPAWN_HPP
#define SIM_MATCH_SPAWN_HPP

#include "sim_world.hpp"

#include <godot_cpp/variant/vector2.hpp>

#include <vector>

namespace sim {
namespace match {

Vector2 find_random_spawn_position_near_excluding(
		const SimWorld &world,
		SimHostCallbacks &host,
		double center_x,
		double center_y,
		double radius,
		int64_t exclude_instance_id);

Vector2 find_random_spawn_position_near_excluding_with_expansion(
		const SimWorld &world,
		SimHostCallbacks &host,
		double center_x,
		double center_y,
		double initial_radius,
		double max_radius,
		int64_t exclude_instance_id,
		const std::vector<Vector2> &pending_positions);

} // namespace match
} // namespace sim

#endif // SIM_MATCH_SPAWN_HPP
