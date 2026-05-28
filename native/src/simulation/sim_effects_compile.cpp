#include "sim_effects_compile.hpp"

#include "sim_constants.hpp"
#include "stat_definitions.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::compile {
namespace {

inline const StringName &sn_player() {
	static const StringName s("player");
	return s;
}
inline const StringName &sn_enemy() {
	static const StringName s("enemy");
	return s;
}
inline const StringName &sn_ally() {
	static const StringName s("ally");
	return s;
}
inline const StringName &sn_tank() {
	static const StringName s("tank");
	return s;
}
inline const StringName &sn_fighter() {
	static const StringName s("fighter");
	return s;
}
inline const StringName &sn_marksman() {
	static const StringName s("marksman");
	return s;
}
inline const StringName &sn_mage() {
	static const StringName s("mage");
	return s;
}
inline const StringName &sn_support() {
	static const StringName s("support");
	return s;
}
inline const StringName &sn_assassin() {
	static const StringName s("assassin");
	return s;
}
inline const StringName &sn_auto() {
	static const StringName s("auto");
	return s;
}
inline const StringName &sn_multi_effect() {
	static const StringName s("multi_effect");
	return s;
}
inline const StringName &sn_damage() {
	static const StringName s("damage");
	return s;
}
inline const StringName &sn_projectile() {
	static const StringName s("projectile");
	return s;
}
inline const StringName &sn_stun() {
	static const StringName s("stun");
	return s;
}
inline const StringName &sn_shield() {
	static const StringName s("shield");
	return s;
}
inline const StringName &sn_heal() {
	static const StringName s("heal");
	return s;
}
inline const StringName &sn_aoe_taunt() {
	static const StringName s("aoe_taunt");
	return s;
}
inline const StringName &sn_aoe_damage() {
	static const StringName s("aoe_damage");
	return s;
}
inline const StringName &sn_damage_threshold_trigger() {
	static const StringName s("damage_threshold_trigger");
	return s;
}
inline const StringName &sn_damage_over_time() {
	static const StringName s("damage_over_time");
	return s;
}
inline const StringName &sn_heal_over_time() {
	static const StringName s("heal_over_time");
	return s;
}
inline const StringName &sn_aoe_damage_over_time() {
	static const StringName s("aoe_damage_over_time");
	return s;
}
inline const StringName &sn_aoe_heal_over_time() {
	static const StringName s("aoe_heal_over_time");
	return s;
}
inline const StringName &sn_mana_regen() {
	static const StringName s("mana_regen");
	return s;
}
inline const StringName &sn_post_damage_mana_gain() {
	static const StringName s("post_damage_mana_gain");
	return s;
}
inline const StringName &sn_damage_based_heal() {
	static const StringName s("damage_based_heal");
	return s;
}
inline const StringName &sn_damage_based_shield() {
	static const StringName s("damage_based_shield");
	return s;
}
inline const StringName &sn_consume_stacks_damage() {
	static const StringName s("consume_stacks_damage");
	return s;
}
inline const StringName &sn_consume_stacks_heal() {
	static const StringName s("consume_stacks_heal");
	return s;
}
inline const StringName &sn_consume_stacks_shield() {
	static const StringName s("consume_stacks_shield");
	return s;
}
inline const StringName &sn_set_stacks() {
	static const StringName s("set_stacks");
	return s;
}
inline const StringName &sn_channel() {
	static const StringName s("channel");
	return s;
}
inline const StringName &sn_mana_restore_on_hit() {
	static const StringName s("mana_restore_on_hit");
	return s;
}
inline const StringName &sn_drain_target_mana_on_hit() {
	static const StringName s("drain_target_mana_on_hit");
	return s;
}
inline const StringName &sn_every_n_attacks_stun() {
	static const StringName s("every_n_attacks_stun");
	return s;
}
inline const StringName &sn_self_dash() {
	static const StringName s("self_dash");
	return s;
}
inline const StringName &sn_auto_dodge() {
	static const StringName s("auto_dodge");
	return s;
}
inline const StringName &sn_constant_multiplier() {
	static const StringName s("constant_multiplier");
	return s;
}
inline const StringName &sn_hp_threshold_damage_multiplier() {
	static const StringName s("hp_threshold_damage_multiplier");
	return s;
}
inline const StringName &sn_distance_threshold_multiplier() {
	static const StringName s("distance_threshold_multiplier");
	return s;
}
inline const StringName &sn_slow() {
	static const StringName s("slow");
	return s;
}
inline const StringName &sn_root() {
	static const StringName s("root");
	return s;
}
inline const StringName &sn_silence() {
	static const StringName s("silence");
	return s;
}
inline const StringName &sn_disarm() {
	static const StringName s("disarm");
	return s;
}
inline const StringName &sn_stealth() {
	static const StringName s("stealth");
	return s;
}
inline const StringName &sn_knockback() {
	static const StringName s("knockback");
	return s;
}
inline const StringName &sn_reflect() {
	static const StringName s("reflect");
	return s;
}
inline const StringName &sn_aoe_slow() {
	static const StringName s("aoe_slow");
	return s;
}
inline const StringName &sn_aoe_root() {
	static const StringName s("aoe_root");
	return s;
}
inline const StringName &sn_aoe_silence() {
	static const StringName s("aoe_silence");
	return s;
}
inline const StringName &sn_aoe_disarm() {
	static const StringName s("aoe_disarm");
	return s;
}
inline const StringName &sn_aoe_knockback() {
	static const StringName s("aoe_knockback");
	return s;
}
inline const StringName &sn_aoe_reflect() {
	static const StringName s("aoe_reflect");
	return s;
}
inline const StringName &sn_aoe_stun() {
	static const StringName s("aoe_stun");
	return s;
}
inline const StringName &sn_reflect_damage() {
	static const StringName s("reflect_damage");
	return s;
}
inline const StringName &sn_redirect_damage() {
	static const StringName s("redirect_damage");
	return s;
}
inline const StringName &sn_summon_ally() {
	static const StringName s("summon_ally");
	return s;
}
inline const StringName &sn_knockback_shield() {
	static const StringName s("knockback_shield");
	return s;
}
inline const StringName &sn_target_status_multiplier() {
	static const StringName s("target_status_multiplier");
	return s;
}
inline const StringName &sn_stat_modifier() {
	static const StringName s("stat_modifier");
	return s;
}
inline const StringName &sn_multi_target() {
	static const StringName s("multi_target");
	return s;
}
inline const StringName &sn_ability() {
	static const StringName s("ability");
	return s;
}
inline const StringName &sn_ultimate() {
	static const StringName s("ultimate");
	return s;
}
inline const StringName &sn_source() {
	static const StringName s("source");
	return s;
}
inline const StringName &sn_target() {
	static const StringName s("target");
	return s;
}
inline const StringName &sn_physical() {
	static const StringName s("physical");
	return s;
}
inline const StringName &sn_magic() {
	static const StringName s("magic");
	return s;
}
inline const StringName &sn_true() {
	static const StringName s("true");
	return s;
}
inline const StringName &sn_passive() {
	static const StringName s("passive");
	return s;
}
inline const StringName &sn_on_attack() {
	static const StringName s("on_attack");
	return s;
}
inline const StringName &sn_on_defense() {
	static const StringName s("on_defense");
	return s;
}
inline const StringName &sn_on_ally_defense() {
	static const StringName s("on_ally_defense");
	return s;
}
inline const StringName &sn_on_tick() {
	static const StringName s("on_tick");
	return s;
}
inline const StringName &sn_post_attack() {
	static const StringName s("post_attack");
	return s;
}
inline const StringName &sn_post_take_damage() {
	static const StringName s("post_take_damage");
	return s;
}
inline const StringName &sn_on_ability() {
	static const StringName s("on_ability");
	return s;
}
inline const StringName &sn_on_ultimate() {
	static const StringName s("on_ultimate");
	return s;
}
inline const StringName &sn_post_heal() {
	static const StringName s("post_heal");
	return s;
}
inline const StringName &sn_on_takedown() {
	static const StringName s("on_takedown");
	return s;
}
inline const StringName &sn_cast_start() {
	static const StringName s("cast_start");
	return s;
}
inline const StringName &sn_success() {
	static const StringName s("success");
	return s;
}
inline const StringName &sn_condition_failed() {
	static const StringName s("condition_failed");
	return s;
}
inline const StringName &sn_damage_dealt() {
	static const StringName s("damage_dealt");
	return s;
}
inline const StringName &sn_target_killed() {
	static const StringName s("target_killed");
	return s;
}
inline const StringName &sn_projectile_created() {
	static const StringName s("projectile_created");
	return s;
}
inline const StringName &sn_stun_applied() {
	static const StringName s("stun_applied");
	return s;
}
inline const StringName &sn_shield_applied() {
	static const StringName s("shield_applied");
	return s;
}
inline const StringName &sn_heal_applied() {
	static const StringName s("heal_applied");
	return s;
}
inline const StringName &sn_taunt_applied() {
	static const StringName s("taunt_applied");
	return s;
}
inline const StringName &sn_splash_applied() {
	static const StringName s("splash_applied");
	return s;
}
inline const StringName &sn_splash_triggered() {
	static const StringName s("splash_triggered");
	return s;
}
inline const StringName &sn_knockback_applied() {
	static const StringName s("knockback_applied");
	return s;
}
inline const StringName &sn_reached_target() {
	static const StringName s("reached_target");
	return s;
}
inline const StringName &sn_amount() {
	static const StringName s("amount");
	return s;
}
inline const StringName &sn_duration() {
	static const StringName s("duration");
	return s;
}
inline const StringName &sn_radius() {
	static const StringName s("radius");
	return s;
}
inline const StringName &sn_mana_restored() {
	static const StringName s("mana_restored");
	return s;
}
inline const StringName &sn_mana_drained() {
	static const StringName s("mana_drained");
	return s;
}
inline const StringName &sn_distance_traveled() {
	static const StringName s("distance_traveled");
	return s;
}
constexpr size_t EFFECT_BUCKET_ON_ATTACK = 0;
constexpr size_t EFFECT_BUCKET_ON_DEFENSE = 1;
constexpr size_t EFFECT_BUCKET_ON_ALLY_DEFENSE = 2;
constexpr size_t EFFECT_BUCKET_ON_TICK = 3;
constexpr size_t EFFECT_BUCKET_POST_ATTACK = 4;
constexpr size_t EFFECT_BUCKET_POST_TAKE_DAMAGE = 5;
constexpr size_t EFFECT_BUCKET_ON_ABILITY = 6;
constexpr size_t EFFECT_BUCKET_ON_ULTIMATE = 7;
constexpr size_t EFFECT_BUCKET_POST_HEAL = 8;
constexpr size_t EFFECT_BUCKET_ON_TAKEDOWN = 9;

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

} // namespace

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
	if (kind == sn_constant_multiplier()) {
		compiled.scalar0 = double(tracker.get("multiplier", 1.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_hp_threshold_damage_multiplier()) {
		compiled.scalar0 = double(tracker.get("above_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("below_hp_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("multiplier", 1.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_distance_threshold_multiplier()) {
		compiled.scalar0 = double(tracker.get("min_distance", 0.0));
		compiled.scalar1 = double(tracker.get("multiplier", 1.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_damage()) {
		compiled.scalar0 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar3 = bool(tracker.get("trigger_on_hit", false)) ? 1.0 : 0.0;
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
	} else if (kind == sn_projectile()) {
		// Use -1.0 as sentinel for "use unit's projectile_speed/radius stat" when override is null.
		// Python parity: speed_override=None â†’ unit.stats.projectile_speed, radius_override=None â†’ unit.stats.projectile_radius.
		Variant speed_v = tracker.get("speed_override", Variant());
		compiled.scalar0 = (speed_v.get_type() == Variant::NIL) ? -1.0 : double(speed_v);
		Variant radius_v = tracker.get("radius_override", Variant());
		compiled.scalar1 = (radius_v.get_type() == Variant::NIL) ? -1.0 : double(radius_v);
		compiled.scalar2 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar3 = double(tracker.get("stun_duration", 0.0));
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
	} else if (kind == sn_stun()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_shield()) {
		compiled.scalar0 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("damage_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("flat_amount", 0.0));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_heal()) {
		compiled.scalar0 = double(tracker.get("max_hp_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("current_hp_ratio", 0.0));
		compiled.scalar2 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar3 = double(tracker.get("missing_hp_ratio", 0.0));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;  // target_self parameter
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_aoe_taunt()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", "")); // INCONSISTENT: other AOE effects use descriptive defaults like "AOE Slow"
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
	} else if (kind == sn_aoe_damage()) {
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
	} else if (kind == sn_damage_over_time()) {
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
		compiled.int1 = int64_t(tracker.get("max_stacks", 1));
		String calculation_str = String(tracker.get("calculation", "fixed"));
		compiled.int2 = (calculation_str == "dynamic") ? 1 : 0;  // 0=fixed, 1=dynamic
	} else if (kind == sn_heal_over_time()) {
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
		compiled.int1 = int64_t(tracker.get("max_stacks", 1));
		compiled.int2 = bool(tracker.get("allow_overheal", false)) ? 1 : 0;
		String calculation_str = String(tracker.get("calculation", "fixed"));
		compiled.int3 = (calculation_str == "dynamic") ? 1 : 0;  // 0=fixed, 1=dynamic
	} else if (kind == sn_aoe_damage_over_time()) {
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
		compiled.int0 = int64_t(tracker.get("max_stacks", 1));
		String calculation_str = String(tracker.get("calculation", "fixed"));
		compiled.int1 = (calculation_str == "dynamic") ? 1 : 0;  // 0=fixed, 1=dynamic
		compiled.int2 = tracker.get("target_self", false) ? 1 : 0;
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
	} else if (kind == sn_aoe_heal_over_time()) {
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
		compiled.int0 = int64_t(tracker.get("max_stacks", 1));
		compiled.int1 = int64_t(tracker.get("duration", 0.0));  // Duration stays in int1
		compiled.int2 = bool(tracker.get("allow_overheal", false)) ? 1 : 0;
		compiled.int3 = tracker.get("target_self", false) ? 1 : 0;
		String calculation_str = String(tracker.get("calculation", "fixed"));
		compiled.int4 = (calculation_str == "dynamic") ? 1 : 0;  // 0=fixed, 1=dynamic
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
	} else if (kind == sn_damage_threshold_trigger()) {
		compiled.scalar0 = double(tracker.get("threshold_multiplier", 1.0));
		Variant nested = tracker.get("effect", Variant());
		if (nested.get_type() == Variant::DICTIONARY) {
			compiled.children.push_back(compile_effect(Dictionary(nested)));
		}
		// INCONSISTENT: no reason string
	} else if (kind == sn_mana_regen()) {
		compiled.scalar0 = double(tracker.get("flat_amount", 0.0));
		compiled.scalar1 = double(tracker.get("max_mana_ratio", 0.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_post_damage_mana_gain()) {
		compiled.scalar0 = double(tracker.get("damage_ratio", 0.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_damage_based_heal()) {
		compiled.scalar0 = double(tracker.get("damage_ratio", 0.0));
		compiled.int0 = tracker.get("use_accumulated_damage", false) ? 1 : 0;
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_damage_based_shield()) {
		compiled.scalar0 = double(tracker.get("damage_ratio", 0.0));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_consume_stacks_damage()) {
		compiled.stat_name = StringName(tracker.get("stat_name", ""));
		compiled.scalar0 = double(tracker.get("base_damage_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("stack_bonus_ratio", 0.0));
		compiled.string0 = String(tracker.get("stacking_mode", "multiplicative"));
		compiled.damage_type = StringName(tracker.get("damage_type", "physical"));
		compiled.string1 = String(tracker.get("stack_reason", ""));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_consume_stacks_heal()) {
		compiled.stat_name = StringName(tracker.get("stat_name", ""));
		compiled.scalar0 = double(tracker.get("base_heal_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("stack_bonus_ratio", 0.0));
		compiled.string0 = String(tracker.get("stacking_mode", "multiplicative"));
		compiled.string1 = String(tracker.get("stack_reason", ""));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_consume_stacks_shield()) {
		compiled.stat_name = StringName(tracker.get("stat_name", ""));
		compiled.scalar0 = double(tracker.get("base_shield_ratio", 0.0));
		compiled.scalar1 = double(tracker.get("stack_bonus_ratio", 0.0));
		compiled.string0 = String(tracker.get("stacking_mode", "multiplicative"));
		compiled.string1 = String(tracker.get("stack_reason", ""));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_set_stacks()) {
		compiled.stat_name = StringName(tracker.get("stat_name", ""));
		compiled.int0 = int64_t(tracker.get("stack_count", 0));
		compiled.int1 = tracker.get("to_max", false);
		compiled.int2 = int64_t(tracker.get("max_stacks", 0));  // Fallback max_stacks when entry doesn't exist
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.scalar1 = double(tracker.get("additive_per_stack", 0.0));  // Fallback when entry doesn't exist
		compiled.scalar2 = double(tracker.get("multiplicative_per_stack", 1.0));  // Fallback when entry doesn't exist
		compiled.string0 = String(tracker.get("reason", ""));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_channel()) {
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
	} else if (kind == sn_mana_restore_on_hit()) {
		compiled.scalar0 = double(tracker.get("flat_amount", 0.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_drain_target_mana_on_hit()) {
		compiled.scalar0 = double(tracker.get("flat_amount", 0.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_every_n_attacks_stun()) {
		compiled.int0 = int64_t(tracker.get("every_n", 0));
		compiled.scalar0 = double(tracker.get("stun_duration", 0.0));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_self_dash()) {
		compiled.scalar0 = double(tracker.get("distance", 0.0));
		Dictionary direction = Dictionary(tracker.get("direction", Dictionary()));
		compiled.scalar1 = double(direction.get("x", 0.0));
		compiled.scalar2 = double(direction.get("y", 0.0));
		// INCONSISTENT: no reason string
	} else if (kind == sn_auto_dodge()) {
		compiled.scalar0 = double(tracker.get("dodge_chance", 0.0));
		compiled.scalar1 = double(tracker.get("on_dodge_multiplier", 0.0));
		compiled.scalar2 = double(tracker.get("on_hit_multiplier", 1.0));
		// INCONSISTENT: no reason string
	}
	else if (kind == sn_slow()) {
		compiled.scalar0 = double(tracker.get("slow_percentage", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", "Slow"));
	} else if (kind == sn_root()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", "Root"));
	} else if (kind == sn_silence()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("block_abilities", true) ? 1 : 0;
		compiled.int1 = tracker.get("block_ultimate", true) ? 1 : 0;
		compiled.reason = String(tracker.get("reason", "Silence"));
	} else if (kind == sn_disarm()) {
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.reason = String(tracker.get("reason", "Disarm"));
	} else if (kind == sn_stealth()) {
		Dictionary break_conditions = Dictionary(tracker.get("break_conditions", Dictionary()));
		compiled.scalar0 = double(tracker.get("duration", 0.0));
		compiled.int0 = bool(break_conditions.get("on_attack", false)) ? 1 : 0;
		compiled.int1 = bool(break_conditions.get("on_ability", false)) ? 1 : 0;
		compiled.int2 = bool(break_conditions.get("on_damage_taken", false)) ? 1 : 0;
		compiled.int3 = tracker.get("target_self", false) ? 1 : 0;
		compiled.reason = String(tracker.get("reason", "Stealth"));
	} else if (kind == sn_knockback()) {
		compiled.scalar0 = double(tracker.get("distance", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("direction", "away_from_source") == "away_from_source" ? 1 : 0;
		compiled.reason = String(tracker.get("reason", "Knockback"));
	} else if (kind == sn_reflect()) {
		compiled.scalar0 = double(tracker.get("reflect_percentage", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("reflect_type", "all") == "all" ? 1 : 0;
		compiled.reason = String(tracker.get("reason", "Reflect"));
	} else if (kind == sn_aoe_slow()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("slow_percentage", 0.0));
		compiled.scalar2 = double(tracker.get("duration", 0.0));
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Slow"));
	} else if (kind == sn_aoe_root()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Root"));
	} else if (kind == sn_aoe_silence()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("block_abilities", true) ? 1 : 0;
		compiled.int1 = tracker.get("block_ultimate", true) ? 1 : 0;
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Silence"));
	} else if (kind == sn_aoe_disarm()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Disarm"));
	} else if (kind == sn_aoe_knockback()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("distance", 0.0));
		compiled.scalar2 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("direction", "away_from_source") == "away_from_source" ? 1 : 0;
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Knockback"));
	} else if (kind == sn_aoe_reflect()) {
		// TODO: Rework reflect_type to be selectable per damage type (physical, magic, true) instead of binary "all" vs "physical"
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("reflect_percentage", 0.0));
		compiled.scalar2 = double(tracker.get("duration", 0.0));
		compiled.int0 = tracker.get("reflect_type", "all") == "all" ? 1 : 0;
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Reflect"));
	} else if (kind == sn_aoe_stun()) {
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		compiled.scalar1 = double(tracker.get("duration", 0.0));
		compiled.aoe_shape_params = parse_aoe_shape_metadata(params, tracker);
		compiled.reason = String(tracker.get("reason", "AOE Stun"));
	} else if (kind == sn_reflect_damage()) {
		// TODO: Rework reflect_type to be selectable per damage type (physical, magic, true) instead of binary "all" vs "physical"
		compiled.scalar0 = double(tracker.get("reflect_percentage", 0.0));
		compiled.int0 = tracker.get("reflect_type", "all") == "all" ? 1 : 0;
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_redirect_damage()) {
		compiled.scalar0 = Math::clamp(double(tracker.get("redirect_ratio", 0.0)), 0.0, 1.0);
		compiled.scalar1 = Math::clamp(double(tracker.get("reduction_ratio", 0.0)), 0.0, 1.0);
		compiled.scalar2 = double(tracker.get("redirect_cap", 0.0));
		compiled.reason = String(tracker.get("reason", ""));
	} else if (kind == sn_summon_ally()) {
		compiled.scalar0 = double(tracker.get("spawn_radius", 0.0));
		// Parse minions array: each entry has minion_id and count
		Array minions_array = tracker.get("minions", Array());
		for (int64_t i = 0; i < minions_array.size(); ++i) {
			Dictionary minion_entry = minions_array[i];
			StringName minion_id = StringName(String(minion_entry.get("minion_id", "")));
			int64_t count = int64_t(minion_entry.get("count", 1));
			// Store minion_id in string0 and count in int0 for each entry
			// We'll use children array to store multiple minion specs
			// Note: opcode not set - children are data containers, not executable sub-effects
			EffectRecord minion_spec;
			minion_spec.string0 = String(minion_id);
			minion_spec.int0 = count;
			compiled.children.push_back(minion_spec);
		}
		compiled.reason = String(tracker.get("reason", "Summon Ally"));
	} else if (kind == sn_knockback_shield()) {
		compiled.scalar0 = double(tracker.get("shield_ratio", 0.0));
		compiled.reason = String(tracker.get("reason", "Knockback shield"));
	} else if (kind == sn_target_status_multiplier()) {
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
	} else if (kind == sn_stat_modifier()) {
		// Validate stat name
		StringName stat_name = StringName(String(tracker.get("stat_name", "")));
		if (!is_valid_stat_name(stat_name)) {
			compiled.opcode = EFFECT_OPCODE_UNKNOWN;
			return compiled;
		}

		compiled.damage_type = stat_name;
		compiled.scalar0 = double(tracker.get("additive", 0.0));
		compiled.scalar1 = double(tracker.get("multiplicative", 1.0));
		compiled.scalar2 = double(tracker.get("duration", 0.0));
		compiled.scalar3 = double(tracker.get("heal_gained_ratio", 0.0));
		compiled.int0 = tracker.get("target_self", false) ? 1 : 0;
		compiled.int1 = tracker.get("duration_type", "respawn") == "match" ? 1 : 0;
		compiled.int2 = int64_t(tracker.get("max_stacks", 1));
		String stack_behavior = String(tracker.get("stack_behavior", "refresh"));
		if (stack_behavior == "accumulate") {
			compiled.int3 = 1;
		} else if (stack_behavior == "reset") {
			compiled.int3 = 2;
		} else {
			compiled.int3 = 0;
		}
		compiled.reason = String(tracker.get("reason", "Stat modifier"));
	} else if (kind == sn_multi_target()) {
		// Multi-target effect parameters
		compiled.int0 = int64_t(tracker.get("target_count", 1));
		String strategy_str = String(tracker.get("selection_strategy", "closest"));
		if (strategy_str == "random") {
			compiled.int1 = TARGET_SELECTION_RANDOM;
		} else if (strategy_str == "lowest_hp") {
			compiled.int1 = TARGET_SELECTION_LOWEST_HP;
		} else if (strategy_str == "highest_hp") {
			compiled.int1 = TARGET_SELECTION_HIGHEST_HP;
		} else if (strategy_str == "closest_to_target") {
			compiled.int1 = TARGET_SELECTION_CLOSEST_TO_TARGET;
		} else if (strategy_str == "lowest_percent_hp") {
			compiled.int1 = TARGET_SELECTION_LOWEST_PERCENT_HP;
		} else if (strategy_str == "highest_percent_hp") {
			compiled.int1 = TARGET_SELECTION_HIGHEST_PERCENT_HP;
		} else if (strategy_str == "closest") {
			compiled.int1 = TARGET_SELECTION_CLOSEST;
		} else {
			UtilityFunctions::push_error(vformat("Invalid selection_strategy '%s' for multi_target effect", strategy_str));
			compiled.int1 = -1;
		}
		compiled.int2 = tracker.get("include_self", false) ? 1 : 0;
		String handling_str = String(tracker.get("excess_handling", "drop"));
		if (handling_str == "stack") {
			compiled.int3 = EXCESS_TARGET_STACK;
		} else if (handling_str == "drop") {
			compiled.int3 = EXCESS_TARGET_DROP;
		} else {
			UtilityFunctions::push_error(vformat("Invalid excess_handling '%s' for multi_target effect", handling_str));
			compiled.int3 = -1;
		}
		compiled.int4 = int64_t(tracker.get("repeat_count", 1));
		String team_filter_str = String(tracker.get("team_filter", ""));
		if (team_filter_str == "ally") {
			compiled.team_filter = sn_ally();
		} else if (team_filter_str == "enemy") {
			compiled.team_filter = sn_enemy();
		} else {
			compiled.team_filter = StringName(team_filter_str);
		}
		
		// Compile sub_effects
		Variant sub_effects_value = tracker.get("sub_effects", Variant());
		if (sub_effects_value.get_type() == Variant::ARRAY) {
			Array sub_effects_array = sub_effects_value;
			compiled.sub_effects = compile_effect_array(sub_effects_array);
		} else if (sub_effects_value.get_type() == Variant::DICTIONARY) {
			Dictionary sub_effect_dict = sub_effects_value;
			compiled.sub_effects.push_back(compile_effect(sub_effect_dict));
		}
		
		compiled.reason = String(tracker.get("reason", "Multiple target"));
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
