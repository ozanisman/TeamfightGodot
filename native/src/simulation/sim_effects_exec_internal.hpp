#ifndef SIM_EFFECTS_EXEC_INTERNAL_HPP
#define SIM_EFFECTS_EXEC_INTERNAL_HPP

#include "sim_effects_host.hpp"
#include "sim_status.hpp"
#include "sim_world.hpp"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::execution {
namespace internal {

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


inline UnitState *unit_by_id(SimWorld &world, int64_t instance_id) {
	auto it = world.unit_index_map.find(instance_id);
	if (it == world.unit_index_map.end()) {
		return nullptr;
	}
	const int64_t index = it->second;
	if (index < 0 || index >= int64_t(world.units.size())) {
		return nullptr;
	}
	return &world.units[static_cast<size_t>(index)];
}



inline void merge_accumulated_results(Dictionary &target, const Dictionary &source) {
	Array keys = source.keys();
	for (int64_t i = 0; i < keys.size(); ++i) {
		Variant key = keys[i];
		Variant source_value = source[key];
		target[key] = source_value;
	}
}

inline bool check_condition(const EffectRecord &effect, const Dictionary &results) {
	if (effect.requires_result_from.is_empty()) {
		return true;  // No condition
	}

	if (!results.has(effect.requires_result_from)) {
		UtilityFunctions::push_error(vformat(
			"Missing requires_result_from '%s' for conditional effect '%s'",
			String(effect.requires_result_from),
			String(effect.reason)
		));
		return false;  // Required effect didn't run
	}

	Dictionary required_result = results[effect.requires_result_from];
	if (!required_result.has(effect.requires_field)) {
		UtilityFunctions::push_error(vformat(
			"Missing requires_field '%s' in result '%s' for conditional effect '%s'",
			String(effect.requires_field),
			String(effect.requires_result_from),
			String(effect.reason)
		));
		return false;  // Field doesn't exist
	}

	Variant actual_value = required_result[effect.requires_field];
	return actual_value == effect.requires_value;
}

inline bool check_target_status_condition(const EffectRecord &effect, const EffectContext &context, SimWorld &world) {
	if (effect.requires_target_status.is_empty()) {
		return true;  // No condition
	}
	
	// Validate status string
	StringName status_to_check = effect.requires_target_status;
	bool is_valid_status = (status_to_check == sn_slow() || status_to_check == sn_root() || 
	                       status_to_check == sn_silence() || status_to_check == sn_disarm() || 
	                       status_to_check == sn_stealth() || status_to_check == sn_stun() || 
	                       status_to_check == sn_reflect());
	if (!is_valid_status) {
		UtilityFunctions::push_error(vformat("Invalid requires_target_status '%s' for effect '%s'. Valid statuses: slow, root, silence, disarm, stealth, stun, reflect", 
			String(status_to_check), String(effect.reason)));
		return false;
	}
	
	UnitState *unit_to_check = nullptr;
	if (effect.status_target == sn_source()) {
		unit_to_check = context.source;
	} else if (effect.status_target == sn_target()) {
		unit_to_check = context.target;
	} else {
		UtilityFunctions::push_error(vformat("Invalid status_target '%s' for effect '%s'", String(effect.status_target), String(effect.reason)));
		return false;
	}
	
	if (unit_to_check == nullptr) {
		return false;
	}
	
	return ::sim::status::target_has_status(world, *unit_to_check, status_to_check);
}

inline bool check_all_conditions(const EffectRecord &effect, const Dictionary &results, const EffectContext &context, SimWorld &world) {
	// Check requires_result_from condition
	if (!check_condition(effect, results)) {
		return false;
	}
	
	// Check requires_target_status condition
	if (!check_target_status_condition(effect, context, world)) {
		return false;
	}
	
	return true;
}

inline void merge_result(Dictionary &target_result, const Dictionary &source_result) {
	Array keys = source_result.keys();
	for (int64_t i = 0; i < keys.size(); ++i) {
		Variant key = keys[i];
		Variant source_value = source_result[key];
		if (target_result.has(key) && source_value.get_type() == Variant::FLOAT && target_result[key].get_type() == Variant::FLOAT) {
			target_result[key] = double(target_result[key]) + double(source_value);
		} else if (target_result.has(key) && source_value.get_type() == Variant::INT && target_result[key].get_type() == Variant::INT) {
			target_result[key] = int64_t(target_result[key]) + int64_t(source_value);
		} else {
			target_result[key] = source_value;
		}
	}
}




Dictionary execute_impl(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		const ::sim::effects::SimMatchHost &match_host);

Dictionary execute_recursive(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		const ::sim::effects::SimMatchHost &match_host);

Dictionary exec_damage(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, UnitState &source, UnitState *target, UnitState *target_ally);
Dictionary exec_status(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, UnitState &source, UnitState *target, UnitState *target_ally);
Dictionary exec_spawn(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, UnitState &source, UnitState *target, UnitState *target_ally);
Dictionary exec_aoe(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, UnitState &source, UnitState *target, UnitState *target_ally);

} // namespace internal
} // namespace sim::effects::execution

#endif // SIM_EFFECTS_EXEC_INTERNAL_HPP
