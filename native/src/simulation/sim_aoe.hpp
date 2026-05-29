#ifndef SIM_AOE_HPP
#define SIM_AOE_HPP

#include "sim_spatial.hpp"

#include <godot_cpp/variant/vector2.hpp>

#include <cstdint>
#include <vector>

namespace sim {

/// Parameters for circular AoE iteration over an alive-team index list.
/// `spatial_team` must match `UnitState::team` for units referenced by `indices` (used by broad-phase stamp).
struct AoCircleIterationParams {
	double center_x = 0.0;
	double center_y = 0.0;
	double radius = 0.0;
	const std::vector<int64_t> *indices = nullptr;
	StringName spatial_team;
	int64_t exclude_instance_id = 0;
};

/// Parameters for universal AoE shape iteration over an alive-team index list.
struct AoShapeIterationParams {
	AoShapeParams shape_params;
	const std::vector<int64_t> *indices = nullptr;
	StringName spatial_team;
	int64_t exclude_instance_id = 0;
	const UnitState *source = nullptr;
	const UnitState *target_override = nullptr;
};

/// Inclusive disk: `dist_sq <= radius²`. Uses spatial broad-phase when enabled (threshold on alive counts).
template<typename Fn>
void for_each_unit_in_circle(SimWorld &world, const AoCircleIterationParams &p, Fn &&fn) {
	if (p.radius <= 0.0 || p.indices == nullptr) {
		return;
	}
	const double r2 = p.radius * p.radius;
	const std::vector<int64_t> indices = *p.indices;
	auto visit = [&](int64_t idx) {
		if (idx < 0 || idx >= int64_t(world.units.size())) {
			return;
		}
		UnitState &u = world.units[static_cast<size_t>(idx)];
		if (!u.alive) {
			return;
		}
		if (p.exclude_instance_id != 0 && u.instance_id == p.exclude_instance_id) {
			return;
		}
		const double dx = u.pos_x - p.center_x;
		const double dy = u.pos_y - p.center_y;
		if (dx * dx + dy * dy <= r2) {
			fn(u);
		}
	};
	if (!use_spatial_broad_phase(world)) {
		for (int64_t idx : indices) {
			visit(idx);
		}
		return;
	}
	fill_buckets_for_indices(world, indices);
	stamp_circle(world, p.center_x, p.center_y, p.radius, p.spatial_team);
	for (int64_t idx : indices) {
		if (!stamp_has(world, idx)) {
			continue;
		}
		visit(idx);
	}
}

/// Universal AoE shape iteration: supports circle, cone, rectangle, line.
/// Uses spatial broad-phase when enabled (threshold on alive counts).
template<typename Fn, typename ResolveDirectionFn>
void for_each_unit_in_shape(SimWorld &world, const AoShapeIterationParams &p, Fn &&fn, ResolveDirectionFn &&resolve_direction) {
	if (p.indices == nullptr) {
		return;
	}
	const std::vector<int64_t> indices = *p.indices;

	// Resolve anchor position
	double center_x = 0.0;
	double center_y = 0.0;
	if (p.shape_params.anchor == AoAnchorKind::Self && p.source != nullptr) {
		center_x = p.source->pos_x;
		center_y = p.source->pos_y;
	} else if (p.shape_params.anchor == AoAnchorKind::Target && p.target_override != nullptr) {
		center_x = p.target_override->pos_x;
		center_y = p.target_override->pos_y;
	} else if (p.shape_params.anchor == AoAnchorKind::Point) {
		center_x = p.shape_params.anchor_x;
		center_y = p.shape_params.anchor_y;
	} else if (p.shape_params.anchor == AoAnchorKind::Forward && p.source != nullptr) {
		Vector2 direction = resolve_direction(*p.source, p.shape_params, p.target_override);
		if (p.shape_params.shape == AoShapeKind::Rectangle) {
			center_x = p.source->pos_x + direction.x * p.shape_params.height * 0.5;
			center_y = p.source->pos_y + direction.y * p.shape_params.height * 0.5;
		} else if (p.shape_params.shape == AoShapeKind::Cone) {
			center_x = p.source->pos_x;
			center_y = p.source->pos_y;
		} else if (p.shape_params.shape == AoShapeKind::Circle) {
			center_x = p.source->pos_x + direction.x * p.shape_params.radius * 0.5;
			center_y = p.source->pos_y + direction.y * p.shape_params.radius * 0.5;
		} else {
			center_x = p.source->pos_x;
			center_y = p.source->pos_y;
		}
	} else if (p.source != nullptr) {
		center_x = p.source->pos_x;
		center_y = p.source->pos_y;
	}

	Vector2 forward(1.0, 0.0);
	if (p.source != nullptr) {
		forward = resolve_direction(*p.source, p.shape_params, p.target_override);
	}

	auto shape_contains = [&](const UnitState &u) -> bool {
		const double dx = u.pos_x - center_x;
		const double dy = u.pos_y - center_y;

		switch (p.shape_params.shape) {
			case AoShapeKind::Circle: {
				const double r2 = p.shape_params.radius * p.shape_params.radius;
				return dx * dx + dy * dy <= r2;
			}
			case AoShapeKind::Cone: {
				const double r2 = p.shape_params.radius * p.shape_params.radius;
				if (dx * dx + dy * dy > r2) {
					return false;
				}
				const double half_angle = p.shape_params.width * 0.5;
				Vector2 to_unit(dx, dy);
				if (to_unit.length_squared() < EPSILON * EPSILON) {
					return true;
				}
				to_unit = to_unit.normalized();
				const double dot = to_unit.dot(forward);
				const double angle_cos = Math::cos(half_angle);
				return dot >= angle_cos;
			}
			case AoShapeKind::Rectangle: {
				const double half_w = p.shape_params.width * 0.5;
				const double half_h = p.shape_params.height * 0.5;
				Vector2 to_unit(dx, dy);
				Vector2 right = Vector2(-forward.y, forward.x);
				const double forward_dist = to_unit.dot(forward);
				const double right_dist = to_unit.dot(right);
				return Math::abs(forward_dist) <= half_h && Math::abs(right_dist) <= half_w;
			}
			default:
				return false;
		}
	};

	auto visit = [&](int64_t idx) {
		if (idx < 0 || idx >= int64_t(world.units.size())) {
			return;
		}
		UnitState &u = world.units[static_cast<size_t>(idx)];
		if (!u.alive) {
			return;
		}
		if (p.exclude_instance_id != 0 && u.instance_id == p.exclude_instance_id) {
			return;
		}
		if (shape_contains(u)) {
			fn(u);
		}
	};

	if (!use_spatial_broad_phase(world)) {
		for (int64_t idx : indices) {
			visit(idx);
		}
		return;
	}

	double bounds_radius = p.shape_params.radius;
	if (p.shape_params.shape == AoShapeKind::Rectangle) {
		bounds_radius = Math::sqrt(p.shape_params.width * p.shape_params.width + p.shape_params.height * p.shape_params.height) * 0.5;
	}

	fill_buckets_for_indices(world, indices);
	stamp_circle(world, center_x, center_y, bounds_radius, p.spatial_team);
	for (int64_t idx : indices) {
		if (!stamp_has(world, idx)) {
			continue;
		}
		visit(idx);
	}
}

} // namespace sim

#endif // SIM_AOE_HPP
