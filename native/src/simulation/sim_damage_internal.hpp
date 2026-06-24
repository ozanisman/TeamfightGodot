#ifndef SIM_DAMAGE_INTERNAL_HPP
#define SIM_DAMAGE_INTERNAL_HPP

#include "sim_world.hpp"

namespace sim {
namespace damage {
namespace internal {

using sim::EFFECT_BUCKET_ON_ATTACK;
using sim::EFFECT_BUCKET_ON_DEFENSE;
using sim::EFFECT_BUCKET_ON_ALLY_DEFENSE;
using sim::EFFECT_BUCKET_ON_ABILITY;
using sim::EFFECT_BUCKET_ON_ULTIMATE;
using sim::EFFECT_BUCKET_POST_TAKE_DAMAGE;
using sim::EFFECT_BUCKET_COUNT;

const StringName &sn_auto();
const StringName &sn_ability();
const StringName &sn_ultimate();
const StringName &sn_passive();
const StringName &sn_physical();
const StringName &sn_magic();
const StringName &sn_true();

EffectContext build_context(
		UnitState &source,
		UnitState *target,
		UnitState *target_ally,
		double damage,
		const StringName &action_kind);

UnitState *unit_by_id(SimWorld &world, int64_t instance_id);
bool target_has_status(const SimWorld &world, const UnitState &target, const StringName &status_kind);

void run_post_take_damage_passives(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &target,
		double total_damage,
		const EffectContext &context);

} // namespace internal
} // namespace damage
} // namespace sim

#endif // SIM_DAMAGE_INTERNAL_HPP
