#ifndef SIM_PERIODIC_HPP
#define SIM_PERIODIC_HPP

#include "sim_aoe.hpp"
#include "sim_world.hpp"

namespace sim {
namespace periodic {

void apply_dot(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double attack_damage_ratio,
		double max_hp_ratio,
		double flat_amount,
		double duration,
		double tick_interval,
		const StringName &damage_type,
		const StringName &stacking_mode,
		int max_stacks,
		const StringName &effect_type,
		const String &reason,
		const StringName &action_kind,
		bool is_dynamic = false);

void apply_hot(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double max_hp_ratio,
		double current_hp_ratio,
		double missing_hp_ratio,
		double flat_amount,
		double duration,
		double tick_interval,
		const StringName &stacking_mode,
		int max_stacks,
		bool allow_overheal,
		const StringName &effect_type,
		const String &reason,
		const StringName &action_kind,
		bool is_dynamic = false);

void tick_periodic_effects(SimWorld &world, SimHostCallbacks &host, UnitState &unit, double delta);
void cleanse_dots(SimWorld &world, UnitState &unit, const StringName &effect_type_filter);
void clear_periodic_effects(SimWorld &world, UnitState &unit);

void apply_aoe_dot(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		double radius,
		double attack_damage_ratio,
		double max_hp_ratio,
		double flat_amount,
		double duration,
		double tick_interval,
		const StringName &damage_type,
		const StringName &stacking_mode,
		int max_stacks,
		const StringName &effect_type,
		const String &reason,
		bool target_self,
		const StringName &action_kind,
		bool is_dynamic = false);

void apply_aoe_dot_shape(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double attack_damage_ratio,
		double max_hp_ratio,
		double flat_amount,
		double duration,
		double tick_interval,
		const StringName &damage_type,
		const StringName &stacking_mode,
		int max_stacks,
		const StringName &effect_type,
		const String &reason,
		bool target_self,
		const StringName &action_kind,
		bool is_dynamic = false);

void apply_aoe_hot(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		double radius,
		double max_hp_ratio,
		double current_hp_ratio,
		double missing_hp_ratio,
		double flat_amount,
		double duration,
		double tick_interval,
		const StringName &stacking_mode,
		int max_stacks,
		bool allow_overheal,
		const StringName &effect_type,
		const String &reason,
		bool target_self,
		const StringName &action_kind,
		bool is_dynamic = false);

void apply_aoe_hot_shape(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double max_hp_ratio,
		double current_hp_ratio,
		double missing_hp_ratio,
		double flat_amount,
		double duration,
		double tick_interval,
		const StringName &stacking_mode,
		int max_stacks,
		bool allow_overheal,
		const StringName &effect_type,
		const String &reason,
		bool target_self,
		const StringName &action_kind,
		bool is_dynamic = false);

void apply_aoe_taunt(SimWorld &world, UnitState &source, double radius, double duration);
void apply_aoe_taunt_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration, const SimHostCallbacks *host = nullptr);

double apply_aoe_damage(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &center_source,
		double damage,
		double radius,
		const StringName &damage_type,
		const StringName &reason,
		const StringName &action_kind);

double apply_aoe_damage_shape(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double damage,
		const StringName &damage_type,
		const StringName &action_kind);

double apply_aoe_damage_shape_per_target(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double damage_ratio,
		double flat_amount,
		double max_hp_ratio,
		double splash_ratio,
		const StringName &damage_type,
		const StringName &action_kind);

bool apply_knockback(SimWorld &world, SimHostCallbacks &host, UnitState &source, UnitState &target, double distance, bool away_from_source);
bool apply_aoe_knockback(SimWorld &world, SimHostCallbacks &host, UnitState &source, double radius, double distance, bool away_from_source);
bool apply_aoe_knockback_shape(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double distance,
		bool away_from_source);

void apply_reflect_buff(
		SimWorld &world,
		UnitState &source,
		UnitState &target,
		double pct,
		double duration,
		const StringName &action_kind,
		const StringName &damage_type,
		const String &reason);

void apply_aoe_reflect_shape(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double pct,
		double duration,
		bool all_damage_types,
		const StringName &action_kind,
		const String &reason);

} // namespace periodic
} // namespace sim

#endif // SIM_PERIODIC_HPP
