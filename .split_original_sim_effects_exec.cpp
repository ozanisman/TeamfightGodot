#include "sim_effects_exec.hpp"
#include "sim_effects_compile.hpp"

#include "sim_channel.hpp"
#include "sim_combat.hpp"
#include "sim_constants.hpp"
#include "sim_movement.hpp"
#include "sim_stats_modifiers.hpp"
#include "sim_damage.hpp"
#include "sim_periodic.hpp"
#include "sim_match_spawn.hpp"
#include "sim_stats.hpp"
#include "sim_status.hpp"
#include "sim_targeting.hpp"

#include "stat_definitions.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#define X(name, def, min_val, max_val) using sim::get_effective_##name;
STAT_LIST
#undef X

namespace sim::effects::execution {
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

UnitState *unit_by_id(SimWorld &world, int64_t instance_id) {
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

Dictionary execute_impl(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const effects::SimMatchHost &match_host);

Dictionary execute_recursive(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const effects::SimMatchHost &match_host) {
	return execute_impl(effect, context, world, host, hooks, match_host);
}

void merge_accumulated_results(Dictionary &target, const Dictionary &source) {
	Array keys = source.keys();
	for (int64_t i = 0; i < keys.size(); ++i) {
		Variant key = keys[i];
		Variant source_value = source[key];
		target[key] = source_value;
	}
}

bool check_condition(const EffectRecord &effect, const Dictionary &results) {
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

bool check_target_status_condition(const EffectRecord &effect, const EffectContext &context, SimWorld &world) {
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
	
	return sim::status::target_has_status(world, *unit_to_check, status_to_check);
}

bool check_all_conditions(const EffectRecord &effect, const Dictionary &results, const EffectContext &context, SimWorld &world) {
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

void merge_result(Dictionary &target_result, const Dictionary &source_result) {
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


Dictionary execute_impl(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const effects::SimMatchHost &match_host) {
	Dictionary result;
	
	// DEBUG: Log when _execute_effect is called with unknown opcode
	if (effect.opcode == EFFECT_OPCODE_UNKNOWN) {
		UtilityFunctions::push_error(vformat("[DEBUG] _execute_effect: called with EFFECT_OPCODE_UNKNOWN for source_id=%d", context.source != nullptr ? context.source->instance_id : 0));
	}
	
	if (context.source == nullptr) {
		return result;
	}
	UnitState &source = *context.source;
	UnitState *target = context.target;
	UnitState *target_ally = context.target_ally;
	
	// Check conditional requirements
	if (!check_all_conditions(effect, context.accumulated_results, context, world)) {
		Dictionary failed_result;
		failed_result["success"] = false;
		failed_result["condition_failed"] = true;
		return failed_result;
	}
	
	switch (effect.opcode) {
		case EFFECT_OPCODE_MULTI_EFFECT: {
			Dictionary combined_results;
			for (const EffectRecord &child : effect.children) {
				// Update context with accumulated results before executing child
				context.accumulated_results = combined_results;
				Dictionary child_result = execute_recursive(child, context, world, host, hooks, match_host);
				// Store the result under the effect's kind name for conditional access
				const StringName &child_kind = sim::effects::compile::kind_for_opcode(child.opcode);
				if (!child_kind.is_empty()) {
					combined_results[child_kind] = child_result;
					// Also update the accumulated_results for subsequent children
					context.accumulated_results = combined_results;
				}
			}
			return combined_results;
		}
		case EFFECT_OPCODE_DAMAGE: {
			Dictionary damage_result;
			damage_result["success"] = false;
			
			// Target resolution with self-targeting support
			UnitState *damage_target = (effect.int0 == 1) ? &source : target;
			if (damage_target == nullptr) {
				return damage_result;
			}
			
			// Unified damage calculation
			double damage;
			// Use accumulated damage if requested
			if (effect.int1 != 0 && context.channel_accumulated_damage > 0.0) {
				damage = context.channel_accumulated_damage * effect.scalar1;
			} else {
				damage = get_effective_max_hp(*damage_target) * effect.scalar0;  // max_hp_ratio
				damage += get_effective_attack_damage(source) * effect.scalar1;  // damage_ratio
				damage += effect.scalar2;  // flat_amount
			}
			
			// Apply ability/ultimate modifiers if applicable
			if (context.action_kind == sn_ability()) {
				damage = sim::damage::apply_ability_modifiers(world, host, source, damage_target, damage);
			} else if (context.action_kind == sn_ultimate()) {
				damage = sim::damage::apply_ultimate_modifiers(world, host, source, damage_target, damage);
			}
			
			double dealt = sim::damage::apply_damage(world, host, source, *damage_target, damage, effect.damage_type.is_empty() ? sn_physical() : effect.damage_type, context.action_kind, context);
			context.damage = dealt;
			
			// Minimum health check for self-damage
			if (effect.int0 == 1 && damage_target->alive && damage_target->hp <= 1.0) {
				damage_target->hp = 1.0;
			}
			
			// trigger_on_hit logic
			if (effect.scalar3 > 0.5) {
				sim::combat::run_post_attack_effects(world, host, source, *damage_target, dealt, context);
				
				// Apply lifesteal for abilities with trigger_on_hit (excluding self-damage)
				if (source.instance_id != damage_target->instance_id) {
					double life_steal = get_effective_life_steal(source);
					if (life_steal > 0.0) {
						double old_hp = source.hp;
						double heal_amount = dealt * life_steal;
						sim::status::heal_unit(world, source, source, heal_amount, context.action_kind);
						double heal_gained = source.hp - old_hp;
						sim::combat::run_post_heal_effects(world, host, source, source, heal_amount, heal_gained, context);
					}
				}
			}
			damage_result["success"] = true;
			damage_result["damage_dealt"] = dealt;
			damage_result["target_killed"] = !damage_target->alive;
			return damage_result;
		}
		case EFFECT_OPCODE_PROJECTILE: {
			Dictionary projectile_result;
			projectile_result["success"] = false;
			if (target == nullptr) {
				return projectile_result;
			}
			ProjectileState projectile_state;
			projectile_state.source_id = source.instance_id;
			projectile_state.target_id = target->instance_id;
			double damage = get_effective_attack_damage(source) * effect.scalar2;
			
			// Apply ability/ultimate modifiers if applicable
			if (context.action_kind == sn_ability()) {
				damage = sim::damage::apply_ability_modifiers(world, host, source, target, damage);
			} else if (context.action_kind == sn_ultimate()) {
				damage = sim::damage::apply_ultimate_modifiers(world, host, source, target, damage);
			}
			
			projectile_state.damage = damage;
			projectile_state.damage_type = effect.damage_type.is_empty() ? sn_physical() : effect.damage_type;
			projectile_state.stun_duration = effect.scalar3;
			// Python parity: null speed/radius override â†’ fall back to unit's projectile stats.
			double projectile_speed = (effect.scalar0 < 0.0)
				? Math::max(0.0001, get_effective_projectile_speed(source))
				: Math::max(0.0001, effect.scalar0);
			projectile_state.radius = (effect.scalar1 < 0.0)
				? get_effective_projectile_radius(source)
				: effect.scalar1;
			projectile_state.speed = projectile_speed;
			projectile_state.pos_x = source.pos_x;
			projectile_state.pos_y = source.pos_y;
			projectile_state.action_kind = context.action_kind;
			projectile_state.reason = String(effect.reason);
			if (match_host.projectiles != nullptr) {
				match_host.projectiles->push_back(projectile_state);
			}
			projectile_result["success"] = true;
			projectile_result["projectile_created"] = true;
			return projectile_result;
		}
		case EFFECT_OPCODE_STUN: {
			Dictionary stun_result;
			stun_result["success"] = false;
			if (target != nullptr) {
				sim::status::apply_stun(world, source, *target, effect.scalar0);
				stun_result["success"] = true;
				stun_result["stun_applied"] = true;
				stun_result["duration"] = effect.scalar0;
			}
			return stun_result;
		}
		case EFFECT_OPCODE_SHIELD: {
			Dictionary shield_result;
			shield_result["success"] = true;
			UnitState &shield_target = (effect.int0 == 1) ? source : (target_ally == nullptr ? source : *target_ally);
			double amount = get_effective_max_hp(shield_target) * effect.scalar0;
			
			// Use context.damage, but fall back to accumulated damage if needed
			double damage_for_ratio = context.damage;
			if (damage_for_ratio <= 0.0 && context.accumulated_results.has("damage")) {
				Dictionary damage_result = context.accumulated_results["damage"];
				if (damage_result.has("damage_dealt")) {
					damage_for_ratio = damage_result["damage_dealt"];
				}
			}
			amount += damage_for_ratio * effect.scalar1;
			
			amount += effect.scalar2;
			
			if (amount <= 0.0) {
				shield_result["shield_applied"] = false;
				shield_result["amount"] = 0.0;
				return shield_result;
			}
			sim::status::add_shield(world, source, shield_target, amount, context.action_kind);
			shield_result["shield_applied"] = true;
			shield_result["amount"] = amount;
			return shield_result;
		}
		case EFFECT_OPCODE_HEAL: { 
			Dictionary heal_result;
			heal_result["success"] = true;
			UnitState &heal_target = (effect.int0 == 1) ?
				source :
				(target_ally == nullptr ? source : *target_ally);
			double heal_amount = get_effective_max_hp(heal_target) * effect.scalar0 + heal_target.hp * effect.scalar1 + effect.scalar2;
			// Add missing HP scaling
			double missing_hp = get_effective_max_hp(heal_target) - heal_target.hp;
			heal_amount += missing_hp * effect.scalar3;
			double old_hp = heal_target.hp;
			sim::status::heal_unit(world, source, heal_target, heal_amount, context.action_kind);
			double heal_gained = heal_target.hp - old_hp;
			sim::combat::run_post_heal_effects(world, host, source, heal_target, heal_amount, heal_gained, context);
			heal_result["heal_applied"] = true;
			heal_result["amount"] = heal_gained;
			return heal_result;
		}
		case EFFECT_OPCODE_AOE_TAUNT: {
			Dictionary taunt_result;
			taunt_result["success"] = true;
			sim::periodic::apply_aoe_taunt_shape(world, source, target, effect, effect.scalar1);
			taunt_result["taunt_applied"] = true;
			taunt_result["radius"] = effect.scalar0;
			taunt_result["duration"] = effect.scalar1;
			// INCONSISTENT: has extra fields (taunt_applied, radius, duration) while other AOE status effects only return success
			return taunt_result;
		}
		case EFFECT_OPCODE_AOE_DAMAGE: {
			Dictionary aoe_damage_result;
			aoe_damage_result["success"] = true;
			// Use accumulated damage if requested
			if (effect.int0 != 0 && context.channel_accumulated_damage > 0.0) {
				double aoe_damage = context.channel_accumulated_damage * effect.scalar1;
				double splash_ratio = effect.scalar2;
				if (splash_ratio != 1.0) {
					aoe_damage *= splash_ratio;
				}
				double total_damage = sim::periodic::apply_aoe_damage_shape(world, host, source, target, effect, aoe_damage, effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type, context.action_kind);
				aoe_damage_result["aoe_damage_applied"] = true;
				aoe_damage_result["damage"] = total_damage;
				context.damage = total_damage;
				return aoe_damage_result;
			} else {
				// Calculate per-target damage using target's max_hp for max_hp_ratio
				double total_damage = sim::periodic::apply_aoe_damage_shape_per_target(world, host, source, target, effect, effect.scalar1, effect.scalar3, effect.scalar4, effect.scalar2, effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type, context.action_kind);
				aoe_damage_result["aoe_damage_applied"] = true;
				aoe_damage_result["damage"] = total_damage;
				context.damage = total_damage;
				return aoe_damage_result;
			}
		}
	case EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER: {
		{
			Dictionary threshold_result;
			threshold_result["success"] = true;
			if (context.damage > get_effective_attack_damage(source) * effect.scalar0 && !effect.children.empty()) {
				Dictionary child_result = execute_recursive(effect.children[0], context, world, host, hooks, match_host);
				threshold_result["triggered"] = true;
				merge_accumulated_results(threshold_result, child_result);
				return threshold_result;
			}
		}
		return Dictionary(); // INCONSISTENT: returns empty Dictionary instead of success=false
	}
	case EFFECT_OPCODE_DAMAGE_OVER_TIME: {
			Dictionary dot_result;
			dot_result["success"] = true;
			UnitState *dot_target = (effect.int0 == 1) ? &source : target;
			if (dot_target != nullptr) {
				sim::periodic::apply_dot(world, host, source, *dot_target, effect.scalar0, effect.scalar1, effect.scalar3,
						   effect.scalar4, effect.scalar2,
						   effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type,
						   effect.stacking_mode, effect.int1, effect.effect_type, effect.reason, context.action_kind, effect.int2 == 1);
				dot_result["dot_applied"] = true;
			}
			return dot_result;
		}
		case EFFECT_OPCODE_HEAL_OVER_TIME: {
			Dictionary hot_result;
			hot_result["success"] = true;
			UnitState &hot_target = (effect.int0 == 1) ? 
				source : 
				(target_ally == nullptr ? source : *target_ally);
			sim::periodic::apply_hot(world, host, source, hot_target, effect.scalar0, effect.scalar1, effect.scalar3, effect.scalar4,
					   effect.scalar5, effect.scalar2,
					   effect.stacking_mode, effect.int1, effect.int2 != 0, effect.effect_type, effect.reason, context.action_kind, effect.int3 == 1);
			hot_result["hot_applied"] = true;
			return hot_result;
		}
		case EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME: {
			Dictionary aoe_dot_result;
			aoe_dot_result["success"] = true;
			sim::periodic::apply_aoe_dot_shape(world, host, source, target, effect, effect.scalar1, effect.scalar2, effect.scalar3,
					   effect.scalar5, effect.scalar4,
					   effect.damage_type.is_empty() ? StringName("physical") : effect.damage_type,
					   effect.stacking_mode, effect.int0, effect.effect_type, effect.reason, effect.int2 != 0, context.action_kind, effect.int1 == 1);
			aoe_dot_result["aoe_dot_applied"] = true;
			return aoe_dot_result;
		}
		case EFFECT_OPCODE_AOE_HEAL_OVER_TIME: {
			Dictionary aoe_hot_result;
			aoe_hot_result["success"] = true;
			sim::periodic::apply_aoe_hot_shape(world, host, source, target, effect, effect.scalar1, effect.scalar2, effect.scalar3, effect.scalar4,
					   double(effect.int1), effect.scalar5,
					   effect.stacking_mode, effect.int0, effect.int2 != 0, effect.effect_type, effect.reason, effect.int3 != 0, context.action_kind, effect.int4 == 1);
			aoe_hot_result["aoe_hot_applied"] = true;
			return aoe_hot_result;
		}
		case EFFECT_OPCODE_MANA_REGEN: {
			Dictionary mana_result;
			mana_result["success"] = true;
			sim::status::restore_mana(world, source, source, effect.scalar0 + get_effective_max_mana(source) * effect.scalar1);
			mana_result["mana_restored"] = effect.scalar0 + get_effective_max_mana(source) * effect.scalar1;
			return mana_result;
		}
		case EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN: {
			Dictionary post_mana_result;
			post_mana_result["success"] = true;
			sim::status::restore_mana(world, source, source, context.damage * effect.scalar0);
			post_mana_result["mana_restored"] = context.damage * effect.scalar0;
			return post_mana_result;
		}
		case EFFECT_OPCODE_DAMAGE_BASED_HEAL: {
			Dictionary heal_result;
			heal_result["success"] = true;
			double damage_to_use = context.damage;
			// Use accumulated damage from channel if requested
			if (effect.int0 != 0 && context.channel_accumulated_damage > 0.0) {
				damage_to_use = context.channel_accumulated_damage;
			}
			double old_hp = source.hp;
			double heal_amount = damage_to_use * effect.scalar0;
			sim::status::heal_unit(world, source, source, heal_amount, context.action_kind);
			double heal_gained = source.hp - old_hp;
			sim::combat::run_post_heal_effects(world, host, source, source, heal_amount, heal_gained, context);
			heal_result["heal_applied"] = true;
			heal_result["amount"] = heal_gained;
			return heal_result;
		}
		case EFFECT_OPCODE_DAMAGE_BASED_SHIELD: {
			Dictionary shield_result;
			shield_result["success"] = true;
			sim::status::add_shield(world, source, source, context.damage * effect.scalar0, context.action_kind);
			shield_result["shield_applied"] = true;
			shield_result["amount"] = context.damage * effect.scalar0;
			return shield_result;
		}
		case EFFECT_OPCODE_CONSUME_STACKS_DAMAGE: {
			Dictionary result;
			result["success"] = true;
			if (target == nullptr) {
				result["damage_dealt"] = 0.0;
				result["stacks_consumed"] = 0;
				result["target_killed"] = false;
				return result;
			}
			double attack_damage = get_effective_attack_damage(source);
			int stacks = sim::stats_modifiers::consume_stat_stacks(source, effect.stat_name, effect.string1);
			double base_ratio = effect.scalar0;
			double stack_bonus = effect.scalar1;
			String stacking_mode = effect.string0;
			double final_ratio = base_ratio;
			if (stacking_mode == "multiplicative") {
				final_ratio = base_ratio * (1.0 + double(stacks) * stack_bonus);
			} else {
				final_ratio = base_ratio + (double(stacks) * stack_bonus);
			}
			double damage = attack_damage * final_ratio;
			sim::damage::apply_damage(world, host, source, *target, damage, effect.damage_type, context.action_kind, context);
			result["damage_dealt"] = damage;
			result["stacks_consumed"] = stacks;
			result["target_killed"] = !target->alive;
			return result;
		}
		case EFFECT_OPCODE_CONSUME_STACKS_HEAL: {
			Dictionary result;
			result["success"] = true;
			double max_hp = get_effective_max_hp(source);
			int stacks = sim::stats_modifiers::consume_stat_stacks(source, effect.stat_name, effect.string1);
			double base_ratio = effect.scalar0;
			double stack_bonus = effect.scalar1;
			String stacking_mode = effect.string0;
			double final_ratio = base_ratio;
			if (stacking_mode == "multiplicative") {
				final_ratio = base_ratio * (1.0 + double(stacks) * stack_bonus);
			} else {
				final_ratio = base_ratio + (double(stacks) * stack_bonus);
			}
			double heal_amount = max_hp * final_ratio;
			double old_hp = source.hp;
			sim::status::heal_unit(world, source, source, heal_amount, context.action_kind);
			double heal_gained = source.hp - old_hp;
			sim::combat::run_post_heal_effects(world, host, source, source, heal_amount, heal_gained, context);
			result["heal_applied"] = true;
			result["amount"] = heal_gained;
			result["stacks_consumed"] = stacks;
			return result;
		}
		case EFFECT_OPCODE_CONSUME_STACKS_SHIELD: {
			Dictionary result;
			result["success"] = true;
			double max_hp = get_effective_max_hp(source);
			int stacks = sim::stats_modifiers::consume_stat_stacks(source, effect.stat_name, effect.string1);
			double base_ratio = effect.scalar0;
			double stack_bonus = effect.scalar1;
			String stacking_mode = effect.string0;
			double final_ratio = base_ratio;
			if (stacking_mode == "multiplicative") {
				final_ratio = base_ratio * (1.0 + double(stacks) * stack_bonus);
			} else {
				final_ratio = base_ratio + (double(stacks) * stack_bonus);
			}
			double shield_amount = max_hp * final_ratio;
			sim::status::add_shield(world, source, source, shield_amount, context.action_kind);
			result["shield_applied"] = true;
			result["amount"] = shield_amount;
			result["stacks_consumed"] = stacks;
			return result;
		}
		case EFFECT_OPCODE_SET_STACKS: {
			Dictionary result;
			result["success"] = true;
			int stack_count = int(effect.int0);
			bool to_max = effect.int1 != 0;
			int fallback_max_stacks = int(effect.int2);
			double duration = effect.scalar0;
			double fallback_additive_per_stack = effect.scalar1;
			double fallback_multiplicative_per_stack = effect.scalar2;
			String stack_reason = effect.string0;
			sim::stats_modifiers::set_stat_stacks(source, effect.stat_name, stack_reason, stack_count, duration, to_max, fallback_max_stacks, fallback_additive_per_stack, fallback_multiplicative_per_stack);
			// Return the actual stack count after setting (accounting for to_max and clamping)
			String stack_key = sim::stats_modifiers::get_stack_key(effect.stat_name, stack_reason);
			Dictionary stack_entry = Dictionary(source.stat_stacks.get(stack_key, Dictionary()));
			int final_stacks = int(stack_entry.get("current_stacks", stack_count));
			result["stacks_set"] = final_stacks;
			return result;
		}
		case EFFECT_OPCODE_CHANNEL: {
			Dictionary channel_result;
			channel_result["success"] = true;
			
			// Check if already channeling
			if (uc(world, source).is_channeling) {
				UtilityFunctions::push_error("Unit is already channeling");
				// Apply cooldown on start failure
				if (context.action_kind == sn_ability()) {
					source.ability_cooldown = get_effective_ability_cd(source);
				}
				// Ultimates: mana already consumed on cast, no additional action needed
				channel_result["success"] = false;
				return channel_result;
			}
			
			// Initialize channel state
			UnitStateCold &cold = uc(world, source);
			cold.is_channeling = true;
			cold.channel_remaining_duration = effect.scalar0 + effect.scalar1;  // duration + tick_interval (accounts for immediate first tick)
			cold.channel_tick_interval = effect.scalar1;  // tick_interval
			cold.channel_allow_movement = effect.int0 != 0;
			cold.channel_target_mode = effect.string0;
			cold.channel_reason = effect.reason;
			cold.channel_action_kind = context.action_kind;
			cold.channel_source_instance_id = source.instance_id;
			cold.channel_tick_accumulator = 0.0;
			cold.channel_accumulated_damage = 0.0;
			
			// Set target
			if (target != nullptr) {
				cold.channel_target_instance_id = target->instance_id;
			}
			
			// Set sub-effect (required for channel to function)
			if (!effect.children.empty()) {
				cold.channel_sub_effect = effect.children[0];
				// Validate sub-effect is valid
				if (cold.channel_sub_effect.opcode == 0) {
					String error_msg = "Channel sub-effect has invalid opcode. Channel reason: ";
					error_msg += cold.channel_reason;
					UtilityFunctions::push_error(error_msg);
					channel_result["success"] = false;
					return channel_result;
				}
			} else {
				String error_msg = "Channel missing required sub-effect. Channel reason: ";
				error_msg += cold.channel_reason;
				UtilityFunctions::push_error(error_msg);
				channel_result["success"] = false;
				return channel_result;
			}
			
			// Set post-complete effect
			if (effect.sub_effects.size() > 0) {
				cold.channel_post_complete_effect = effect.sub_effects[0];
			}
			
			// Set post-interrupt effect
			if (effect.sub_effects.size() > 1) {
				cold.channel_post_interrupt_effect = effect.sub_effects[1];
			}
			
			// Process first tick immediately to avoid 1-tick delay
			channel::ChannelHostHooks channel_hooks;
			channel_hooks.user_data = host.user_data;
			channel_hooks.debug_combat_trace = hooks.debug_combat_trace;
			channel::process_channel_tick(world, host, channel_hooks, source, cold.channel_tick_interval);
			
			channel_result["channel_started"] = true;
			return channel_result;
		}
		case EFFECT_OPCODE_MANA_RESTORE_ON_HIT: {
			Dictionary mana_result;
			mana_result["success"] = true;
			sim::status::restore_mana(world, source, source, effect.scalar0);
			mana_result["mana_restored"] = effect.scalar0;
			return mana_result;
		}
		case EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT: {
			Dictionary drain_result;
			drain_result["success"] = true;
			if (target != nullptr) {
				target->mana = Math::max(0.0, target->mana - effect.scalar0);
				drain_result["mana_drained"] = effect.scalar0;
			}
			return drain_result;
		}
		case EFFECT_OPCODE_EVERY_N_ATTACKS_STUN: {
			Dictionary stun_result;
			stun_result["success"] = true;
			if (effect.int0 > 0 && target != nullptr && source.attack_count % effect.int0 == 0) {
				sim::status::apply_stun(world, source, *target, effect.scalar0);
				stun_result["stun_applied"] = true;
				stun_result["duration"] = effect.scalar0;
			}
			return stun_result;
		}
		case EFFECT_OPCODE_SLOW: {
			Dictionary slow_result;
			slow_result["success"] = true;
			if (target != nullptr) {
				sim::status::apply_slow(world, source, *target, effect.scalar0, effect.scalar1);
			}
			return slow_result;
		}
		case EFFECT_OPCODE_ROOT: {
			Dictionary root_result;
			root_result["success"] = true;
			if (target != nullptr) {
				sim::status::apply_root(world, source, *target, effect.scalar0);
			}
			return root_result;
		}
		case EFFECT_OPCODE_SILENCE: {
			Dictionary silence_result;
			silence_result["success"] = true;
			if (target != nullptr) {
				sim::status::apply_silence(world, source, *target, effect.scalar0, effect.int0 != 0, effect.int1 != 0);
			}
			return silence_result;
		}
		case EFFECT_OPCODE_DISARM: {
			Dictionary disarm_result;
			disarm_result["success"] = true;
			if (target != nullptr) {
				sim::status::apply_disarm(world, source, *target, effect.scalar0);
			}
			return disarm_result;
		}
		case EFFECT_OPCODE_STEALTH: {
			Dictionary stealth_result;
			stealth_result["success"] = true;
			stealth_result["stealth_applied"] = false;
			bool target_self = effect.int3 != 0;
			UnitState *stealth_target = target_self ? &source : target;
			if (stealth_target != nullptr) {
				sim::status::apply_stealth(world, source, *stealth_target, effect.scalar0, effect.int0 != 0, effect.int1 != 0, effect.int2 != 0);
				stealth_result["stealth_applied"] = true;
			}
			return stealth_result;
		}
		case EFFECT_OPCODE_AOE_SLOW: {
			Dictionary aoe_slow_result;
			aoe_slow_result["success"] = true;
			sim::status::apply_aoe_slow_shape(world, source, target, effect, effect.scalar1, effect.scalar2);
			return aoe_slow_result;
		}
		case EFFECT_OPCODE_AOE_ROOT: {
			Dictionary aoe_root_result;
			aoe_root_result["success"] = true;
			sim::status::apply_aoe_root_shape(world, source, target, effect, effect.scalar1);
			return aoe_root_result;
		}
		case EFFECT_OPCODE_AOE_SILENCE: {
			Dictionary aoe_silence_result;
			aoe_silence_result["success"] = true;
			sim::status::apply_aoe_silence_shape(world, source, target, effect, effect.scalar1, effect.int0 != 0, effect.int1 != 0);
			return aoe_silence_result;
		}
		case EFFECT_OPCODE_AOE_DISARM: {
			Dictionary aoe_disarm_result;
			aoe_disarm_result["success"] = true;
			sim::status::apply_aoe_disarm_shape(world, source, target, effect, effect.scalar1);
			return aoe_disarm_result;
		}
		case EFFECT_OPCODE_AOE_KNOCKBACK: {
			Dictionary aoe_kb_result;
			aoe_kb_result["success"] = true;
			aoe_kb_result["knockback_applied"] = sim::periodic::apply_aoe_knockback_shape(world, host, source, target, effect, effect.scalar1, effect.int0 != 0);
			return aoe_kb_result;
		}
		case EFFECT_OPCODE_AOE_REFLECT: {
			Dictionary aoe_rf_result;
			aoe_rf_result["success"] = true;
			sim::periodic::apply_aoe_reflect_shape(world, host, source, target, effect, effect.scalar1, effect.scalar2, effect.int0 == 1, context.action_kind, effect.reason);
			return aoe_rf_result;
		}
		case EFFECT_OPCODE_AOE_STUN: {
			Dictionary aoe_stun_result;
			aoe_stun_result["success"] = true;
			sim::status::apply_aoe_stun_shape(world, source, target, effect, effect.scalar1);
			return aoe_stun_result;
		}
		case EFFECT_OPCODE_KNOCKBACK_SHIELD: {
			Dictionary ks_result;
			ks_result["success"] = false;
			Dictionary knockback_result = Dictionary(context.accumulated_results.get(StringName("knockback"), Dictionary()));
			if (bool(knockback_result.get("knockback_applied", false))) {
				double shield_amt = context.damage * effect.scalar0;
				sim::status::add_shield(world, source, source, shield_amt, context.action_kind);
				ks_result["success"] = true;
				ks_result["shield_applied"] = true;
				ks_result["amount"] = shield_amt;
			}
			return ks_result;
		}
		case EFFECT_OPCODE_KNOCKBACK: {
			Dictionary kb_result;
			kb_result["success"] = false;
			if (target != nullptr) {
				bool knocked_back = sim::periodic::apply_knockback(world, host, source, *target, effect.scalar0, effect.int0 != 0);
				kb_result["success"] = true;
				kb_result["knockback_applied"] = knocked_back;
			}
			return kb_result;
		}
		case EFFECT_OPCODE_REFLECT: {
			Dictionary rf_result;
			rf_result["success"] = true;
			StringName damage_type = effect.int0 == 1 ? StringName("all") : StringName("physical");
			sim::periodic::apply_reflect_buff(world, source, source, effect.scalar0, effect.scalar1, context.action_kind, damage_type, effect.reason);
			return rf_result;
		}
		case EFFECT_OPCODE_REFLECT_DAMAGE: {
			Dictionary rd_noop;
			rd_noop["success"] = true;
			// INCONSISTENT: returns only success with no information about the passive effect it enables
			return rd_noop;
		}
		case EFFECT_OPCODE_REDIRECT_DAMAGE: {
			Dictionary redirect_result;
			redirect_result["success"] = true;
			redirect_result["redirected_damage"] = 0.0;

			// redirect_damage is a passive modifier that should be applied during damage calculation
			// This effect doesn't directly apply damage, but stores parameters for the damage system
			// The actual redirect logic is handled in _apply_damage or a similar function
			// For now, this is a no-op that stores the parameters in the effect record
			return redirect_result;
		}
		case EFFECT_OPCODE_SUMMON_ALLY: {
			Dictionary summon_result;
			summon_result["success"] = true;
			summon_result["minions_spawned"] = 0;

			double spawn_radius = effect.scalar0;
			int64_t total_spawned = 0;

			// Copy source data before modifying _units (push_back can reallocate and invalidate references)
			int64_t source_instance_id = source.instance_id;
			double source_pos_x = source.pos_x;
			double source_pos_y = source.pos_y;
			StringName source_team = source.team;

			// Use tracked max instance ID for efficient ID generation
			int64_t next_instance_id = (match_host.max_instance_id != nullptr ? *match_host.max_instance_id : 0) + 1;

			// Max radius for expansion fallback (3x initial radius, capped at 5.0)
			constexpr double max_spawn_radius_expansion = 5.0;
			double max_spawn_radius = Math::min(spawn_radius * 3.0, max_spawn_radius_expansion);

			// Track pending spawn positions to prevent intra-batch overlap
			std::vector<Vector2> pending_positions;

			// Iterate through children (each child is a minion spec with minion_id and count)
			for (const EffectRecord &minion_spec : effect.children) {
				StringName minion_id = StringName(minion_spec.string0);
				int64_t count = minion_spec.int0;

				// Get minion data from minion_catalog
				Dictionary minion_data;
				if (match_host.catalog != nullptr) {
					minion_data = Dictionary(match_host.catalog->minion_catalog.get(String(minion_id), Dictionary()));
				}
				if (minion_data.is_empty()) {
					UtilityFunctions::push_error(vformat("Summon failed: unknown minion archetype: %s", String(minion_id)));
					continue;
				}

			// Spawn count minions of this type
			for (int64_t i = 0; i < count; ++i) {
				// Find random valid position within spawn_radius, with expansion fallback on failure
				Vector2 spawn_pos = sim::match::find_random_spawn_position_near_excluding_with_expansion(
						world,
						host,
						source_pos_x,
						source_pos_y,
						spawn_radius,
						max_spawn_radius,
						source_instance_id,
						pending_positions);
				if (spawn_pos.x < 0.0) {
					// Failed to find valid position even with expansion
					UtilityFunctions::push_error(vformat("Summon failed: could not find valid spawn position for minion %s (%d/%d) near (%.2f, %.2f) with radius %.2f (expanded to %.2f). Active units: %d",
						String(minion_id), i + 1, count, source_pos_x, source_pos_y, spawn_radius, max_spawn_radius, world.units.size()));
					continue;
				}

				// Add to pending positions to prevent overlap with subsequent minions in this batch
				pending_positions.push_back(spawn_pos);

				// Create spawn spec
				Dictionary spawn_spec;
				spawn_spec["archetype_id"] = minion_id;
				spawn_spec["x"] = spawn_pos.x;
				spawn_spec["y"] = spawn_pos.y;

				// Queue the spawn for later processing (at end of tick)
				PendingSpawn pending;
				pending.spawn_spec = spawn_spec;
				pending.team = source_team;
				pending.summoner_instance_id = source_instance_id;
				if (match_host.pending_spawns != nullptr) {
					match_host.pending_spawns->push_back(pending);
				}

				next_instance_id++;
				if (match_host.max_instance_id != nullptr) {
					*match_host.max_instance_id = next_instance_id;
				}
				total_spawned++;
			}
			}

			summon_result["minions_spawned"] = total_spawned;
			return summon_result;
		}
		case EFFECT_OPCODE_SELF_DASH: {
			Dictionary dash_result;
			dash_result["success"] = true;
			dash_result["reached_target"] = false;
			if (source.root_remaining > 0.0) {
				dash_result["distance_traveled"] = 0.0;
				return dash_result;
			}
			
			double dash_distance = effect.scalar0;
			double dir_x = effect.scalar1;
			double dir_y = effect.scalar2;
			
			double current_x = source.pos_x;
			double current_y = source.pos_y;
			double new_x = current_x;
			double new_y = current_y;
			
			// Check if direction is explicitly provided (non-zero values)
			bool has_direction = (dir_x != 0.0 || dir_y != 0.0);
			
			if (has_direction) {
				// Use explicit direction
				new_x = current_x + dir_x * dash_distance;
				new_y = current_y + dir_y * dash_distance;
			} else {
				// Use target from context
				if (target != nullptr) {
					int64_t target_id = target->instance_id;
					UnitState *target_unit = unit_by_id(world, target_id);
					if (target_unit != nullptr) {
						double target_x = target_unit->pos_x;
						double target_y = target_unit->pos_y;
						double dx = target_x - current_x;
						double dy = target_y - current_y;
						double dist = sim::distance_between_coords(current_x, current_y, target_x, target_y);
						if (dist > 0.0) {
							double move_dist = Math::min(dash_distance, dist);
							new_x = current_x + (dx / dist) * move_dist;
							new_y = current_y + (dy / dist) * move_dist;
						}
					}
				}
			}
			
			// Find valid position avoiding unit collisions
			int64_t source_id = source.instance_id;
			Vector2 valid_pos = sim::movement::find_valid_dash_position(world, current_x, current_y, new_x, new_y, dash_distance, source_id);
			new_x = valid_pos.x;
			new_y = valid_pos.y;
			
			// Clamp to boundaries (already done in _find_valid_dash_position, but ensure it)
			new_x = Math::clamp(new_x, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			new_y = Math::clamp(new_y, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			
			// Calculate reached_target based on final distance to target
			if (!has_direction && target != nullptr) {
				int64_t target_id = target->instance_id;
				UnitState *target_unit = unit_by_id(world, target_id);
				if (target_unit != nullptr) {
					double target_x = target_unit->pos_x;
					double target_y = target_unit->pos_y;
					double final_dx = target_x - new_x;
					double final_dy = target_y - new_y;
					double final_dist = sim::distance_between_coords(new_x, new_y, target_x, target_y);
					// Consider target reached if we're within attack range (with melee buffer)
					bool reached = (final_dist <= get_effective_attack_range(source) + 0.1);
					dash_result["reached_target"] = reached;
				}
			}
			
			source.pos_x = new_x;
			source.pos_y = new_y;
			host.sync_targeting_frame_unit(host.user_data, source);
			
			// Calculate actual distance traveled
			double actual_distance = sim::distance_between_coords(current_x, current_y, new_x, new_y);
			dash_result["distance_traveled"] = actual_distance;
			
			return dash_result;
		}
		case EFFECT_OPCODE_AUTO_DODGE:
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER:
		case EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER:
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
		// INCONSISTENT: multiplier effects fallthrough to stat_modifier execution case instead of having separate logic
		case EFFECT_OPCODE_STAT_MODIFIER: {
			Dictionary stat_result;
			stat_result["success"] = true;
			if (target != nullptr) {
				UnitState &modifier_target = (effect.int0 == 1) ? source : *target;

				// Calculate heal-based additive value if heal_gained_ratio is set
				double additive_value = effect.scalar0;
				double heal_based_additive = 0.0;
				if (effect.scalar3 > 0.0) {
					heal_based_additive = context.heal_gained * effect.scalar3;
					additive_value += heal_based_additive;
				}

				bool use_stacking = effect.int2 > 1 || effect.int3 != 0;
				if (use_stacking) {
					StackBehavior stack_behavior = StackBehavior::Refresh;
					if (effect.int3 == 1) {
						stack_behavior = StackBehavior::Accumulate;
					} else if (effect.int3 == 2) {
						stack_behavior = StackBehavior::Reset;
					}
					sim::stats_modifiers::apply_stacked_stat_modifier(
							source,
							modifier_target,
							effect.damage_type,
							additive_value,
							effect.scalar1,
							effect.scalar2,
							effect.int1 != 0,
							int(effect.int2),
							stack_behavior,
							effect.reason);
					stat_result["stat_modifier_applied"] = true;
					stat_result["use_stacking"] = true;
					stat_result["max_stacks"] = effect.int2;
					stat_result["stack_behavior"] = effect.int3;
				} else {
					sim::stats_modifiers::apply_simple_stat_modifier(source, modifier_target, effect.damage_type, additive_value, effect.scalar1, effect.scalar2, effect.int1 != 0, effect.reason);
					stat_result["stat_modifier_applied"] = true;
					stat_result["use_stacking"] = false;
				}

				stat_result["stat_name"] = String(effect.damage_type);
				stat_result["additive"] = additive_value;
				stat_result["multiplicative"] = effect.scalar1;
				stat_result["duration"] = effect.scalar2;
				stat_result["is_match_duration"] = effect.int1 != 0;
				stat_result["target_self"] = effect.int0 == 1;
				stat_result["reason"] = effect.reason;
			}
			return stat_result;
		}
		case EFFECT_OPCODE_MULTI_TARGET: {
			Dictionary multi_result;
			multi_result["success"] = true;
			
			// Extract multi-target parameters
			int64_t target_count = effect.int0;
			TargetSelectionStrategy strategy = static_cast<TargetSelectionStrategy>(effect.int1);
			bool include_self = effect.int2 != 0;
			ExcessTargetHandling excess_handling = static_cast<ExcessTargetHandling>(effect.int3);
			int64_t repeat_count = effect.int4;
			
			// Validate target_count (must be >= 1, where -1 means "all")
			if (target_count != -1 && target_count < 1) {
				UtilityFunctions::push_error(vformat("Invalid target_count %d, must be >= 1 (-1 for all targets)", target_count));
				multi_result["success"] = false;
				return multi_result;
			}
			
			// Validate repeat_count (must be >= 1)
			if (repeat_count < 1) {
				UtilityFunctions::push_error(vformat("Invalid repeat_count %d, must be >= 1", repeat_count));
				multi_result["success"] = false;
				return multi_result;
			}
			
			if (effect.int1 < TARGET_SELECTION_CLOSEST || effect.int1 > TARGET_SELECTION_HIGHEST_PERCENT_HP) {
				UtilityFunctions::push_error(vformat("Invalid selection_strategy opcode %d for multi_target effect", effect.int1));
				multi_result["success"] = false;
				return multi_result;
			}
			
			if (effect.int3 != EXCESS_TARGET_DROP && effect.int3 != EXCESS_TARGET_STACK) {
				UtilityFunctions::push_error(vformat("Invalid excess_handling opcode %d for multi_target effect", effect.int3));
				multi_result["success"] = false;
				return multi_result;
			}
			
			// Validate sub_effects
			if (effect.sub_effects.empty()) {
				UtilityFunctions::push_error("No sub_effects provided for multi_target effect");
				multi_result["success"] = false;
				return multi_result;
			}
			
			// Validate team_filter is explicitly provided
			if (effect.team_filter.is_empty()) {
				UtilityFunctions::push_error("team_filter is required for multi_target effect, must be 'ally' or 'enemy'");
				multi_result["success"] = false;
				return multi_result;
			}
			
			// Warn about sub-effects with target_self=true
			for (const EffectRecord &sub_effect : effect.sub_effects) {
				if (sub_effect.int0 == 1) {
					UtilityFunctions::push_warning("Sub-effect has target_self=true in multi_target effect, which may override selected targets");
				}
			}
			
			// Select targets
			std::vector<UnitState *> targets = sim::targeting::select_targets(
					world, host, source, target, target_count, strategy, include_self, excess_handling, effect.team_filter);
			
			if (targets.empty()) {
				multi_result["success"] = false;
				multi_result["targets_affected"] = 0;
				return multi_result;
			}
			
			// Apply each sub-effect to each target repeat_count times
			Dictionary nested_results;
			Dictionary summary_applications;
			for (UnitState *current_target : targets) {
				if (current_target == nullptr) {
					continue;
				}
				
				int64_t target_id = current_target->instance_id;
				
				for (const EffectRecord &sub_effect : effect.sub_effects) {
					for (int64_t i = 0; i < repeat_count; ++i) {
						EffectContext sub_context = context;
						sub_context.target = current_target;
						sub_context.target_ally = current_target;
						
						// Execute sub-effect
						Dictionary sub_result = execute_recursive(sub_effect, sub_context, world, host, hooks, match_host);
						
						// Check if sub-effect failed
						if (!sub_result.has("success") || !bool(sub_result.get("success", false))) {
							continue;
						}
						
						// Get effect kind for nesting
						const StringName &effect_kind = sim::effects::compile::kind_for_opcode(sub_effect.opcode);
						if (effect_kind.is_empty()) {
							continue;
						}
						
						// Ensure nested structure exists for this effect kind
						// Convert to String for safer Dictionary key handling across GDExtension
						String effect_kind_str = String(effect_kind);
						if (!nested_results.has(effect_kind_str)) {
							Dictionary effect_dict;
							Dictionary by_target_dict;
							effect_dict["by_target"] = by_target_dict;
							effect_dict["total"] = 0.0;
							nested_results[effect_kind_str] = effect_dict;
						}
						
						Dictionary effect_dict = nested_results[effect_kind_str];
						Dictionary by_target = effect_dict["by_target"];
						
						// Add result to by_target
						if (!by_target.has(target_id)) {
							by_target[target_id] = 0.0;
						}
						if (!summary_applications.has(effect_kind_str)) {
							Dictionary summary_by_target;
							summary_applications[effect_kind_str] = summary_by_target;
						}
						Dictionary summary_by_target = summary_applications[effect_kind_str];
						if (!summary_by_target.has(target_id)) {
							Array applications;
							summary_by_target[target_id] = applications;
						}
						
						// Extract numeric value from result
						double value = 0.0;
						if (effect_kind == sn_damage()) {
							value = double(sub_result.get("damage_dealt", 0.0));
						} else if (effect_kind == sn_heal()) {
							value = double(sub_result.get("amount", 0.0));
						} else if (effect_kind == sn_shield()) {
							value = double(sub_result.get("amount", 0.0));
						}
						// For CC effects, just count applications
						else {
							value = 1.0;
						}
						
						Array applications = summary_by_target[target_id];
						applications.append(value);
						summary_by_target[target_id] = applications;
						summary_applications[effect_kind_str] = summary_by_target;
						by_target[target_id] = double(by_target[target_id]) + value;
						effect_dict["total"] = double(effect_dict["total"]) + value;
						nested_results[effect_kind_str] = effect_dict;
					}
				}
			}
			
			String summary = vformat("multi_target summary source=%d targets=%d", source.instance_id, targets.size());
			Array effect_keys = nested_results.keys();
			for (int64_t effect_index = 0; effect_index < effect_keys.size(); ++effect_index) {
				String effect_key = String(effect_keys[effect_index]);
				Dictionary effect_dict = nested_results[effect_key];
				Dictionary by_target = effect_dict["by_target"];
				Dictionary summary_by_target = summary_applications[effect_key];
				summary += vformat("\n%s:", effect_key);
				Array target_keys = by_target.keys();
				for (int64_t target_index = 0; target_index < target_keys.size(); ++target_index) {
					Variant target_key = target_keys[target_index];
					Array applications = summary_by_target[target_key];
					String values = "[";
					for (int64_t application_index = 0; application_index < applications.size(); ++application_index) {
						if (application_index > 0) {
							values += ", ";
						}
						values += vformat("%.3f", double(applications[application_index]));
					}
					values += "]";
					summary += vformat("\n  target %s: %s total=%.3f", String(target_key), values, double(by_target[target_key]));
				}
				summary += vformat("\n  total: %.3f", double(effect_dict["total"]));
			}
			if (hooks.debug_combat_trace != nullptr && hooks.debug_combat_trace(host.user_data)) {
				UtilityFunctions::print(summary);
			}
			
			multi_result["targets_affected"] = targets.size();
			multi_result["results"] = nested_results;
			multi_result["success"] = true;
			return multi_result;
		}
		default: {
			// Opcodes without runtime execution resolve here (unknown kinds compile to UNKNOWN).
			Dictionary default_result;
			default_result["success"] = true;
			return default_result;
		}
	}

	// Should never reach here - all cases should return
	UtilityFunctions::push_error(vformat("_execute_effect: fell through switch for opcode %d", effect.opcode));
	Dictionary fallback_result;
	fallback_result["success"] = false;
	fallback_result["error"] = "Unhandled opcode in _execute_effect";
	return fallback_result;
}

} // namespace

Dictionary execute(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, effects::SimMatchHost match_host) {
	return execute_impl(effect, context, world, host, hooks, match_host);
}

} // namespace sim::effects::execution
