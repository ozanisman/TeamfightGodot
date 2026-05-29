#include "sim_movement.hpp"

#include "sim_constants.hpp"
#include "sim_spatial.hpp"
#include "sim_stats.hpp"
#include "sim_targeting.hpp"

#include <godot_cpp/core/math.hpp>

#include <chrono>

namespace sim {
namespace movement {
namespace {

inline const StringName &sn_player() {
	static const StringName s("player");
	return s;
}
inline const StringName &sn_enemy() {
	static const StringName s("enemy");
	return s;
}

uint64_t profile_elapsed_ns(std::chrono::steady_clock::time_point t0) {
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now() - t0).count();
}

struct ProfileAccScope {
	bool enabled = false;
	uint64_t *accum = nullptr;
	std::chrono::steady_clock::time_point t0{};
	explicit ProfileAccScope(bool profile, uint64_t *out_accum) : enabled(profile && out_accum != nullptr), accum(out_accum) {
		if (enabled) {
			t0 = std::chrono::steady_clock::now();
		}
	}
	~ProfileAccScope() {
		if (enabled && accum != nullptr) {
			*accum += profile_elapsed_ns(t0);
		}
	}
	ProfileAccScope(const ProfileAccScope &) = delete;
	ProfileAccScope &operator=(const ProfileAccScope &) = delete;
};

void sync_frame_unit(SimWorld &world, const UnitState &unit, const SimHostCallbacks *host) {
	if (host != nullptr && host->sync_targeting_frame_unit != nullptr) {
		host->sync_targeting_frame_unit(host->user_data, unit);
		return;
	}
	targeting::sync_targeting_frame_unit(world, unit);
}

} // namespace

bool position_collides_with_unit(const SimWorld &world, double x, double y, int64_t exclude_instance_id) {
	const double collision_radius_sq = UNIT_COLLISION_RADIUS * UNIT_COLLISION_RADIUS * 4.0;
	for (const UnitState &unit : world.units) {
		if (unit.instance_id == exclude_instance_id) {
			continue;
		}
		if (!unit.alive) {
			continue;
		}
		const double dx = x - unit.pos_x;
		const double dy = y - unit.pos_y;
		const double dist_sq = dx * dx + dy * dy;
		if (dist_sq < collision_radius_sq) {
			return true;
		}
	}
	return false;
}

Vector2 find_valid_dash_position(
		const SimWorld &world,
		double start_x,
		double start_y,
		double target_x,
		double target_y,
		double max_distance,
		int64_t exclude_instance_id) {
	Vector2 direction(target_x - start_x, target_y - start_y);
	if (direction.length_squared() < EPSILON * EPSILON) {
		return Vector2(start_x, start_y);
	}
	direction = direction.normalized();
	if (!position_collides_with_unit(world, target_x, target_y, exclude_instance_id)) {
		return Vector2(target_x, target_y);
	}
	constexpr double step_size = 0.01;
	double outward_dist = step_size;
	double inward_dist = step_size;
	const double target_to_start_dist = distance_between_coords(target_x, target_y, start_x, start_y);
	while (outward_dist <= max_distance || inward_dist <= target_to_start_dist) {
		if (outward_dist <= max_distance) {
			double test_x_out = target_x + direction.x * outward_dist;
			double test_y_out = target_y + direction.y * outward_dist;
			test_x_out = Math::clamp(test_x_out, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			test_y_out = Math::clamp(test_y_out, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			if (!position_collides_with_unit(world, test_x_out, test_y_out, exclude_instance_id)) {
				return Vector2(test_x_out, test_y_out);
			}
		}
		if (inward_dist <= target_to_start_dist) {
			double test_x_in = target_x - direction.x * inward_dist;
			double test_y_in = target_y - direction.y * inward_dist;
			test_x_in = Math::clamp(test_x_in, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			test_y_in = Math::clamp(test_y_in, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			if (!position_collides_with_unit(world, test_x_in, test_y_in, exclude_instance_id)) {
				return Vector2(test_x_in, test_y_in);
			}
		}
		outward_dist += step_size;
		inward_dist += step_size;
	}
	return Vector2(start_x, start_y);
}

void move_toward_target(SimWorld &world, UnitState &unit, const UnitState &target) {
	const double target_range = targeting::effective_attack_range(unit);
	move_toward_target_with_range(world, unit, target, target_range);
}

void move_toward_target_with_range(SimWorld &world, UnitState &unit, const UnitState &target, double target_range) {
	double speed = get_effective_move_speed(unit) * movement_speed_multiplier(unit) * world.tick_rate;
	if (unit.last_kite_timer > 0.0) {
		speed *= KITE_SPEED_MODIFIER;
	}
	if (speed <= 0.0) {
		return;
	}
	const double dx = target.pos_x - unit.pos_x;
	const double dy = target.pos_y - unit.pos_y;
	const double distance = distance_between_coords(unit.pos_x, unit.pos_y, target.pos_x, target.pos_y);
	if (distance <= EPSILON) {
		return;
	}
	const double desired_step = Math::max(0.0, distance - target_range);
	const double max_step = Math::min(speed, desired_step);
	if (max_step <= 0.0) {
		return;
	}
	const double nx = dx / distance;
	const double ny = dy / distance;
		unit.pos_x = Math::clamp(unit.pos_x + nx * max_step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
		unit.pos_y = Math::clamp(unit.pos_y + ny * max_step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
		on_unit_position_changed(world, unit);
}

bool kite_from_enemies(SimWorld &world, SimHostCallbacks &host, UnitState &unit, const KiteProfileCounters *profile) {
	const StringName &enemy_team = unit.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = alive_indices_for_team(world, enemy_team);
	if (enemy_indices.empty()) {
		return false;
	}
	const double attack_range = targeting::attack_range(unit);
	const double danger_radius = attack_range * KITE_DANGER_THRESHOLD;
	if (danger_radius <= 0.0) {
		return false;
	}
	const double danger_r2 = danger_radius * danger_radius;
	const double ux = unit.pos_x;
	const double uy = unit.pos_y;
	double rep_x = 0.0;
	double rep_y = 0.0;
	int count = 0;
	const bool profile_active = profile != nullptr && profile->active;
	if (use_spatial_broad_phase(world)) {
		ProfileAccScope scope(profile_active, profile != nullptr ? profile->kiting_spatial : nullptr);
		fill_buckets_for_indices_cached(world, enemy_indices);
		stamp_kite_threat(world, ux, uy, danger_radius);
		for (const int64_t idx : enemy_indices) {
			if (!stamp_has(world, idx)) {
				continue;
			}
			const UnitState &enemy = world.units[static_cast<size_t>(idx)];
			if (!enemy.alive) {
				continue;
			}
			const double dx = ux - enemy.pos_x;
			const double dy = uy - enemy.pos_y;
			const double d2 = dx * dx + dy * dy;
			if (d2 <= EPSILON || d2 >= danger_r2) {
				continue;
			}
			const double d = Math::sqrt(d2);
			const double weight = 1.0 / d;
			rep_x += (dx / d) * weight;
			rep_y += (dy / d) * weight;
			count += 1;
		}
	} else {
		ProfileAccScope scope(profile_active, profile != nullptr ? profile->kiting_brute : nullptr);
		for (const int64_t idx : enemy_indices) {
			const UnitState &enemy = world.units[static_cast<size_t>(idx)];
			if (!enemy.alive) {
				continue;
			}
			const double dx = ux - enemy.pos_x;
			const double dy = uy - enemy.pos_y;
			const double d2 = dx * dx + dy * dy;
			if (d2 <= EPSILON || d2 >= danger_r2) {
				continue;
			}
			const double d = Math::sqrt(d2);
			const double weight = 1.0 / d;
			rep_x += (dx / d) * weight;
			rep_y += (dy / d) * weight;
			count += 1;
		}
	}
	if (count <= 0) {
		return false;
	}
	const double mag = distance_between_coords(0.0, 0.0, rep_x, rep_y);
	if (mag <= EPSILON) {
		return false;
	}
	double vel_x = rep_x / mag;
	double vel_y = rep_y / mag;
	const double boundary_safe_min = WORLD_BOUNDARY_MIN + BOUNDARY_DETECTION_MARGIN + 1.0;
	const double boundary_safe_max = WORLD_BOUNDARY_MAX - BOUNDARY_DETECTION_MARGIN - 1.0;
	if (!(ux >= boundary_safe_min && ux <= boundary_safe_max && uy >= boundary_safe_min && uy <= boundary_safe_max)) {
		const bool blocked_x = (ux <= WORLD_BOUNDARY_MIN + BOUNDARY_DETECTION_MARGIN && vel_x < 0.0) ||
				(ux >= WORLD_BOUNDARY_MAX - BOUNDARY_DETECTION_MARGIN && vel_x > 0.0);
		const bool blocked_y = (uy <= WORLD_BOUNDARY_MIN + BOUNDARY_DETECTION_MARGIN && vel_y < 0.0) ||
				(uy >= WORLD_BOUNDARY_MAX - BOUNDARY_DETECTION_MARGIN && vel_y > 0.0);
		if (blocked_x) {
			vel_x = 0.0;
		}
		if (blocked_y) {
			vel_y = 0.0;
		}
	}
	if (Math::is_zero_approx(vel_x) && Math::is_zero_approx(vel_y)) {
		vel_x = ux < (WORLD_SIZE * 0.5) ? RECOVERY_VELOCITY : -RECOVERY_VELOCITY;
		vel_y = uy < (WORLD_SIZE * 0.5) ? RECOVERY_VELOCITY : -RECOVERY_VELOCITY;
	}
	const double new_mag = distance_between_coords(0.0, 0.0, vel_x, vel_y);
	if (new_mag <= EPSILON) {
		return false;
	}
	vel_x /= new_mag;
	vel_y /= new_mag;
	unit.last_kite_timer = KITE_DURATION;
	const double move_speed = get_effective_move_speed(unit) * movement_speed_multiplier(unit);
	const double step = move_speed * KITE_SPEED_MODIFIER * world.tick_rate;
	const double new_x = Math::clamp(ux + vel_x * step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	const double new_y = Math::clamp(uy + vel_y * step, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	const double dx = new_x - ux;
	const double dy = new_y - uy;
	if (dx * dx + dy * dy > EPSILON) {
		unit.pos_x = new_x;
		unit.pos_y = new_y;
		on_unit_position_changed(world, unit);
		sync_frame_unit(world, unit, &host);
	}
	return true;
}

} // namespace movement
} // namespace sim
