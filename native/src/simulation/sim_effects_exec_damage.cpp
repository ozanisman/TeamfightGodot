#include "sim_effects_exec.hpp"
#include "sim_effects_exec_internal.hpp"
#include "sim_effects_compile.hpp"

#include "sim_channel.hpp"
#include "sim_combat.hpp"
#include "sim_constants.hpp"
#include "sim_movement.hpp"
#include "sim_match_spawn.hpp"
#include "sim_stats_modifiers.hpp"
#include "sim_targeting.hpp"
#include "sim_damage.hpp"
#include "sim_periodic.hpp"
#include "sim_stats.hpp"
#include "sim_status.hpp"

#include "stat_definitions.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#define X(name, def, min_val, max_val) using sim::get_effective_##name;
STAT_LIST
#undef X


namespace sim::effects::execution {
namespace internal {

Dictionary exec_damage(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, UnitState &source, UnitState *target, UnitState *target_ally) {
	switch (effect.opcode) {
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
	}
	UtilityFunctions::push_error(vformat("exec_damage: unhandled opcode %d", effect.opcode));
	Dictionary fallback_result;
	fallback_result["success"] = false;
	fallback_result["error"] = "Unhandled opcode in exec_damage";
	return fallback_result;
}

} // namespace internal
} // namespace sim::effects::execution
