#include "sim_effects_compile_internal.hpp"

#include "sim_effects_compile.hpp"

#include "sim_constants.hpp"
#include "sim_effect_kinds.inl.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::compile::internal {

using namespace sim::effect_kinds;

bool try_fill_aoe(EffectRecord &compiled, const StringName &kind, ParamTracker &tracker, const Dictionary &params) {
	if (kind == sn_aoe_taunt()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", "")); // INCONSISTENT: other AOE effects use descriptive defaults like "AOE Slow"
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		return true;
	}
	if (kind == sn_aoe_damage()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("splash_ratio", 1.0));
		compiled.scalar3 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar4 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.int0 = tracker.get("use_accumulated_damage", false) ? 1 : 0;
		String damage_type_str = String(tracker.get("damage_type", "physical"));
		if (damage_type_str == "physical") {
			compiled.damage_type = sn_physical();
		} else if (damage_type_str == "magic") {
			compiled.damage_type = sn_magic();
		} else if (damage_type_str == "true") {
			compiled.damage_type = sn_true();
		} else {
			compiled.damage_type = StringName(damage_type_str);
		}
		compiled.reason = String(tracker.get("reason", "")); // INCONSISTENT: other AOE effects use descriptive defaults like "AOE Slow"
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		return true;
	}
	if (kind == sn_aoe_slow()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("slow_percentage", 0.0));
		compiled.scalar2 = double(tracker.get("duration", 0.0));
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Slow"));
		return true;
	}
	if (kind == sn_aoe_root()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Root"));
		return true;
	}
	if (kind == sn_aoe_silence()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("block_abilities", true) ? 1 : 0;
		compiled.int1 = tracker.get("block_ultimate", true) ? 1 : 0;
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Silence"));
		return true;
	}
	if (kind == sn_aoe_disarm()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Disarm"));
		return true;
	}
	if (kind == sn_aoe_knockback()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("distance", 0.0));
		compiled.scalar2 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("direction", "away_from_source") == "away_from_source" ? 1 : 0;
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Knockback"));
		return true;
	}
	if (kind == sn_aoe_reflect()) {
		// TODO: Rework reflect_type to be selectable per damage type (physical, magic, true) instead of binary "all" vs "physical"
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("reflect_percentage", 0.0));
		compiled.scalar2 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("reflect_type", "all") == "all" ? 1 : 0;
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Reflect"));
		return true;
	}
	if (kind == sn_aoe_stun()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Stun"));
		return true;
	}
	return false;
}

} // namespace sim::effects::compile::internal
