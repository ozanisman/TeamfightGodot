#include "sim_effects_compile_internal.hpp"

#include "sim_effects_compile.hpp"

#include "sim_effect_kinds.inl.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::compile::internal {

using namespace sim::effect_kinds;

namespace {

StringName validated_projectile_policy(
		ParamTracker &tracker,
		const String &param_name,
		const StringName &default_value,
		const StringName &supported_value) {
	const StringName value = StringName(String(tracker.get(param_name, String(default_value))));
	if (value != supported_value) {
		UtilityFunctions::push_error(vformat(
				"Unsupported projectile %s '%s'. Only '%s' is currently supported.",
				param_name,
				String(value),
				String(supported_value)));
		return supported_value;
	}
	return value;
}

} // namespace

bool try_fill_damage(EffectRecord &compiled, const StringName &kind, ParamTracker &tracker, const Dictionary &params) {
	if (kind == sn_constant_multiplier()) {
		compiled.scalar0 = double(tracker.get("multiplier", 1.0));
		// INCONSISTENT: no reason string
		return true;
	}
	if (kind == sn_hp_threshold_damage_multiplier()) {
		compiled.scalar0 = double(tracker.get("above_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("below_hp_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("multiplier", 1.0));
		// INCONSISTENT: no reason string
		return true;
	}
	if (kind == sn_distance_threshold_multiplier()) {
		compiled.scalar0 = double(tracker.get("min_distance", 0.0));
		compiled.scalar1 = double(tracker.get("multiplier", 1.0));
		// INCONSISTENT: no reason string
		return true;
	}
	if (kind == sn_damage()) {
		compiled.scalar0 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar3 = bool(tracker.get("trigger_on_hit", false)) ? 1.0 : 0.0;
		compiled.scalar4 = double(tracker.get("current_hp_ratio", 0.0));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;
		compiled.int1 = tracker.get("use_accumulated_damage", false) ? 1 : 0;
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
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_projectile()) {
		// Use -1.0 as sentinel for "use unit's projectile_speed/radius stat" when override is null.
		// Python parity: speed_override=None â†’ unit.stats.projectile_speed, radius_override=None â†’ unit.stats.projectile_radius.
		Variant speed_v = tracker.get("speed_override", Variant());
		compiled.scalar0 = (speed_v.get_type() == Variant::NIL) ? -1.0 : double(speed_v);
		Variant radius_v = tracker.get("radius_override", Variant());
		compiled.scalar1 = (radius_v.get_type() == Variant::NIL) ? -1.0 : double(radius_v);
		compiled.damage_type = validated_projectile_policy(tracker, "motion", sn_homing(), sn_homing());
		compiled.stat_name = validated_projectile_policy(tracker, "collision", sn_target_only(), sn_target_only());
		compiled.stacking_mode = validated_projectile_policy(tracker, "on_target_lost", sn_drop(), sn_drop());
		compiled.effect_type = StringName(String(tracker.get("visual_id", "")));
		Variant on_hit = params.get("on_hit", Variant());
		tracker.mark_accessed("on_hit");
		if (on_hit.get_type() != Variant::DICTIONARY) {
			UtilityFunctions::push_error("Projectile effect requires params.on_hit as an effect Dictionary.");
			compiled.children.clear();
		} else {
			compiled.children.push_back(compile_effect(Dictionary(on_hit)));
		}
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_damage_over_time()) {
		// Ratio-based parameters (now represent TOTAL amounts over full duration)
		compiled.scalar0 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("tick_interval", 1.0));
		compiled.scalar3 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar4 = double(tracker.get("duration", 0.0));  // Duration moved to scalar4
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
		compiled.stacking_mode = StringName(tracker.get("stacking_mode", "refresh"));
		compiled.effect_type = StringName(tracker.get("effect_type", "generic"));
		compiled.reason = String(tracker.get("reason", ""));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;  // target_self parameter
		compiled.int1 = int64_t(tracker.get("max_stacks", 0));
		String calculation_str = String(tracker.get("calculation", "fixed"));
		compiled.int2 = (calculation_str == "dynamic") ? 1 : 0;  // 0=fixed, 1=dynamic
		return true;
	}
	if (kind == sn_heal_over_time()) {
		// Ratio-based parameters (now represent TOTAL amounts over full duration)
		compiled.scalar0 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("current_hp_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("tick_interval", 1.0));
		compiled.scalar3 = double(tracker.get("missing_hp_ratio", 0.0));
		compiled.scalar4 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar5 = double(tracker.get("duration", 0.0));  // Duration moved to scalar5
		compiled.stacking_mode = StringName(tracker.get("stacking_mode", "refresh"));
		compiled.effect_type = StringName(tracker.get("effect_type", "generic"));
		compiled.reason = String(tracker.get("reason", ""));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;  // target_self parameter
		compiled.int1 = int64_t(tracker.get("max_stacks", 0));
		compiled.int2 = bool(tracker.get("allow_overheal", false)) ? 1 : 0;
		String calculation_str = String(tracker.get("calculation", "fixed"));
		compiled.int3 = (calculation_str == "dynamic") ? 1 : 0;  // 0=fixed, 1=dynamic
		return true;
	}
	if (kind == sn_aoe_damage_over_time()) {
		// AoE parameters
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		// DoT ratio parameters (now represent TOTAL amounts over full duration)
		compiled.scalar1 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar3 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar4 = double(tracker.get("tick_interval", 1.0));
		compiled.scalar5 = double(tracker.get("duration", 0.0));  // Duration moved to scalar5
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
		compiled.stacking_mode = StringName(tracker.get("stacking_mode", "refresh"));
		compiled.effect_type = StringName(tracker.get("effect_type", "generic"));
		compiled.reason = String(tracker.get("reason", "")); // INCONSISTENT: other AOE effects use descriptive defaults like "AOE Slow"
		compiled.int0 = int64_t(tracker.get("max_stacks", 0));
		String calculation_str = String(tracker.get("calculation", "fixed"));
		compiled.int1 = (calculation_str == "dynamic") ? 1 : 0;  // 0=fixed, 1=dynamic
		compiled.int2 = tracker.get("target_self", false) ? 1 : 0;
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		return true;
	}
	if (kind == sn_aoe_heal_over_time()) {
		// AoE parameters
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		// HoT ratio parameters (now represent TOTAL amounts over full duration)
		compiled.scalar1 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("current_hp_ratio", 0.0));
		compiled.scalar3 = double(tracker.get("missing_hp_ratio", 0.0));
		compiled.scalar4 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar5 = double(tracker.get("tick_interval", 1.0));
		compiled.stacking_mode = StringName(tracker.get("stacking_mode", "refresh"));
		compiled.effect_type = StringName(tracker.get("effect_type", "generic"));
		compiled.reason = String(tracker.get("reason", "")); // INCONSISTENT: other AOE effects use descriptive defaults like "AOE Slow"
		compiled.int0 = int64_t(tracker.get("max_stacks", 0));
		compiled.int1 = int64_t(tracker.get("duration", 0.0));  // Duration stays in int1
		compiled.int2 = bool(tracker.get("allow_overheal", false)) ? 1 : 0;
		compiled.int3 = tracker.get("target_self", false) ? 1 : 0;
		String calculation_str = String(tracker.get("calculation", "fixed"));
		compiled.int4 = (calculation_str == "dynamic") ? 1 : 0;  // 0=fixed, 1=dynamic
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		return true;
	}
	if (kind == sn_damage_threshold_trigger()) {
		compiled.scalar0 = double(tracker.get("threshold_multiplier", 1.0));
		Variant nested = tracker.get("effect", Variant());
		if (nested.get_type() == Variant::DICTIONARY) {
			compiled.children.push_back(compile_effect(Dictionary(nested)));
		}
		// INCONSISTENT: no reason string
		return true;
	}
	if (kind == sn_mana_regen()) {
		compiled.scalar0 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar1 = double(tracker.get("mana_cost_ratio", 0.0));
		// INCONSISTENT: no reason string
		return true;
	}
	if (kind == sn_post_damage_mana_gain()) {
		compiled.scalar0 = double(tracker.get("damage_ratio", 0.0));
		// INCONSISTENT: no reason string
		return true;
	}
	if (kind == sn_damage_based_heal()) {
		compiled.scalar0 = double(tracker.get("damage_ratio", 0.0));
		compiled.int0 = tracker.get("use_accumulated_damage", false) ? 1 : 0;
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_damage_based_shield()) {
		compiled.scalar0 = double(tracker.get("damage_ratio", 0.0));
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_consume_stacks_damage()) {
		compiled.stat_name = StringName(tracker.get("stat_name", ""));
		compiled.scalar0 = double(tracker.get("base_damage_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("stack_bonus_ratio", 0.0));
		compiled.string0 = String(tracker.get("stacking_mode", "multiplicative"));
		compiled.damage_type = StringName(tracker.get("damage_type", "physical"));
		compiled.string1 = String(tracker.get("stack_reason", ""));
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_consume_stacks_heal()) {
		compiled.stat_name = StringName(tracker.get("stat_name", ""));
		compiled.scalar0 = double(tracker.get("base_heal_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("stack_bonus_ratio", 0.0));
		compiled.string0 = String(tracker.get("stacking_mode", "multiplicative"));
		compiled.string1 = String(tracker.get("stack_reason", ""));
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_consume_stacks_shield()) {
		compiled.stat_name = StringName(tracker.get("stat_name", ""));
		compiled.scalar0 = double(tracker.get("base_shield_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("stack_bonus_ratio", 0.0));
		compiled.string0 = String(tracker.get("stacking_mode", "multiplicative"));
		compiled.string1 = String(tracker.get("stack_reason", ""));
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_reflect_damage()) {
		// TODO: Rework reflect_type to be selectable per damage type (physical, magic, true) instead of binary "all" vs "physical"
		compiled.scalar0 = double(tracker.get("reflect_percentage", 0.0));
		compiled.int0 = tracker.get("reflect_type", "all") == "all" ? 1 : 0;
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_redirect_damage()) {
		compiled.scalar0 = Math::clamp(double(tracker.get("redirect_ratio", 0.0)), 0.0, 1.0);
		compiled.scalar1 = Math::clamp(double(tracker.get("reduction_ratio", 0.0)), 0.0, 1.0);
		compiled.scalar2 = double(tracker.get("redirect_cap", 0.0));
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	return false;
}

} // namespace sim::effects::compile::internal
