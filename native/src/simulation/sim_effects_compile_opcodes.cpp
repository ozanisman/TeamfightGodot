#include "sim_effects_compile.hpp"

#include "sim_effect_kinds.inl.hpp"

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
	if (kind == sn_damage_threshold_trigger()) {
		return EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER;
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
	if (kind == sn_reflect_damage()) {
		return EFFECT_OPCODE_REFLECT_DAMAGE;
	}
	if (kind == sn_target_status_multiplier()) {
		return EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER;
	}
	if (kind == sn_stat_modifier()) {
		return EFFECT_OPCODE_STAT_MODIFIER;
	}
	if (kind == sn_damage_over_time()) {
		return EFFECT_OPCODE_DAMAGE_OVER_TIME;
	}
	if (kind == sn_cleanse_dots()) {
		return EFFECT_OPCODE_CLEANSE_DOTS;
	}
	if (kind == sn_cleanse_hots()) {
		return EFFECT_OPCODE_CLEANSE_HOTS;
	}
	if (kind == sn_consume_stacks_damage()) {
		return EFFECT_OPCODE_CONSUME_STACKS_DAMAGE;
	}
	if (kind == sn_redirect_damage()) {
		return EFFECT_OPCODE_REDIRECT_DAMAGE;
	}
	if (kind == sn_summon_ally()) {
		return EFFECT_OPCODE_SUMMON_ALLY;
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
	if (kind == sn_mana_regen()) {
		return EFFECT_OPCODE_MANA_REGEN;
	}
	if (kind == sn_post_damage_mana_gain()) {
		return EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN;
	}
	if (kind == sn_damage_based_heal()) {
		return EFFECT_OPCODE_DAMAGE_BASED_HEAL;
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
	if (kind == sn_knockback()) {
		return EFFECT_OPCODE_KNOCKBACK;
	}
	if (kind == sn_reflect()) {
		return EFFECT_OPCODE_REFLECT;
	}
	if (kind == sn_stealth()) {
		return EFFECT_OPCODE_STEALTH;
	}
	if (kind == sn_heal_over_time()) {
		return EFFECT_OPCODE_HEAL_OVER_TIME;
	}
	if (kind == sn_damage_based_shield()) {
		return EFFECT_OPCODE_DAMAGE_BASED_SHIELD;
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
	if (kind == sn_aoe_taunt()) {
		return EFFECT_OPCODE_AOE_TAUNT;
	}
	if (kind == sn_aoe_damage()) {
		return EFFECT_OPCODE_AOE_DAMAGE;
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
	if (kind == sn_aoe_damage_over_time()) {
		return EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME;
	}
	if (kind == sn_aoe_heal_over_time()) {
		return EFFECT_OPCODE_AOE_HEAL_OVER_TIME;
	}
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
		case EFFECT_OPCODE_MULTI_TARGET:
			return sn_multi_target();
		case EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER:
			return sn_damage_threshold_trigger();
		case EFFECT_OPCODE_AUTO_DODGE:
			return sn_auto_dodge();
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER:
			return sn_constant_multiplier();
		case EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER:
			return sn_hp_threshold_damage_multiplier();
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
			return sn_distance_threshold_multiplier();
		case EFFECT_OPCODE_REFLECT_DAMAGE:
			return sn_reflect_damage();
		case EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER:
			return sn_target_status_multiplier();
		case EFFECT_OPCODE_STAT_MODIFIER:
			return sn_stat_modifier();
		case EFFECT_OPCODE_DAMAGE_OVER_TIME:
			return sn_damage_over_time();
		case EFFECT_OPCODE_CLEANSE_DOTS:
			return sn_cleanse_dots();
		case EFFECT_OPCODE_CLEANSE_HOTS:
			return sn_cleanse_hots();
		case EFFECT_OPCODE_CONSUME_STACKS_DAMAGE:
			return sn_consume_stacks_damage();
		case EFFECT_OPCODE_REDIRECT_DAMAGE:
			return sn_redirect_damage();
		case EFFECT_OPCODE_SUMMON_ALLY:
			return sn_summon_ally();
		case EFFECT_OPCODE_STUN:
			return sn_stun();
		case EFFECT_OPCODE_SHIELD:
			return sn_shield();
		case EFFECT_OPCODE_HEAL:
			return sn_heal();
		case EFFECT_OPCODE_MANA_REGEN:
			return sn_mana_regen();
		case EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN:
			return sn_post_damage_mana_gain();
		case EFFECT_OPCODE_DAMAGE_BASED_HEAL:
			return sn_damage_based_heal();
		case EFFECT_OPCODE_MANA_RESTORE_ON_HIT:
			return sn_mana_restore_on_hit();
		case EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT:
			return sn_drain_target_mana_on_hit();
		case EFFECT_OPCODE_EVERY_N_ATTACKS_STUN:
			return sn_every_n_attacks_stun();
		case EFFECT_OPCODE_SELF_DASH:
			return sn_self_dash();
		case EFFECT_OPCODE_SLOW:
			return sn_slow();
		case EFFECT_OPCODE_ROOT:
			return sn_root();
		case EFFECT_OPCODE_SILENCE:
			return sn_silence();
		case EFFECT_OPCODE_DISARM:
			return sn_disarm();
		case EFFECT_OPCODE_KNOCKBACK:
			return sn_knockback();
		case EFFECT_OPCODE_REFLECT:
			return sn_reflect();
		case EFFECT_OPCODE_STEALTH:
			return sn_stealth();
		case EFFECT_OPCODE_HEAL_OVER_TIME:
			return sn_heal_over_time();
		case EFFECT_OPCODE_DAMAGE_BASED_SHIELD:
			return sn_damage_based_shield();
		case EFFECT_OPCODE_CONSUME_STACKS_HEAL:
			return sn_consume_stacks_heal();
		case EFFECT_OPCODE_CONSUME_STACKS_SHIELD:
			return sn_consume_stacks_shield();
		case EFFECT_OPCODE_SET_STACKS:
			return sn_set_stacks();
		case EFFECT_OPCODE_CHANNEL:
			return sn_channel();
		case EFFECT_OPCODE_AOE_TAUNT:
			return sn_aoe_taunt();
		case EFFECT_OPCODE_AOE_DAMAGE:
			return sn_aoe_damage();
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
		case EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME:
			return sn_aoe_damage_over_time();
		case EFFECT_OPCODE_AOE_HEAL_OVER_TIME:
			return sn_aoe_heal_over_time();
		default:
			return empty_kind;
	}
}

} // namespace sim::effects::compile
