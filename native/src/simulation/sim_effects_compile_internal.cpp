#include "sim_effects_compile_internal.hpp"

#include "sim_constants.hpp"
#include "sim_effect_kinds.inl.hpp"
#include "stat_definitions.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim::effects::compile::internal {

using namespace sim::effect_kinds;

bool is_valid_stat_name(const StringName &stat_name) {
#define X(name, def, min_val, max_val) \
	if (stat_name == StringName(#name)) return true;
	STAT_LIST
#undef X
	return false;
}

AoShapeParams parse_aoe_shape_metadata(const Dictionary &params, ParamTracker &tracker) {
	AoShapeParams shape_params;
	shape_params.shape = AoShapeKind::Circle;
	shape_params.anchor = AoAnchorKind::Self;
	shape_params.radius = 0.0;
	shape_params.width = 0.0;
	shape_params.height = 0.0;
	shape_params.rotation_radians = 0.0;
	shape_params.anchor_x = 0.0;
	shape_params.anchor_y = 0.0;
	shape_params.target_id = 0;
	
	// Parse shape
	Variant shape_var = params.get("shape", Variant());
	tracker.mark_accessed("shape");
	if (shape_var.get_type() == Variant::STRING) {
		String shape_str = String(shape_var);
		if (shape_str == "circle") {
			shape_params.shape = AoShapeKind::Circle;
		} else if (shape_str == "cone") {
			shape_params.shape = AoShapeKind::Cone;
		} else if (shape_str == "rectangle") {
			shape_params.shape = AoShapeKind::Rectangle;
		}
	}
	
	// Parse anchor
	Variant anchor_var = params.get("anchor", Variant());
	tracker.mark_accessed("anchor");
	if (anchor_var.get_type() == Variant::STRING) {
		String anchor_str = String(anchor_var);
		if (anchor_str == "self") {
			shape_params.anchor = AoAnchorKind::Self;
		} else if (anchor_str == "target") {
			shape_params.anchor = AoAnchorKind::Target;
		} else if (anchor_str == "point") {
			shape_params.anchor = AoAnchorKind::Point;
		} else if (anchor_str == "forward") {
			shape_params.anchor = AoAnchorKind::Forward;
		}
	}
	
	// Parse numeric parameters
	shape_params.radius = params.get("radius", 0.0);
	tracker.mark_accessed("radius");
	shape_params.width = params.get("width", 0.0);
	tracker.mark_accessed("width");
	shape_params.height = params.get("height", 0.0);
	tracker.mark_accessed("height");
	
	// Parse rotation (degrees to radians)
	Variant rotation_var = params.get("rotation_degrees", Variant());
	tracker.mark_accessed("rotation_degrees");
	if (rotation_var.get_type() == Variant::FLOAT || rotation_var.get_type() == Variant::INT) {
		double rotation_degrees = rotation_var;
		shape_params.rotation_radians = Math::deg_to_rad(rotation_degrees);
	}
	
	// Parse anchor position for Point anchor
	shape_params.anchor_x = params.get("anchor_x", 0.0);
	tracker.mark_accessed("anchor_x");
	shape_params.anchor_y = params.get("anchor_y", 0.0);
	tracker.mark_accessed("anchor_y");
	
	// Parse target_id for Target anchor
	shape_params.target_id = params.get("target_id", 0);
	tracker.mark_accessed("target_id");
	
	return shape_params;
}



void fill_compiled_kind_fields(
		EffectRecord &compiled,
		const StringName &kind,
		ParamTracker &tracker,
		const Dictionary &params) {
	if (try_fill_damage(compiled, kind, tracker, params)) return;
	if (try_fill_status(compiled, kind, tracker, params)) return;
	if (try_fill_aoe(compiled, kind, tracker, params)) return;
	(void)try_fill_spawn(compiled, kind, tracker, params);
}

} // namespace sim::effects::compile::internal
