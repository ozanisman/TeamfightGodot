#ifndef SIM_EFFECTS_HOST_HPP
#define SIM_EFFECTS_HOST_HPP

#include "sim_world.hpp"

#include <vector>

namespace sim {
namespace effects {

/// Coordinator-owned match state accessed by spawn/stat-stack effect opcodes.
struct SimMatchHost {
	void *user_data = nullptr;
	std::vector<PendingSpawn> *pending_spawns = nullptr;
	int64_t *max_instance_id = nullptr;
	Dictionary (*get_minion_data)(void *user_data, const StringName &minion_id) = nullptr;
	Vector2 (*find_random_spawn_position_near_excluding_with_expansion)(
			void *user_data,
			double center_x,
			double center_y,
			double initial_radius,
			double max_radius,
			int64_t exclude_instance_id,
			const std::vector<Vector2> &pending_positions) = nullptr;
};

} // namespace effects
} // namespace sim

#endif // SIM_EFFECTS_HOST_HPP
