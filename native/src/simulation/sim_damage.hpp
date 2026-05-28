#ifndef SIM_DAMAGE_HPP
#define SIM_DAMAGE_HPP

#include "sim_world.hpp"

namespace sim::damage {

double evaluate_multiplier_effect(
		SimWorld &world,
		SimHostCallbacks &host,
		const EffectRecord &effect,
		const EffectContext &context,
		double current_value);

double defense_multiplier(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &target,
		UnitState &source,
		double damage,
		const StringName &action_kind);

double auto_dodge_multiplier(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &target,
		UnitState &source,
		double damage);

double apply_attack_modifiers(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &unit,
		UnitState &target,
		double distance,
		double damage);

double apply_ability_modifiers(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &unit,
		UnitState *target,
		double damage);

double apply_ultimate_modifiers(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &unit,
		UnitState *target,
		double damage);

double apply_damage(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double damage,
		const StringName &damage_type,
		const StringName &action_kind,
		const EffectContext &context);

void maybe_apply_reflect_damage(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &attacker,
		UnitState &defender,
		double total_damage_applied,
		const StringName &damage_type,
		const EffectContext &context);

void touch_damage_source(SimWorld &world, UnitState &target, int64_t source_id, double incoming_damage);

double trigger_ally_defense_effects(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &target,
		UnitState &source,
		double damage,
		const StringName &damage_type,
		const StringName &action_kind,
		const EffectContext &context);

} // namespace sim::damage

#endif // SIM_DAMAGE_HPP
