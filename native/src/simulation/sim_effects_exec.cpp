#include "sim_effects_exec.hpp"
#include "sim_effects_exec_internal.hpp"
#include "sim_effects_compile.hpp"

#include "sim_targeting.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

#include <cstdint>

namespace sim::effects::execution {
namespace internal {

namespace {

} // namespace

ExecRoute exec_route_for_opcode(int64_t opcode) {
	switch (opcode) {
		// Core effects (1-99)
		case EFFECT_OPCODE_MULTI_EFFECT: // 1
			return ExecRoute::MultiEffect;
		case EFFECT_OPCODE_DAMAGE: // 2
			return ExecRoute::Damage;
		case EFFECT_OPCODE_PROJECTILE: // 3
			return ExecRoute::Spawn;
		case EFFECT_OPCODE_MULTI_TARGET: // 4
			return ExecRoute::MultiTarget;
		case EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER: // 5
			return ExecRoute::Damage;
		case EFFECT_OPCODE_AUTO_DODGE: // 6
			return ExecRoute::Damage;
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER: // 7
			return ExecRoute::Damage;
		case EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER: // 8
			return ExecRoute::Damage;
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER: // 9
			return ExecRoute::Damage;
		case EFFECT_OPCODE_REFLECT_DAMAGE: // 10
			return ExecRoute::Damage;
		case EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER: // 11
			return ExecRoute::Damage;
		case EFFECT_OPCODE_STAT_MODIFIER: // 12
			return ExecRoute::Damage;
		case EFFECT_OPCODE_DAMAGE_OVER_TIME: // 13
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_CONSUME_STACKS_DAMAGE: // 14
			return ExecRoute::Damage;
		case EFFECT_OPCODE_REDIRECT_DAMAGE: // 15
			return ExecRoute::Damage;
		case EFFECT_OPCODE_SUMMON_ALLY: // 16
			return ExecRoute::Spawn;

		// Status effects (100-199)
		case EFFECT_OPCODE_STUN: // 100
			return ExecRoute::Status;
		case EFFECT_OPCODE_SHIELD: // 101
			return ExecRoute::Status;
		case EFFECT_OPCODE_HEAL: // 102
			return ExecRoute::Status;
		case EFFECT_OPCODE_MANA_REGEN: // 103
			return ExecRoute::Status;
		case EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN: // 104
			return ExecRoute::Status;
		case EFFECT_OPCODE_DAMAGE_BASED_HEAL: // 105
			return ExecRoute::Status;
		case EFFECT_OPCODE_MANA_RESTORE_ON_HIT: // 106
			return ExecRoute::Status;
		case EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT: // 107
			return ExecRoute::Status;
		case EFFECT_OPCODE_EVERY_N_ATTACKS_STUN: // 108
			return ExecRoute::Status;
		case EFFECT_OPCODE_SELF_DASH: // 109
			return ExecRoute::Status;
		case EFFECT_OPCODE_SLOW: // 110
			return ExecRoute::Status;
		case EFFECT_OPCODE_ROOT: // 111
			return ExecRoute::Status;
		case EFFECT_OPCODE_SILENCE: // 112
			return ExecRoute::Status;
		case EFFECT_OPCODE_DISARM: // 113
			return ExecRoute::Status;
		case EFFECT_OPCODE_KNOCKBACK: // 114
			return ExecRoute::Status;
		case EFFECT_OPCODE_REFLECT: // 115
			return ExecRoute::Status;
		case EFFECT_OPCODE_KNOCKBACK_SHIELD: // 116
			return ExecRoute::Status;
		case EFFECT_OPCODE_STEALTH: // 117
			return ExecRoute::Status;
		case EFFECT_OPCODE_HEAL_OVER_TIME: // 118
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_DAMAGE_BASED_SHIELD: // 119
			return ExecRoute::Status;
		case EFFECT_OPCODE_CONSUME_STACKS_HEAL: // 120
			return ExecRoute::Status;
		case EFFECT_OPCODE_CONSUME_STACKS_SHIELD: // 121
			return ExecRoute::Status;
		case EFFECT_OPCODE_SET_STACKS: // 122
			return ExecRoute::Status;
		case EFFECT_OPCODE_CHANNEL: // 123
			return ExecRoute::Status;

		// AOE effects (200-299)
		case EFFECT_OPCODE_AOE_TAUNT: // 200
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_DAMAGE: // 201
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_SLOW: // 202
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_ROOT: // 203
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_SILENCE: // 204
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_DISARM: // 205
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_KNOCKBACK: // 206
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_REFLECT: // 207
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_STUN: // 208
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME: // 209
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_HEAL_OVER_TIME: // 210
			return ExecRoute::Aoe;

		default:
			return ExecRoute::DefaultOk;
	}
}

Dictionary execute_recursive(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		const ::sim::effects::SimMatchHost &match_host) {
	return execute_impl(effect, context, world, host, hooks, match_host);
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
	
	switch (exec_route_for_opcode(effect.opcode)) {
		case ExecRoute::Damage:
			return exec_damage(effect, context, world, host, hooks, match_host, source, target, target_ally);
		case ExecRoute::Status:
			return exec_status(effect, context, world, host, hooks, match_host, source, target, target_ally);
		case ExecRoute::Spawn:
			return exec_spawn(effect, context, world, host, hooks, match_host, source, target, target_ally);
		case ExecRoute::Aoe:
			return exec_aoe(effect, context, world, host, hooks, match_host, source, target, target_ally);
		case ExecRoute::MultiEffect: {
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
		case ExecRoute::MultiTarget: {
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
			
			multi_result["targets_affected"] = targets.size();
			multi_result["results"] = nested_results;
			multi_result["success"] = true;
			return multi_result;
		}
		case ExecRoute::DefaultOk:
		default: {
			Dictionary default_result;
			default_result["success"] = true;
			return default_result;
		}
	}
}

} // namespace internal

Dictionary execute(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		::sim::effects::SimMatchHost match_host) {
	return internal::execute_impl(effect, context, world, host, hooks, match_host);
}

} // namespace sim::effects::execution
