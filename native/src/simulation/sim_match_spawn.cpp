#include "sim_match_spawn.hpp"

#include "sim_constants.hpp"
#include "sim_movement.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim {
namespace match {
namespace {

double random_unit(SimHostCallbacks &host) {
	if (host.randf != nullptr) {
		return host.randf(host.user_data);
	}
	return 0.0;
}

bool collides_with_pending(double x, double y, const std::vector<Vector2> &pending_positions) {
	const double collision_radius_sq = UNIT_COLLISION_RADIUS * UNIT_COLLISION_RADIUS * 4.0;
	for (const Vector2 &pending_pos : pending_positions) {
		const double dx = x - pending_pos.x;
		const double dy = y - pending_pos.y;
		if (dx * dx + dy * dy < collision_radius_sq) {
			return true;
		}
	}
	return false;
}

} // namespace

Vector2 find_random_spawn_position_near_excluding(
		const SimWorld &world,
		SimHostCallbacks &host,
		double center_x,
		double center_y,
		double radius,
		int64_t exclude_instance_id) {
	constexpr int max_attempts = 50;
	constexpr double pi = 3.14159265358979323846;

	for (int attempt = 0; attempt < max_attempts; ++attempt) {
		const double angle = random_unit(host) * pi * 2.0;
		const double distance = random_unit(host) * radius;

		double test_x = center_x + Math::cos(angle) * distance;
		double test_y = center_y + Math::sin(angle) * distance;

		test_x = Math::clamp(test_x, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
		test_y = Math::clamp(test_y, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);

		if (!movement::position_collides_with_unit(world, test_x, test_y, exclude_instance_id)) {
			return Vector2(test_x, test_y);
		}
	}

	return Vector2(-1.0, -1.0);
}

Vector2 find_random_spawn_position_near_excluding_with_expansion(
		const SimWorld &world,
		SimHostCallbacks &host,
		double center_x,
		double center_y,
		double initial_radius,
		double max_radius,
		int64_t exclude_instance_id,
		const std::vector<Vector2> &pending_positions) {
	Vector2 result = find_random_spawn_position_near_excluding(
			world, host, center_x, center_y, initial_radius, exclude_instance_id);
	if (result.x >= 0.0 && !collides_with_pending(result.x, result.y, pending_positions)) {
		return result;
	}

	constexpr int expansion_steps = 5;
	constexpr double pi = 3.14159265358979323846;
	constexpr int attempts_per_step = 30;

	for (int step = 0; step < expansion_steps; ++step) {
		const double expansion_factor =
				1.0 + (double(step + 1) / double(expansion_steps)) * ((max_radius / initial_radius) - 1.0);
		const double current_radius = initial_radius * expansion_factor;

		for (int attempt = 0; attempt < attempts_per_step; ++attempt) {
			const double angle = random_unit(host) * pi * 2.0;
			const double distance = random_unit(host) * current_radius;

			double test_x = center_x + Math::cos(angle) * distance;
			double test_y = center_y + Math::sin(angle) * distance;

			test_x = Math::clamp(test_x, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			test_y = Math::clamp(test_y, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);

			if (!movement::position_collides_with_unit(world, test_x, test_y, exclude_instance_id) &&
					!collides_with_pending(test_x, test_y, pending_positions)) {
				return Vector2(test_x, test_y);
			}
		}
	}

	UtilityFunctions::push_error(vformat(
			"Spawn position failed completely: could not find valid position within max radius %.2f near (%.2f, %.2f). Active units: %d",
			max_radius,
			center_x,
			center_y,
			world.units.size()));
	return Vector2(-1.0, -1.0);
}

} // namespace match
} // namespace sim
