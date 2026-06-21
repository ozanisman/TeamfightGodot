#ifndef SIM_MOVEMENT_HPP
#define SIM_MOVEMENT_HPP

#include "sim_world.hpp"

#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <cstdint>

namespace sim {
namespace movement {

struct KiteProfileCounters {
	bool active = false;
	uint64_t *kiting_spatial = nullptr;
	uint64_t *kiting_brute = nullptr;
};

bool position_collides_with_unit(const SimWorld &world, double x, double y, int64_t exclude_instance_id);
Vector2 find_valid_dash_position(
		const SimWorld &world,
		double start_x,
		double start_y,
		double target_x,
		double target_y,
		double max_distance,
		int64_t exclude_instance_id);

void move_toward_target(SimWorld &world, UnitState &unit, const UnitState &target);
void move_toward_target_with_range(SimWorld &world, UnitState &unit, const UnitState &target, double target_range);
void move_toward_target_with_ally_leash(
		SimWorld &world,
		UnitState &unit,
		const UnitState &enemy,
		double enemy_stop_range,
		const UnitState &ally,
		double max_ally_dist);
bool kite_from_enemies(SimWorld &world, SimHostCallbacks &host, UnitState &unit, const KiteProfileCounters *profile = nullptr);

} // namespace movement
} // namespace sim

#endif // SIM_MOVEMENT_HPP
