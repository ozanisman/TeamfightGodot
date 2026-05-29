#include "sim_effects_exec_internal.hpp"

#include "sim_stats.hpp"
#include "sim_status.hpp"

#include <godot_cpp/core/math.hpp>

#define X(name, def, min_val, max_val) using sim::get_effective_##name;
STAT_LIST
#undef X

namespace sim::effects::execution {
namespace internal {

Dictionary exec_status_mana(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		UnitState &source,
		UnitState *target) {
	switch (effect.opcode) {
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
		default:
			break;
	}
	Dictionary fallback_result;
	fallback_result["success"] = false;
	return fallback_result;
}

} // namespace internal
} // namespace sim::effects::execution
