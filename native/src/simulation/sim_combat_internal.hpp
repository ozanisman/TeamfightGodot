#ifndef SIM_COMBAT_INTERNAL_HPP
#define SIM_COMBAT_INTERNAL_HPP

#include "sim_world.hpp"

#include <godot_cpp/variant/string_name.hpp>

namespace sim {
namespace combat {
namespace internal {

using sim::EFFECT_BUCKET_ON_ATTACK;
using sim::EFFECT_BUCKET_ON_DEFENSE;
using sim::EFFECT_BUCKET_ON_ALLY_DEFENSE;
using sim::EFFECT_BUCKET_ON_TICK;
using sim::EFFECT_BUCKET_POST_ATTACK;
using sim::EFFECT_BUCKET_POST_TAKE_DAMAGE;
using sim::EFFECT_BUCKET_ON_ABILITY;
using sim::EFFECT_BUCKET_ON_ULTIMATE;
using sim::EFFECT_BUCKET_POST_HEAL;
using sim::EFFECT_BUCKET_ON_TAKEDOWN;
using sim::EFFECT_BUCKET_ON_KNOCKBACK;
using sim::EFFECT_BUCKET_ON_KNOCKBACK_ACTION;
using sim::EFFECT_BUCKET_COUNT;

const StringName &sn_player();
const StringName &sn_enemy();
const StringName &sn_ability();
const StringName &sn_ultimate();
const StringName &sn_auto();
const StringName &sn_physical();
const StringName &sn_passive();
const StringName &sn_cast_start();
const StringName &sn_on_attack();
const StringName &sn_on_defense();
const StringName &sn_on_ally_defense();
const StringName &sn_on_tick();
const StringName &sn_post_attack();
const StringName &sn_post_take_damage();
const StringName &sn_on_ability();
const StringName &sn_on_ultimate();
const StringName &sn_post_heal();
const StringName &sn_on_takedown();
const StringName &sn_on_knockback();
const StringName &sn_on_knockback_action();
const StringName &sn_homing();
const StringName &sn_target_only();
const StringName &sn_drop();

UnitState *unit_by_id(SimWorld &world, int64_t instance_id);
void heal_with_hooks(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double amount,
		const StringName &action_kind,
		bool allow_overheal);
void emit_trace(SimHostCallbacks &host, const StringName &kind, int64_t src_id, int64_t tgt_id, double val);
bool effect_uses_projectile(const EffectRecord &e);
EffectCastRangeSpec compile_cast_range_spec(const EffectRecord &e);
bool is_in_cast_range(
		SimWorld &world,
		const UnitState &caster,
		const EffectCastRangeSpec &spec,
		const UnitState *enemy_target,
		int64_t current_ally_target_id,
		double cast_range);
int64_t snapshot_ally_cast_target_id(int64_t current_ally_target_id, const EffectCastRangeSpec &spec);

} // namespace internal
} // namespace combat
} // namespace sim

#endif // SIM_COMBAT_INTERNAL_HPP
