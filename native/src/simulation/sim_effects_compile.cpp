#include "sim_effects_compile.hpp"

#include "sim_constants.hpp"
#include "stat_definitions.hpp"
#include "sim_effect_kinds.inl.hpp"
#include "sim_effects_compile_internal.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::compile {

using namespace sim::effect_kinds;

int64_t opcode_for_kind(const StringName &kind) {
	if (kind == sn_multi_target()) {
		return EFFECT_OPCODE_MULTI_TARGET;
	}
	if (kind == sn_multi_effect()) {
		return EFFECT_OPCODE_MULTI_EFFECT;
	}
	if (kind == sn_damage()) {
		return EFFECT_OPCODE_DAMAGE;
	}
	if (kind == sn_projectile()) {
		return EFFECT_OPCODE_PROJECTILE;
	}
	if (kind == sn_stun()) {
		return EFFECT_OPCODE_STUN;
	}
	if (kind == sn_shield()) {
		return EFFECT_OPCODE_SHIELD;
	}
	if (kind == sn_heal()) {
		return EFFECT_OPCODE_HEAL;
	}
	if (kind == sn_aoe_taunt()) {
		return EFFECT_OPCODE_AOE_TAUNT;
	}
	if (kind == sn_aoe_damage()) {
		return EFFECT_OPCODE_AOE_DAMAGE;
	}
	if (kind == sn_damage_threshold_trigger()) {
		return EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER;
	}
	if (kind == sn_damage_over_time()) {
		return EFFECT_OPCODE_DAMAGE_OVER_TIME;
	}
	if (kind == sn_heal_over_time()) {
		return EFFECT_OPCODE_HEAL_OVER_TIME;
	}
	if (kind == sn_aoe_damage_over_time()) {
		return EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME;
	}
	if (kind == sn_aoe_heal_over_time()) {
		return EFFECT_OPCODE_AOE_HEAL_OVER_TIME;
	}
	if (kind == sn_mana_regen()) {
		return EFFECT_OPCODE_MANA_REGEN;
	}
	if (kind == sn_post_damage_mana_gain()) {
		return EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN;
	}
	if (kind == sn_damage_based_heal()) {
		return EFFECT_OPCODE_DAMAGE_BASED_HEAL;
	}
	if (kind == sn_damage_based_shield()) {
		return EFFECT_OPCODE_DAMAGE_BASED_SHIELD;
	}
	if (kind == sn_consume_stacks_damage()) {
		return EFFECT_OPCODE_CONSUME_STACKS_DAMAGE;
	}
	if (kind == sn_consume_stacks_heal()) {
		return EFFECT_OPCODE_CONSUME_STACKS_HEAL;
	}
	if (kind == sn_consume_stacks_shield()) {
		return EFFECT_OPCODE_CONSUME_STACKS_SHIELD;
	}
	if (kind == sn_set_stacks()) {
		return EFFECT_OPCODE_SET_STACKS;
	}
	if (kind == sn_channel()) {
		return EFFECT_OPCODE_CHANNEL;
	}
	if (kind == sn_mana_restore_on_hit()) {
		return EFFECT_OPCODE_MANA_RESTORE_ON_HIT;
	}
	if (kind == sn_drain_target_mana_on_hit()) {
		return EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT;
	}
	if (kind == sn_every_n_attacks_stun()) {
		return EFFECT_OPCODE_EVERY_N_ATTACKS_STUN;
	}
	if (kind == sn_self_dash()) {
		return EFFECT_OPCODE_SELF_DASH;
	}
	if (kind == sn_auto_dodge()) {
		return EFFECT_OPCODE_AUTO_DODGE;
	}
	if (kind == sn_constant_multiplier()) {
		return EFFECT_OPCODE_CONSTANT_MULTIPLIER;
	}
	if (kind == sn_hp_threshold_damage_multiplier()) {
		return EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER;
	}
	if (kind == sn_distance_threshold_multiplier()) {
		return EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER;
	}
	// New effect kinds
	if (kind == sn_slow()) {
		return EFFECT_OPCODE_SLOW;
	}
	if (kind == sn_root()) {
		return EFFECT_OPCODE_ROOT;
	}
	if (kind == sn_silence()) {
		return EFFECT_OPCODE_SILENCE;
	}
	if (kind == sn_disarm()) {
		return EFFECT_OPCODE_DISARM;
	}
	if (kind == sn_stealth()) {
		return EFFECT_OPCODE_STEALTH;
	}
	if (kind == sn_knockback()) {
		return EFFECT_OPCODE_KNOCKBACK;
	}
	if (kind == sn_reflect()) {
		return EFFECT_OPCODE_REFLECT;
	}
	if (kind == sn_aoe_slow()) {
		return EFFECT_OPCODE_AOE_SLOW;
	}
	if (kind == sn_aoe_root()) {
		return EFFECT_OPCODE_AOE_ROOT;
	}
	if (kind == sn_aoe_silence()) {
		return EFFECT_OPCODE_AOE_SILENCE;
	}
	if (kind == sn_aoe_disarm()) {
		return EFFECT_OPCODE_AOE_DISARM;
	}
	if (kind == sn_aoe_knockback()) {
		return EFFECT_OPCODE_AOE_KNOCKBACK;
	}
	if (kind == sn_aoe_reflect()) {
		return EFFECT_OPCODE_AOE_REFLECT;
	}
	if (kind == sn_aoe_stun()) {
		return EFFECT_OPCODE_AOE_STUN;
	}
	if (kind == sn_reflect_damage()) {
		return EFFECT_OPCODE_REFLECT_DAMAGE;
	}
	if (kind == sn_redirect_damage()) {
		return EFFECT_OPCODE_REDIRECT_DAMAGE;
	}
	if (kind == sn_summon_ally()) {
		return EFFECT_OPCODE_SUMMON_ALLY;
	}
	if (kind == sn_knockback_shield()) {
		return EFFECT_OPCODE_KNOCKBACK_SHIELD;
	}
	if (kind == sn_target_status_multiplier()) {
		return EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER;
	}
	if (kind == sn_stat_modifier()) {
		return EFFECT_OPCODE_STAT_MODIFIER;
	}
	// DEBUG: Log unrecognized effect kinds
	UtilityFunctions::push_error(vformat("[DEBUG] _opcode_for_kind: unrecognized effect kind '%s', returning EFFECT_OPCODE_UNKNOWN", String(kind)));
	return EFFECT_OPCODE_UNKNOWN;
}

const StringName &kind_for_opcode(int64_t opcode) {
	static const StringName empty_kind;
	switch (opcode) {
		case EFFECT_OPCODE_MULTI_EFFECT:
			return sn_multi_effect();
		case EFFECT_OPCODE_DAMAGE:
			return sn_damage();
		case EFFECT_OPCODE_PROJECTILE:
			return sn_projectile();
		case EFFECT_OPCODE_STUN:
			return sn_stun();
		case EFFECT_OPCODE_SHIELD:
			return sn_shield();
		case EFFECT_OPCODE_HEAL:
			return sn_heal();
		case EFFECT_OPCODE_AOE_TAUNT:
			return sn_aoe_taunt();
		case EFFECT_OPCODE_AOE_DAMAGE:
			return sn_aoe_damage();
		case EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER:
			return sn_damage_threshold_trigger();
		case EFFECT_OPCODE_DAMAGE_OVER_TIME:
			return sn_damage_over_time();
		case EFFECT_OPCODE_HEAL_OVER_TIME:
			return sn_heal_over_time();
		case EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME:
			return sn_aoe_damage_over_time();
		case EFFECT_OPCODE_AOE_HEAL_OVER_TIME:
			return sn_aoe_heal_over_time();
		case EFFECT_OPCODE_MANA_REGEN:
			return sn_mana_regen();
		case EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN:
			return sn_post_damage_mana_gain();
		case EFFECT_OPCODE_DAMAGE_BASED_HEAL:
			return sn_damage_based_heal();
		case EFFECT_OPCODE_DAMAGE_BASED_SHIELD:
			return sn_damage_based_shield();
		case EFFECT_OPCODE_CONSUME_STACKS_DAMAGE:
			return sn_consume_stacks_damage();
		case EFFECT_OPCODE_CONSUME_STACKS_HEAL:
			return sn_consume_stacks_heal();
		case EFFECT_OPCODE_CONSUME_STACKS_SHIELD:
			return sn_consume_stacks_shield();
		case EFFECT_OPCODE_SET_STACKS:
			return sn_set_stacks();
		case EFFECT_OPCODE_CHANNEL:
			return sn_channel();
		case EFFECT_OPCODE_MANA_RESTORE_ON_HIT:
			return sn_mana_restore_on_hit();
		case EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT:
			return sn_drain_target_mana_on_hit();
		case EFFECT_OPCODE_EVERY_N_ATTACKS_STUN:
			return sn_every_n_attacks_stun();
		case EFFECT_OPCODE_SELF_DASH:
			return sn_self_dash();
		case EFFECT_OPCODE_AUTO_DODGE:
			return sn_auto_dodge();
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER:
			return sn_constant_multiplier();
		case EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER:
			return sn_hp_threshold_damage_multiplier();
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
			return sn_distance_threshold_multiplier();
		// New effect opcodes
		case EFFECT_OPCODE_SLOW:
			return sn_slow();
		case EFFECT_OPCODE_ROOT:
			return sn_root();
		case EFFECT_OPCODE_SILENCE:
			return sn_silence();
		case EFFECT_OPCODE_DISARM:
			return sn_disarm();
		case EFFECT_OPCODE_STEALTH:
			return sn_stealth();
		case EFFECT_OPCODE_KNOCKBACK:
			return sn_knockback();
		case EFFECT_OPCODE_REFLECT:
			return sn_reflect();
		case EFFECT_OPCODE_AOE_SLOW:
			return sn_aoe_slow();
		case EFFECT_OPCODE_AOE_ROOT:
			return sn_aoe_root();
		case EFFECT_OPCODE_AOE_SILENCE:
			return sn_aoe_silence();
		case EFFECT_OPCODE_AOE_DISARM:
			return sn_aoe_disarm();
		case EFFECT_OPCODE_AOE_KNOCKBACK:
			return sn_aoe_knockback();
		case EFFECT_OPCODE_AOE_REFLECT:
			return sn_aoe_reflect();
		case EFFECT_OPCODE_AOE_STUN:
			return sn_aoe_stun();
		case EFFECT_OPCODE_REFLECT_DAMAGE:
			return sn_reflect_damage();
		case EFFECT_OPCODE_REDIRECT_DAMAGE:
			return sn_redirect_damage();
		case EFFECT_OPCODE_SUMMON_ALLY:
			return sn_summon_ally();
		case EFFECT_OPCODE_KNOCKBACK_SHIELD:
			return sn_knockback_shield();
		case EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER:
			return sn_target_status_multiplier();
		case EFFECT_OPCODE_STAT_MODIFIER:
			return sn_stat_modifier();
		case EFFECT_OPCODE_MULTI_TARGET:
			return sn_multi_target();
		default:
			return empty_kind;
	}
}

void ParamTracker::report_unused(const String &effect_kind) const {
	// Report parameters that were provided but never accessed
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
	
	// Handle conditional execution parameters
	compiled.requires_result_from = StringName(String(tracker.get("requires_result_from", "")));
	compiled.requires_field = StringName(String(tracker.get("requires_field", "")));
	compiled.requires_value = tracker.get("requires_value", Variant());
	compiled.requires_target_status = StringName(String(tracker.get("requires_target_status", "")));
	compiled.status_target = StringName(String(tracker.get("status_target", "target")));
	
	// Validate status_target parameter
	if (!compiled.status_target.is_empty() && compiled.status_target != sn_source() && compiled.status_target != sn_target()) {
		UtilityFunctions::push_error(vformat("Invalid status_target '%s' in effect parameters. Valid values: 'source', 'target'", String(compiled.status_target)));
		compiled.status_target = sn_target();  // Default to target on error
	}
	
	// Handle on_tick_interval parameter for timing control
	compiled.on_tick_interval = double(tracker.get("on_tick_interval", 1.0));
	// Validate minimum on_tick_interval (must be at least game tick rate)
	compiled.on_tick_interval = Math::max(compiled.on_tick_interval, DEFAULT_TICK_RATE);
	
	// Set reason for error reporting
	tracker.reason = compiled.reason;
	
	// Report any unused parameters to catch mismatches
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
