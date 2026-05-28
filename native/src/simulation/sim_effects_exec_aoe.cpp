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

Dictionary exec_aoe(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, UnitState &source, UnitState *target, UnitState *target_ally) {
	switch (effect.opcode) {
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
	}
	UtilityFunctions::push_error(vformat("exec_aoe: unhandled opcode %d", effect.opcode));
	Dictionary fallback_result;
	fallback_result["success"] = false;
	fallback_result["error"] = "Unhandled opcode in exec_aoe";
	return fallback_result;
}

} // namespace internal
} // namespace sim::effects::execution
