#include "sim_periodic_internal.hpp"

#include "sim_combat.hpp"
#include "sim_status.hpp"
#include "sim_targeting.hpp"

#include <algorithm>
#include <godot_cpp/core/math.hpp>

namespace sim {
namespace periodic {
namespace internal {

const StringName &sn_player() {
	static const StringName s("player");
	return s;
}

const StringName &sn_enemy() {
	static const StringName s("enemy");
	return s;
}

const StringName &sn_taunt() {
	static const StringName s("taunt");
	return s;
}

UnitState *resolve_shape_target(SimWorld &world, UnitState *target, const EffectRecord &effect) {
	if (target != nullptr) {
		return target;
	}
	if (effect.aoe_shape_params.target_id == 0) {
		return nullptr;
	}
	return targeting::unit_by_id(world, effect.aoe_shape_params.target_id);
}

EffectRecord make_circle_self_aoe(double radius) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	return effect;
}

void apply_hot_tick_heal(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double heal_per_tick,
		const StringName &action_kind,
		bool allow_overheal) {
	EffectContext context = combat::build_context(source, &target, nullptr, 0.0, action_kind);
	const double gained = status::heal_unit(world, source, target, heal_per_tick, action_kind, allow_overheal, &host);
	combat::run_post_heal_effects(world, host, source, target, heal_per_tick, gained, context);
}

} // namespace internal

void cleanse_dots(SimWorld &world, UnitState &unit, const StringName &effect_type_filter) {
	auto &periodic_effects = uc(world, unit).periodic_effects;
	if (effect_type_filter.is_empty()) {
		periodic_effects.erase(
				std::remove_if(periodic_effects.begin(), periodic_effects.end(), [](const UnitStateCold::PeriodicEffect &e) { return e.damage_total > 0.0; }),
				periodic_effects.end());
	} else {
		periodic_effects.erase(
				std::remove_if(
						periodic_effects.begin(),
						periodic_effects.end(),
						[&effect_type_filter](const UnitStateCold::PeriodicEffect &e) { return e.damage_total > 0.0 && e.effect_type == effect_type_filter; }),
				periodic_effects.end());
	}
}

void cleanse_hots(SimWorld &world, UnitState &unit, const StringName &effect_type_filter) {
	auto &periodic_effects = uc(world, unit).periodic_effects;
	if (effect_type_filter.is_empty()) {
		periodic_effects.erase(
				std::remove_if(periodic_effects.begin(), periodic_effects.end(), [](const UnitStateCold::PeriodicEffect &e) { return e.heal_total > 0.0; }),
				periodic_effects.end());
	} else {
		periodic_effects.erase(
				std::remove_if(
						periodic_effects.begin(),
						periodic_effects.end(),
						[&effect_type_filter](const UnitStateCold::PeriodicEffect &e) { return e.heal_total > 0.0 && e.effect_type == effect_type_filter; }),
				periodic_effects.end());
	}
}

void clear_periodic_effects(SimWorld &world, UnitState &unit) {
	uc(world, unit).periodic_effects.clear();
}

} // namespace periodic
} // namespace sim
