#include "sim_effects_compile.hpp"

#include "sim_constants.hpp"
#include "sim_effect_kinds.inl.hpp"
#include "sim_effects_compile_internal.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::compile {

using namespace sim::effect_kinds;

void ParamTracker::report_unused(const String &effect_kind) const {
	Array keys = params.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = String(keys[i]);
		bool was_accessed = false;
		for (const auto &accessed_key : accessed) {
			if (key == accessed_key) {
				was_accessed = true;
				break;
			}
		}
		if (!was_accessed) {
			ERR_PRINT(vformat("EFFECT PARAMETER MISMATCH: Effect kind '%s' (reason: '%s') has unused parameter '%s'. This parameter was not read by the C++ compilation code. Check if the parameter name is correct.", effect_kind, reason, key));
		}
	}
}

EffectRecord compile_effect(const Dictionary &effect) {
	EffectRecord compiled;
	String kind_str = String(effect.get("kind", ""));

	StringName kind;
	Dictionary params = Dictionary(effect.get("params", Dictionary()));
	ParamTracker tracker(params);
	compiled.windup = double(tracker.get("windup_override", -1.0));
	if (kind_str == "multi_target") {
		kind = sn_multi_target();
	} else if (kind_str == "multi_effect") {
		kind = sn_multi_effect();
	} else if (kind_str == "damage") {
		kind = sn_damage();
	} else if (kind_str == "projectile") {
		kind = sn_projectile();
	} else if (kind_str == "stun") {
		kind = sn_stun();
	} else if (kind_str == "shield") {
		kind = sn_shield();
	} else if (kind_str == "heal") {
		kind = sn_heal();
	} else if (kind_str == "aoe_taunt") {
		kind = sn_aoe_taunt();
	} else if (kind_str == "aoe_damage") {
		kind = sn_aoe_damage();
	} else if (kind_str == "damage_threshold_trigger") {
		kind = sn_damage_threshold_trigger();
	} else if (kind_str == "damage_over_time") {
		kind = sn_damage_over_time();
	} else if (kind_str == "heal_over_time") {
		kind = sn_heal_over_time();
	} else if (kind_str == "aoe_damage_over_time") {
		kind = sn_aoe_damage_over_time();
	} else if (kind_str == "aoe_heal_over_time") {
		kind = sn_aoe_heal_over_time();
	} else if (kind_str == "mana_regen") {
		kind = sn_mana_regen();
	} else if (kind_str == "post_damage_mana_gain") {
		kind = sn_post_damage_mana_gain();
	} else if (kind_str == "damage_based_heal") {
		kind = sn_damage_based_heal();
	} else if (kind_str == "damage_based_shield") {
		kind = sn_damage_based_shield();
	} else if (kind_str == "consume_stacks_damage") {
		kind = sn_consume_stacks_damage();
	} else if (kind_str == "consume_stacks_heal") {
		kind = sn_consume_stacks_heal();
	} else if (kind_str == "consume_stacks_shield") {
		kind = sn_consume_stacks_shield();
	} else if (kind_str == "set_stacks") {
		kind = sn_set_stacks();
	} else if (kind_str == "mana_restore_on_hit") {
		kind = sn_mana_restore_on_hit();
	} else if (kind_str == "drain_target_mana_on_hit") {
		kind = sn_drain_target_mana_on_hit();
	} else if (kind_str == "every_n_attacks_stun") {
		kind = sn_every_n_attacks_stun();
	} else if (kind_str == "self_dash") {
		kind = sn_self_dash();
	} else if (kind_str == "auto_dodge") {
		kind = sn_auto_dodge();
	} else if (kind_str == "constant_multiplier") {
		kind = sn_constant_multiplier();
	} else if (kind_str == "hp_threshold_damage_multiplier") {
		kind = sn_hp_threshold_damage_multiplier();
	} else if (kind_str == "distance_threshold_multiplier") {
		kind = sn_distance_threshold_multiplier();
	} else if (kind_str == "slow") {
		kind = sn_slow();
	} else if (kind_str == "root") {
		kind = sn_root();
	} else if (kind_str == "silence") {
		kind = sn_silence();
	} else if (kind_str == "disarm") {
		kind = sn_disarm();
	} else if (kind_str == "stealth") {
		kind = sn_stealth();
	} else if (kind_str == "knockback") {
		kind = sn_knockback();
	} else if (kind_str == "reflect") {
		kind = sn_reflect();
	} else if (kind_str == "aoe_slow") {
		kind = sn_aoe_slow();
	} else if (kind_str == "aoe_root") {
		kind = sn_aoe_root();
	} else if (kind_str == "aoe_silence") {
		kind = sn_aoe_silence();
	} else if (kind_str == "aoe_disarm") {
		kind = sn_aoe_disarm();
	} else if (kind_str == "aoe_knockback") {
		kind = sn_aoe_knockback();
	} else if (kind_str == "aoe_reflect") {
		kind = sn_aoe_reflect();
	} else if (kind_str == "aoe_stun") {
		kind = sn_aoe_stun();
	} else if (kind_str == "reflect_damage") {
		kind = sn_reflect_damage();
	} else if (kind_str == "knockback_shield") {
		kind = sn_knockback_shield();
	} else if (kind_str == "target_status_multiplier") {
		kind = sn_target_status_multiplier();
	} else if (kind_str == "stat_modifier") {
		kind = sn_stat_modifier();
	} else {
		kind = StringName(kind_str);
	}
	compiled.opcode = opcode_for_kind(kind);
	if (kind == sn_multi_effect()) {
		Variant effects_value = tracker.get("effects", Variant());
		Array effects = effects_value.get_type() == Variant::ARRAY ? Array(effects_value) : Array();
		compiled.children = compile_effect_array(effects);
		return compiled;
	}
	internal::fill_compiled_kind_fields(compiled, kind, tracker, params);
	if (kind == sn_stat_modifier() && compiled.opcode == EFFECT_OPCODE_UNKNOWN) {
		tracker.reason = compiled.reason;
		tracker.report_unused(kind_str);
		return compiled;
	}

	compiled.requires_result_from = StringName(String(tracker.get("requires_result_from", "")));
	compiled.requires_field = StringName(String(tracker.get("requires_field", "")));
	compiled.requires_value = tracker.get("requires_value", Variant());
	compiled.requires_target_status = StringName(String(tracker.get("requires_target_status", "")));
	compiled.status_target = StringName(String(tracker.get("status_target", "target")));

	if (!compiled.status_target.is_empty() && compiled.status_target != sn_source() && compiled.status_target != sn_target()) {
		UtilityFunctions::push_error(vformat("Invalid status_target '%s' in effect parameters. Valid values: 'source', 'target'", String(compiled.status_target)));
		compiled.status_target = sn_target();
	}

	compiled.on_tick_interval = double(tracker.get("on_tick_interval", 1.0));
	compiled.on_tick_interval = Math::max(compiled.on_tick_interval, DEFAULT_TICK_RATE);

	tracker.reason = compiled.reason;
	tracker.report_unused(kind_str);

	return compiled;
}

std::vector<EffectRecord> compile_effect_array(const Array &effects) {
	std::vector<EffectRecord> compiled;
	compiled.reserve(size_t(effects.size()));
	for (int64_t index = 0; index < effects.size(); ++index) {
		Variant effect = effects[index];
		if (effect.get_type() == Variant::DICTIONARY) {
			compiled.push_back(compile_effect(Dictionary(effect)));
		}
	}
	return compiled;
}

} // namespace sim::effects::compile
