#include "sim_effects_exec_internal.hpp"

#include "sim_combat.hpp"
#include "sim_stats.hpp"
#include "sim_stats_modifiers.hpp"
#include "sim_status.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

#define X(name, def, min_val, max_val) using sim::get_effective_##name;
STAT_LIST
#undef X

namespace sim::effects::execution {
namespace internal {

Dictionary exec_status_heal_shield(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks & /*hooks*/,
		UnitState &source,
		UnitState * /*target*/,
		UnitState *target_ally) {
	switch (effect.opcode) {
		case EFFECT_OPCODE_SHIELD: {
			Dictionary shield_result;
			shield_result["success"] = true;
			UnitState &shield_target = (effect.int0 == 1) ? source : (target_ally == nullptr ? source : *target_ally);
			double amount = (shield_target.stats_dirty ? get_effective_max_hp(shield_target) : shield_target.cached_max_hp) * effect.scalar0;

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
			sim::status::add_shield(world, source, shield_target, amount, context.action_kind, &host);
			shield_result["shield_applied"] = true;
			shield_result["amount"] = amount;
			return shield_result;
		}
		case EFFECT_OPCODE_HEAL: {
			Dictionary heal_result;
			heal_result["success"] = true;
			UnitState &heal_target = (effect.int0 == 1) ? source : (target_ally == nullptr ? source : *target_ally);
			double heal_amount = (heal_target.stats_dirty ? get_effective_max_hp(heal_target) : heal_target.cached_max_hp) * effect.scalar0 + heal_target.hp * effect.scalar1 + effect.scalar2;
			double missing_hp = (heal_target.stats_dirty ? get_effective_max_hp(heal_target) : heal_target.cached_max_hp) - heal_target.hp;
			heal_amount += missing_hp * effect.scalar3;
			double old_hp = heal_target.hp;
			sim::status::heal_unit(world, source, heal_target, heal_amount, context.action_kind, false, &host);
			double heal_gained = heal_target.hp - old_hp;
			context.heal_gained += heal_gained;
			sim::combat::run_post_heal_effects(world, host, source, heal_target, heal_amount, heal_gained, context);
			heal_result["heal_applied"] = true;
			heal_result["amount"] = heal_gained;
			return heal_result;
		}
		case EFFECT_OPCODE_DAMAGE_BASED_HEAL: {
			Dictionary heal_result;
			heal_result["success"] = true;
			double damage_to_use = context.damage;
			if (effect.int0 != 0 && context.channel_accumulated_damage > 0.0) {
				damage_to_use = context.channel_accumulated_damage;
			}
			double old_hp = source.hp;
			double heal_amount = damage_to_use * effect.scalar0;
			sim::status::heal_unit(world, source, source, heal_amount, context.action_kind, false, &host);
			double heal_gained = source.hp - old_hp;
			context.heal_gained += heal_gained;
			sim::combat::run_post_heal_effects(world, host, source, source, heal_amount, heal_gained, context);
			heal_result["heal_applied"] = true;
			heal_result["amount"] = heal_gained;
			return heal_result;
		}
		case EFFECT_OPCODE_DAMAGE_BASED_SHIELD: {
			Dictionary shield_result;
			shield_result["success"] = true;
			double damage_to_use = context.damage;
			if (effect.int0 != 0 && context.channel_accumulated_damage > 0.0) {
				damage_to_use = context.channel_accumulated_damage;
			}
			sim::status::add_shield(world, source, source, damage_to_use * effect.scalar0, context.action_kind, &host);
			shield_result["shield_applied"] = true;
			shield_result["amount"] = damage_to_use * effect.scalar0;
			return shield_result;
		}
		case EFFECT_OPCODE_CONSUME_STACKS_HEAL: {
			Dictionary result;
			result["success"] = true;
			double max_hp = source.stats_dirty ? get_effective_max_hp(source) : source.cached_max_hp;
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
			sim::status::heal_unit(world, source, source, heal_amount, context.action_kind, false, &host);
			double heal_gained = source.hp - old_hp;
			context.heal_gained += heal_gained;
			sim::combat::run_post_heal_effects(world, host, source, source, heal_amount, heal_gained, context);
			result["heal_applied"] = true;
			result["amount"] = heal_gained;
			result["stacks_consumed"] = stacks;
			return result;
		}
		case EFFECT_OPCODE_CONSUME_STACKS_SHIELD: {
			Dictionary result;
			result["success"] = true;
			double max_hp = source.stats_dirty ? get_effective_max_hp(source) : source.cached_max_hp;
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
			sim::status::add_shield(world, source, source, shield_amount, context.action_kind, &host);
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
			String stack_key = sim::stats_modifiers::get_stack_key(effect.stat_name, stack_reason);
			Dictionary stack_entry = Dictionary(source.stat_stacks.get(stack_key, Dictionary()));
			int final_stacks = int(stack_entry.get("current_stacks", stack_count));
			result["stacks_set"] = final_stacks;
			return result;
		}
		default:
			break;
	}
	Dictionary fallback_result;
	fallback_result["success"] = false;
	return fallback_result;
}

} // namespace internal
} // namespace sim::effects::execution
