#include "sim_effects_compile_internal.hpp"

#include "sim_effects_compile.hpp"

#include "sim_constants.hpp"
#include "sim_effect_kinds.inl.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::compile::internal {

using namespace sim::effect_kinds;

bool try_fill_status(EffectRecord &compiled, const StringName &kind, ParamTracker &tracker, const Dictionary &params) {
	if (kind == sn_stun()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_shield()) {
		compiled.scalar0 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("flat_amount", 0.0));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_heal()) {
		compiled.scalar0 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("current_hp_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar3 = double(tracker.get("missing_hp_ratio", 0.0));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;  // target_self parameter
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_set_stacks()) {
		compiled.stat_name = StringName(tracker.get("stat_name", ""));
		compiled.int0 = int64_t(tracker.get("stack_count", 0));
		compiled.int1 = tracker.get("to_max", false);
		compiled.int2 = int64_t(tracker.get("max_stacks", 0));  // Fallback max_stacks when entry doesn't exist
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.scalar1 = double(tracker.get("additive_per_stack", 0.0));  // Fallback when entry doesn't exist
		compiled.scalar2 = double(tracker.get("multiplicative_per_stack", 1.0));  // Fallback when entry doesn't exist
		compiled.string0 = String(tracker.get("reason", ""));
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_channel()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));  // total duration
		compiled.scalar1 = double(tracker.get("tick_interval", 1.0));  // tick interval
		compiled.int0 = tracker.get("allow_movement", false) ? 1 : 0;
		compiled.string0 = String(tracker.get("target_mode", "fixed"));  // "fixed", "dynamic", or "self"
		compiled.reason = String(tracker.get("reason", ""));
		
		// Parse sub_effect (required)
		if (params.has("sub_effect")) {
			tracker.mark_accessed("sub_effect");
			Dictionary sub_effect_dict = params["sub_effect"];
			compiled.children.push_back(compile_effect(sub_effect_dict));
		}
		
		// Parse post_complete_effect (optional)
		if (params.has("post_complete_effect")) {
			tracker.mark_accessed("post_complete_effect");
			Dictionary post_complete_dict = params["post_complete_effect"];
			compiled.sub_effects.push_back(compile_effect(post_complete_dict));
		}
		
		// Parse post_interrupt_effect (optional)
		if (params.has("post_interrupt_effect")) {
			tracker.mark_accessed("post_interrupt_effect");
			Dictionary post_interrupt_dict = params["post_interrupt_effect"];
			compiled.sub_effects.push_back(compile_effect(post_interrupt_dict));
		}
		return true;
	}
	if (kind == sn_mana_restore_on_hit()) {
		compiled.scalar0 = double(tracker.get("flat_amount", 0.0));
		// INCONSISTENT: no reason string
		return true;
	}
	if (kind == sn_drain_target_mana_on_hit()) {
		compiled.scalar0 = double(tracker.get("flat_amount", 0.0));
		// INCONSISTENT: no reason string
		return true;
	}
	if (kind == sn_every_n_attacks_stun()) {
		compiled.int0 = int64_t(tracker.get("every_n", 0));
		compiled.scalar0 = double(tracker.get("stun_duration", 0.0));
		compiled.reason = String(tracker.get("reason", ""));
		return true;
	}
	if (kind == sn_self_dash()) {
		compiled.scalar0 = double(tracker.get("distance", 0.0));
		Dictionary direction = Dictionary(tracker.get("direction", Dictionary()));
		compiled.scalar1 = double(direction.get("x", 0.0));
		compiled.scalar2 = double(direction.get("y", 0.0));
		// INCONSISTENT: no reason string
		return true;
	}
	if (kind == sn_auto_dodge()) {
		compiled.scalar0 = double(tracker.get("dodge_chance", 0.0));
		compiled.scalar1 = double(tracker.get("on_dodge_multiplier", 0.0));
		compiled.scalar2 = double(tracker.get("on_hit_multiplier", 1.0));
		// INCONSISTENT: no reason string
		return true;
	}
	if (kind == sn_slow()) {
		compiled.scalar0 = double(tracker.get("slow_percentage", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", "Slow"));
		return true;
	}
	if (kind == sn_root()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", "Root"));
		return true;
	}
	if (kind == sn_silence()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("block_abilities", true) ? 1 : 0;
		compiled.int1 = tracker.get("block_ultimate", true) ? 1 : 0;
		compiled.reason = String(tracker.get("reason", "Silence"));
		return true;
	}
	if (kind == sn_disarm()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", "Disarm"));
		return true;
	}
	if (kind == sn_stealth()) {
		Dictionary break_conditions = Dictionary(tracker.get("break_conditions", Dictionary()));
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.int0 = bool(break_conditions.get("on_attack", false)) ? 1 : 0;
		compiled.int1 = bool(break_conditions.get("on_ability", false)) ? 1 : 0;
		compiled.int2 = bool(break_conditions.get("on_damage_taken", false)) ? 1 : 0;
		compiled.int3 = tracker.get("target_self", false) ? 1 : 0;
		compiled.reason = String(tracker.get("reason", "Stealth"));
		return true;
	}
	if (kind == sn_knockback()) {
		compiled.scalar0 = double(tracker.get("distance", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("direction", "away_from_source") == "away_from_source" ? 1 : 0;
		compiled.reason = String(tracker.get("reason", "Knockback"));
		return true;
	}
	if (kind == sn_reflect()) {
		compiled.scalar0 = double(tracker.get("reflect_percentage", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		String reflect_type_str = String(tracker.get("reflect_type", "all"));
		if (reflect_type_str == "physical") {
			compiled.damage_type = sn_physical();
		} else if (reflect_type_str == "magic") {
			compiled.damage_type = sn_magic();
		} else if (reflect_type_str == "true") {
			compiled.damage_type = sn_true();
		} else if (reflect_type_str == "all") {
			compiled.damage_type = StringName("all");
		} else {
			UtilityFunctions::push_error(vformat("Invalid reflect_type '%s' for reflect effect. Must be 'physical', 'magic', 'true', or 'all'", reflect_type_str));
			return false;
		}
		compiled.reason = String(tracker.get("reason", "Reflect"));
		return true;
	}
	if (kind == sn_target_status_multiplier()) {
		compiled.scalar0 = double(tracker.get("multiplier", 1.0));
		String status_kind_str = String(tracker.get("status_kind", ""));
		if (status_kind_str == "slow") {
			compiled.damage_type = sn_slow();
		} else if (status_kind_str == "root") {
			compiled.damage_type = sn_root();
		} else if (status_kind_str == "silence") {
			compiled.damage_type = sn_silence();
		} else if (status_kind_str == "disarm") {
			compiled.damage_type = sn_disarm();
		} else if (status_kind_str == "stealth") {
			compiled.damage_type = sn_stealth();
		} else if (status_kind_str == "stun") {
			compiled.damage_type = sn_stun();
		} else if (status_kind_str == "reflect") {
			compiled.damage_type = sn_reflect();
		} else {
			compiled.damage_type = StringName(status_kind_str);
		}
		compiled.reason = String(tracker.get("reason", "Target status multiplier"));
		return true;
	}
	if (kind == sn_stat_modifier()) {
		// Validate stat name
		StringName stat_name = StringName(String(tracker.get("stat_name", "")));
		if (!is_valid_stat_name(stat_name)) {
			compiled.opcode = EFFECT_OPCODE_UNKNOWN;
			return true;
		}

		compiled.damage_type = stat_name;
		compiled.scalar0 = double(tracker.get("additive", 0.0));
		compiled.scalar1 = double(tracker.get("multiplicative", 1.0));
		compiled.scalar2 = double(tracker.get("duration", 0.0));
		compiled.scalar3 = double(tracker.get("heal_gained_ratio", 0.0));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;
		compiled.int1 = tracker.get("duration_type", "respawn") == "match" ? 1 : 0;
		compiled.int2 = int64_t(tracker.get("max_stacks", 0));
		String stack_behavior = String(tracker.get("stack_behavior", "refresh"));
		if (stack_behavior == "accumulate") {
			compiled.int3 = 1;
		} else if (stack_behavior == "reset") {
			compiled.int3 = 2;
		} else {
			compiled.int3 = 0;
		}
		compiled.reason = String(tracker.get("reason", "Stat modifier"));
		return true;
	}
	return false;
}

} // namespace sim::effects::compile::internal
