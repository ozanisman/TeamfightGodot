#ifndef SIM_STATUS_HPP
#define SIM_STATUS_HPP

#include "sim_aoe.hpp"
#include "sim_world.hpp"

namespace sim {
namespace status {

bool target_has_status(const SimWorld &world, const UnitState &target, const StringName &status_kind);

void apply_stun(SimWorld &world, UnitState &source, UnitState &target, double duration);
void apply_slow(SimWorld &world, UnitState &source, UnitState &target, double slow_percentage, double duration);
void apply_root(SimWorld &world, UnitState &source, UnitState &target, double duration);
void apply_silence(SimWorld &world, UnitState &source, UnitState &target, double duration, bool block_abilities, bool block_ultimate);
void apply_disarm(SimWorld &world, UnitState &source, UnitState &target, double duration);
void apply_stealth(SimWorld &world, UnitState &source, UnitState &target, double duration, bool break_on_attack, bool break_on_ability, bool break_on_damage_taken);

void add_shield(SimWorld &world, UnitState &source, UnitState &target, double amount, const StringName &action_kind, SimHostCallbacks *host = nullptr);
double heal_unit(SimWorld &world, UnitState &source, UnitState &target, double amount, const StringName &action_kind, bool allow_overheal = false, SimHostCallbacks *host = nullptr);
void restore_mana(SimWorld &world, UnitState &source, UnitState &target, double amount);

Vector2 resolve_aoe_direction(const SimWorld &world, const UnitState &source, const AoShapeParams &params, const UnitState *target_override = nullptr);

void apply_aoe_slow(SimWorld &world, UnitState &source, double radius, double slow_percentage, double duration);
void apply_aoe_slow_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double slow_percentage, double duration, const SimHostCallbacks *host = nullptr);
void apply_aoe_root(SimWorld &world, UnitState &source, double radius, double duration);
void apply_aoe_root_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration, const SimHostCallbacks *host = nullptr);
void apply_aoe_silence(SimWorld &world, UnitState &source, double radius, double duration, bool block_abilities, bool block_ultimate);
void apply_aoe_silence_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration, bool block_abilities, bool block_ultimate, const SimHostCallbacks *host = nullptr);
void apply_aoe_disarm(SimWorld &world, UnitState &source, double radius, double duration);
void apply_aoe_disarm_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration, const SimHostCallbacks *host = nullptr);
void apply_aoe_stun(SimWorld &world, UnitState &source, double radius, double duration);
void apply_aoe_stun_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration, const SimHostCallbacks *host = nullptr);

} // namespace status
} // namespace sim

#endif // SIM_STATUS_HPP
