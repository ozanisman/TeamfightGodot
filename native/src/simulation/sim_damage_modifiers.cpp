#include "sim_damage.hpp"
#include "sim_damage_internal.hpp"

#include "sim_constants.hpp"
#include "sim_spatial.hpp"
#include "sim_stats.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim {
namespace damage {

using namespace internal;

double evaluate_multiplier_effect(
		SimWorld &world,
		SimHostCallbacks &host,
		const EffectRecord &effect,
		const EffectContext &context,
		double current_value) {
	(void)world;
	(void)current_value;
	switch (effect.opcode) {
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER:
			return effect.scalar0;
		case EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER: {
			if (effect.scalar0 > 0.0 && context.source != nullptr) {
				const double hp_ratio = context.source->hp / Math::max(0.0001, get_effective_max_hp(*context.source));
				if (hp_ratio > effect.scalar0) {
					return effect.scalar2;
				}
			}
			if (effect.scalar1 > 0.0 && context.target != nullptr) {
				const double target_hp = context.target->hp;
				const double target_max_hp = Math::max(0.0001, get_effective_max_hp(*context.target));
				if (target_hp / target_max_hp <= effect.scalar1) {
					return effect.scalar2;
				}
			}
			return 1.0;
		}
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
			return context.distance > effect.scalar0 ? effect.scalar1 : 1.0;
		case EFFECT_OPCODE_AUTO_DODGE:
			if (host.randf != nullptr && host.randf(host.user_data) < effect.scalar0) {
				return effect.scalar1;
			}
			return effect.scalar2;
		case EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER:
			if (context.target == nullptr) {
				return 1.0;
			}
			return target_has_status(world, *context.target, effect.damage_type) ? effect.scalar0 : 1.0;
		default:
			return 1.0;
	}
}

double defense_multiplier(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &target,
		UnitState &source,
		double damage,
		const StringName &action_kind) {
	double multiplier = 1.0;
	EffectContext context = build_context(source, &target, nullptr, damage, action_kind);
	const std::vector<EffectRecord> &effects = uc(world, target).passive_effects[EFFECT_BUCKET_ON_DEFENSE];
	for (const EffectRecord &effect : effects) {
		if (effect.opcode == EFFECT_OPCODE_AUTO_DODGE) {
			continue;
		}
		if (effect.opcode == EFFECT_OPCODE_STAT_MODIFIER) {
			EffectContext stat_context = context;
			stat_context.source = &target;
			stat_context.target = &target;
			if (host.execute_effect != nullptr) {
				host.execute_effect(host, effect, stat_context);
			}
			continue;
		}
		multiplier *= evaluate_multiplier_effect(world, host, effect, context, multiplier);
	}
	return multiplier;
}

double auto_dodge_multiplier(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &target,
		UnitState &source,
		double damage) {
	double multiplier = 1.0;
	EffectContext context = build_context(source, &target, nullptr, damage, sn_auto());
	const std::vector<EffectRecord> &effects = uc(world, target).passive_effects[EFFECT_BUCKET_ON_DEFENSE];
	for (const EffectRecord &effect : effects) {
		if (effect.opcode != EFFECT_OPCODE_AUTO_DODGE) {
			continue;
		}
		multiplier *= evaluate_multiplier_effect(world, host, effect, context, multiplier);
	}
	return multiplier;
}

double apply_attack_modifiers(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &unit,
		UnitState &target,
		double distance,
		double damage) {
	(void)distance;
	EffectContext context = build_context(unit, &target, nullptr, damage, sn_passive());
	const std::vector<EffectRecord> &effects = uc(world, unit).passive_effects[EFFECT_BUCKET_ON_ATTACK];
	double modified_damage = damage;
	for (const EffectRecord &effect : effects) {
		modified_damage *= evaluate_multiplier_effect(world, host, effect, context, modified_damage);
	}
	return modified_damage;
}

double apply_ability_modifiers(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &unit,
		UnitState *target,
		double damage) {
	EffectContext context = build_context(unit, target, nullptr, damage, sn_ability());
	const std::vector<EffectRecord> &effects = uc(world, unit).passive_effects[EFFECT_BUCKET_ON_ABILITY];
	double modified_damage = damage;
	for (const EffectRecord &effect : effects) {
		modified_damage *= evaluate_multiplier_effect(world, host, effect, context, modified_damage);
	}
	return modified_damage;
}

double apply_ultimate_modifiers(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &unit,
		UnitState *target,
		double damage) {
	EffectContext context = build_context(unit, target, nullptr, damage, sn_ultimate());
	const std::vector<EffectRecord> &effects = uc(world, unit).passive_effects[EFFECT_BUCKET_ON_ULTIMATE];
	double modified_damage = damage;
	for (const EffectRecord &effect : effects) {
		modified_damage *= evaluate_multiplier_effect(world, host, effect, context, modified_damage);
	}
	return modified_damage;
}

} // namespace damage
} // namespace sim
