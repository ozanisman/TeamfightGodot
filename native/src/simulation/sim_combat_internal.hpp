#ifndef SIM_COMBAT_INTERNAL_HPP
#define SIM_COMBAT_INTERNAL_HPP

#include "sim_world.hpp"

#include <godot_cpp/variant/string_name.hpp>

namespace sim {
namespace combat {
namespace internal {

constexpr size_t EFFECT_BUCKET_ON_ATTACK = 0;
constexpr size_t EFFECT_BUCKET_ON_DEFENSE = 1;
constexpr size_t EFFECT_BUCKET_ON_ALLY_DEFENSE = 2;
constexpr size_t EFFECT_BUCKET_ON_TICK = 3;
constexpr size_t EFFECT_BUCKET_POST_ATTACK = 4;
constexpr size_t EFFECT_BUCKET_POST_TAKE_DAMAGE = 5;
constexpr size_t EFFECT_BUCKET_ON_ABILITY = 6;
constexpr size_t EFFECT_BUCKET_ON_ULTIMATE = 7;
constexpr size_t EFFECT_BUCKET_POST_HEAL = 8;
constexpr size_t EFFECT_BUCKET_ON_TAKEDOWN = 9;

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

} // namespace internal
} // namespace combat
} // namespace sim

#endif // SIM_COMBAT_INTERNAL_HPP
