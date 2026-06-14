#include "sim_status_internal.hpp"

#include "sim_targeting.hpp"

namespace sim {
namespace status {
namespace internal {

const StringName &sn_player() {
	static const StringName s("player");
	return s;
}

const StringName &sn_enemy() {
	static const StringName s("enemy");
	return s;
}

const StringName &sn_auto() {
	static const StringName s("auto");
	return s;
}

const StringName &sn_ability() {
	static const StringName s("ability");
	return s;
}

const StringName &sn_ultimate() {
	static const StringName s("ultimate");
	return s;
}

const StringName &sn_passive() {
	static const StringName s("passive");
	return s;
}

const StringName &sn_slow() {
	static const StringName s("slow");
	return s;
}

const StringName &sn_root() {
	static const StringName s("root");
	return s;
}

const StringName &sn_silence() {
	static const StringName s("silence");
	return s;
}

const StringName &sn_disarm() {
	static const StringName s("disarm");
	return s;
}

const StringName &sn_stealth() {
	static const StringName s("stealth");
	return s;
}

const StringName &sn_stun() {
	static const StringName s("stun");
	return s;
}

const StringName &sn_reflect() {
	static const StringName s("reflect");
	return s;
}

void record_shielding_by_action_kind(UnitStateRare &source_rare, double amount, const StringName &action_kind) {
	source_rare.shielding_done += amount;
	if (action_kind == sn_auto()) {
		source_rare.shielding_done_auto += amount;
	} else if (action_kind == sn_ability()) {
		source_rare.shielding_done_ability += amount;
	} else if (action_kind == sn_ultimate()) {
		source_rare.shielding_done_ultimate += amount;
	} else if (action_kind == sn_passive()) {
		source_rare.shielding_done_passive += amount;
	}
}

void record_healing_by_action_kind(UnitStateRare &source_rare, double gained, const StringName &action_kind) {
	source_rare.healing_done += gained;
	if (action_kind == sn_auto()) {
		source_rare.healing_done_auto += gained;
	} else if (action_kind == sn_ability()) {
		source_rare.healing_done_ability += gained;
	} else if (action_kind == sn_ultimate()) {
		source_rare.healing_done_ultimate += gained;
	} else if (action_kind == sn_passive()) {
		source_rare.healing_done_passive += gained;
	}
}

void record_benefactor(SimWorld &world, UnitState &source, UnitState &target) {
	if (source.instance_id != target.instance_id) {
		ur(world, target).recent_benefactors[source.instance_id] = world.time;
	}
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

} // namespace internal
} // namespace status
} // namespace sim
