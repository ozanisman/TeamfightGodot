#include "sim_effects_exec_internal.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::execution {
namespace internal {

Dictionary exec_status(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		const ::sim::effects::SimMatchHost & /*match_host*/,
		UnitState &source,
		UnitState *target,
		UnitState *target_ally) {
	switch (effect.opcode) {
		case EFFECT_OPCODE_STUN:
		case EFFECT_OPCODE_SLOW:
		case EFFECT_OPCODE_ROOT:
		case EFFECT_OPCODE_SILENCE:
		case EFFECT_OPCODE_DISARM:
		case EFFECT_OPCODE_STEALTH:
		case EFFECT_OPCODE_EVERY_N_ATTACKS_STUN:
		case EFFECT_OPCODE_KNOCKBACK_SHIELD:
		case EFFECT_OPCODE_KNOCKBACK:
		case EFFECT_OPCODE_REFLECT:
			return exec_status_cc(effect, context, world, host, source, target);
		case EFFECT_OPCODE_SHIELD:
		case EFFECT_OPCODE_HEAL:
		case EFFECT_OPCODE_DAMAGE_BASED_HEAL:
		case EFFECT_OPCODE_DAMAGE_BASED_SHIELD:
		case EFFECT_OPCODE_CONSUME_STACKS_HEAL:
		case EFFECT_OPCODE_CONSUME_STACKS_SHIELD:
		case EFFECT_OPCODE_SET_STACKS:
			return exec_status_heal_shield(effect, context, world, host, hooks, source, target, target_ally);
		case EFFECT_OPCODE_MANA_REGEN:
		case EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN:
		case EFFECT_OPCODE_MANA_RESTORE_ON_HIT:
		case EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT:
			return exec_status_mana(effect, context, world, source, target);
		case EFFECT_OPCODE_CHANNEL:
		case EFFECT_OPCODE_SELF_DASH:
			return exec_status_channel(effect, context, world, host, hooks, source, target);
		default:
			break;
	}
	UtilityFunctions::push_error(vformat("exec_status: unhandled opcode %d", effect.opcode));
	Dictionary fallback_result;
	fallback_result["success"] = false;
	fallback_result["error"] = "Unhandled opcode in exec_status";
	return fallback_result;
}

} // namespace internal
} // namespace sim::effects::execution
