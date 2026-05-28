#ifndef SIM_DAMAGE_INTERNAL_HPP
#define SIM_DAMAGE_INTERNAL_HPP

#include "sim_world.hpp"

namespace sim {
namespace damage {
namespace internal {

constexpr size_t EFFECT_BUCKET_ON_ATTACK = 0;
constexpr size_t EFFECT_BUCKET_ON_DEFENSE = 1;
constexpr size_t EFFECT_BUCKET_ON_ALLY_DEFENSE = 2;
constexpr size_t EFFECT_BUCKET_ON_ABILITY = 6;
constexpr size_t EFFECT_BUCKET_ON_ULTIMATE = 7;
constexpr size_t EFFECT_BUCKET_POST_TAKE_DAMAGE = 5;

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
