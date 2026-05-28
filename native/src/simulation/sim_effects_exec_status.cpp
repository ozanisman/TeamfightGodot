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

Dictionary exec_status(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, UnitState &source, UnitState *target, UnitState *target_ally) {
	switch (effect.opcode) {
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
	}
	UtilityFunctions::push_error(vformat("exec_status: unhandled opcode %d", effect.opcode));
	Dictionary fallback_result;
	fallback_result["success"] = false;
	fallback_result["error"] = "Unhandled opcode in exec_status";
	return fallback_result;
}

} // namespace internal
} // namespace sim::effects::execution
