#include "sim_viewer.hpp"

#include "sim_constants.hpp"
#include "sim_stats.hpp"
#include "sim_stats.inl.hpp"
#include "sim_status.hpp"

namespace sim {
namespace viewer {

void ViewerFxBuffer::clear() {
	events.clear();
}

void ViewerFxBuffer::push(const ViewerFxEvent &ev) {
	if (events.size() >= VIEWER_FX_CAP) {
		return;
	}
	events.push_back(ev);
}

void record_damage_fx(
		ViewerFxBuffer &buffer,
		const UnitState &source,
		const UnitState &target,
		double total_damage,
		const StringName &action_kind,
		const StringName &damage_type) {
	ViewerFxEvent ev;
	ev.kind = StringName("damage");
	ev.target_id = target.instance_id;
	ev.src_id = source.instance_id;
	ev.pos_x = target.pos_x;
	ev.pos_y = target.pos_y;
	ev.val = total_damage;
	ev.damage_type = damage_type;
	buffer.push(ev);
	if (action_kind == StringName("auto") && (source.stats_dirty ? get_effective_attack_range(source) : source.cached_attack_range) <= RANGED_THRESHOLD) {
		ViewerFxEvent slash;
		slash.kind = StringName("melee_slash");
		slash.pos_x = target.pos_x;
		slash.pos_y = target.pos_y;
		buffer.push(slash);
	}
}

void record_heal_fx(ViewerFxBuffer &buffer, const UnitState &target, double amount) {
	ViewerFxEvent ev;
	ev.kind = StringName("heal");
	ev.target_id = target.instance_id;
	ev.pos_x = target.pos_x;
	ev.pos_y = target.pos_y;
	ev.val = amount;
	buffer.push(ev);
}

void record_shield_fx(ViewerFxBuffer &buffer, const UnitState &target, double amount) {
	ViewerFxEvent ev;
	ev.kind = StringName("shield");
	ev.target_id = target.instance_id;
	ev.pos_x = target.pos_x;
	ev.pos_y = target.pos_y;
	ev.val = amount;
	buffer.push(ev);
}

void record_aoe_shape_fx(
		ViewerFxBuffer &buffer,
		const SimWorld &world,
		const UnitState &source,
		const UnitState *target,
		const AoShapeParams &params,
		const StringName &kind) {
	ViewerFxEvent ev;
	ev.kind = kind;
	ev.src_id = source.instance_id;
	ev.target_id = target != nullptr ? target->instance_id : params.target_id;

	if (params.anchor == AoAnchorKind::Self) {
		ev.pos_x = source.pos_x;
		ev.pos_y = source.pos_y;
	} else if (params.anchor == AoAnchorKind::Target) {
		const UnitState *resolved = target != nullptr ? target : targeting::unit_by_id(world, params.target_id);
		if (resolved != nullptr) {
			ev.pos_x = resolved->pos_x;
			ev.pos_y = resolved->pos_y;
		} else {
			ev.pos_x = source.pos_x;
			ev.pos_y = source.pos_y;
		}
	} else if (params.anchor == AoAnchorKind::Forward) {
		const Vector2 direction = status::resolve_aoe_direction(world, source, params, target);
		if (params.shape == AoShapeKind::Rectangle) {
			ev.pos_x = source.pos_x + direction.x * params.height * 0.5;
			ev.pos_y = source.pos_y + direction.y * params.height * 0.5;
		} else if (params.shape == AoShapeKind::Cone) {
			ev.pos_x = source.pos_x;
			ev.pos_y = source.pos_y;
		} else if (params.shape == AoShapeKind::Circle) {
			ev.pos_x = source.pos_x + direction.x * params.radius * 0.5;
			ev.pos_y = source.pos_y + direction.y * params.radius * 0.5;
		} else {
			ev.pos_x = source.pos_x;
			ev.pos_y = source.pos_y;
		}
	} else {
		ev.pos_x = params.anchor_x;
		ev.pos_y = params.anchor_y;
	}

	Dictionary shape_dict;
	switch (params.shape) {
		case AoShapeKind::Circle:
			shape_dict["shape"] = "circle";
			break;
		case AoShapeKind::Cone:
			shape_dict["shape"] = "cone";
			break;
		case AoShapeKind::Rectangle:
			shape_dict["shape"] = "rectangle";
			break;
	}

	switch (params.anchor) {
		case AoAnchorKind::Self:
			shape_dict["anchor"] = "self";
			break;
		case AoAnchorKind::Target:
			shape_dict["anchor"] = "target";
			break;
		case AoAnchorKind::Point:
			shape_dict["anchor"] = "point";
			break;
		case AoAnchorKind::Forward:
			shape_dict["anchor"] = "forward";
			break;
	}

	shape_dict["radius"] = params.radius;
	shape_dict["width"] = params.width;
	shape_dict["height"] = params.height;
	shape_dict["rotation_radians"] = params.rotation_radians;
	shape_dict["anchor_x"] = params.anchor_x;
	shape_dict["anchor_y"] = params.anchor_y;
	shape_dict["target_id"] = ev.target_id;
	const Vector2 forward = status::resolve_aoe_direction(world, source, params, target);
	shape_dict["forward_x"] = forward.x;
	shape_dict["forward_y"] = forward.y;

	ev.val = 0.0;
	ev.radius = params.radius;
	ev.extra = shape_dict;
	buffer.push(ev);
}

void record_hot_status_fx(ViewerFxBuffer &buffer, const UnitState &target, double duration, const StringName & /*effect_type*/) {
	ViewerFxEvent ev;
	ev.kind = StringName("hot_status");
	ev.target_id = target.instance_id;
	ev.val = duration;
	ev.radius = 0.0;
	buffer.push(ev);
}

void record_dot_status_fx(ViewerFxBuffer &buffer, const UnitState &target, double duration, const StringName & /*effect_type*/) {
	ViewerFxEvent ev;
	ev.kind = StringName("dot_status");
	ev.target_id = target.instance_id;
	ev.val = duration;
	ev.radius = 0.0;
	buffer.push(ev);
}

void record_passive_aoe_fx(ViewerFxBuffer &buffer, const UnitState &unit, double radius, const StringName &passive_id) {
	ViewerFxEvent ev;
	ev.kind = StringName("passive_aoe");
	ev.target_id = unit.instance_id;
	ev.src_id = unit.instance_id;
	ev.pos_x = unit.pos_x;
	ev.pos_y = unit.pos_y;
	ev.val = 0.0;
	ev.radius = radius;
	Dictionary extra;
	extra["passive_id"] = String(passive_id);
	ev.extra = extra;
	buffer.push(ev);
}

void record_unit_death_fx(ViewerFxBuffer &buffer, const UnitState &target) {
	ViewerFxEvent ev;
	ev.kind = StringName("unit_death");
	ev.target_id = target.instance_id;
	ev.pos_x = target.pos_x;
	ev.pos_y = target.pos_y;
	buffer.push(ev);
}

} // namespace viewer
} // namespace sim
