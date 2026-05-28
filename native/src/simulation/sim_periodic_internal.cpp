#include "sim_periodic_internal.hpp"

#include "sim_combat.hpp"
#include "sim_status.hpp"
#include "sim_targeting.hpp"

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

void sync_targeting_if_healed(SimHostCallbacks &host, UnitState &target, double gained) {
	if (gained > 1e-9 && host.sync_targeting_frame_unit != nullptr) {
		host.sync_targeting_frame_unit(host.user_data, target);
	}
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
	const double gained = status::heal_unit(world, source, target, heal_per_tick, action_kind, allow_overheal);
	sync_targeting_if_healed(host, target, gained);
	combat::run_post_heal_effects(world, host, source, target, heal_per_tick, gained, context);
}


} // namespace internal
} // namespace periodic
} // namespace sim
