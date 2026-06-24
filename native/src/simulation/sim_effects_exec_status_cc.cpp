#include "sim_effects_exec_internal.hpp"

#include "sim_combat.hpp"
#include "sim_periodic.hpp"
#include "sim_status.hpp"

#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::execution {
namespace internal {

Dictionary exec_status_cc(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target) {
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
				sim::status::apply_slow(world, source, *target, effect.scalar0, effect.scalar1, effect.reason);
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
		case EFFECT_OPCODE_KNOCKBACK: {
			Dictionary kb_result;
			kb_result["success"] = false;
			if (target != nullptr && target->alive) {
				bool knocked_back = sim::periodic::apply_knockback(world, host, source, *target, effect.scalar0, effect.int0 != 0);
				kb_result["success"] = true;
				kb_result["knockback_applied"] = knocked_back;
				if (knocked_back) {
					context.knockback_applied = true;
					if (context.knockback_hook_depth == 0) {
						sim::combat::run_on_knockback_effects(world, host, source, target, context);
					}
				}
			}
			return kb_result;
		}
		case EFFECT_OPCODE_REFLECT: {
			Dictionary rf_result;
			rf_result["success"] = true;
			sim::periodic::apply_reflect_buff(world, source, source, effect.scalar0, effect.scalar1, context.action_kind, effect.damage_type, effect.reason);
			return rf_result;
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
