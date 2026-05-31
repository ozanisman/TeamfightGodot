#include "sim_combat_internal.hpp"

#include "sim_status.hpp"
#include "sim_targeting.hpp"

namespace sim {
namespace combat {
namespace internal {

const StringName &sn_player() {
	static const StringName s("player");
	return s;
}
const StringName &sn_enemy() {
	static const StringName s("enemy");
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
const StringName &sn_auto() {
	static const StringName s("auto");
	return s;
}
const StringName &sn_physical() {
	static const StringName s("physical");
	return s;
}
const StringName &sn_passive() {
	static const StringName s("passive");
	return s;
}
const StringName &sn_cast_start() {
	static const StringName s("cast_start");
	return s;
}
const StringName &sn_on_attack() {
	static const StringName s("on_attack");
	return s;
}
const StringName &sn_on_defense() {
	static const StringName s("on_defense");
	return s;
}
const StringName &sn_on_ally_defense() {
	static const StringName s("on_ally_defense");
	return s;
}
const StringName &sn_on_tick() {
	static const StringName s("on_tick");
	return s;
}
const StringName &sn_post_attack() {
	static const StringName s("post_attack");
	return s;
}
const StringName &sn_post_take_damage() {
	static const StringName s("post_take_damage");
	return s;
}
const StringName &sn_on_ability() {
	static const StringName s("on_ability");
	return s;
}
const StringName &sn_on_ultimate() {
	static const StringName s("on_ultimate");
	return s;
}
const StringName &sn_post_heal() {
	static const StringName s("post_heal");
	return s;
}
const StringName &sn_on_takedown() {
	static const StringName s("on_takedown");
	return s;
}

UnitState *unit_by_id(SimWorld &world, int64_t instance_id) {
	return targeting::unit_by_id(world, instance_id);
}

void heal_with_hooks(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double amount,
		const StringName &action_kind,
		bool allow_overheal) {
	status::heal_unit(world, source, target, amount, action_kind, allow_overheal, &host);
}

void emit_trace(SimHostCallbacks &host, const StringName &kind, int64_t src_id, int64_t tgt_id, double val) {
	if (host.emit_trace != nullptr) {
		host.emit_trace(host.user_data, kind, src_id, tgt_id, val);
	}
}

bool effect_uses_projectile(const EffectRecord &e) {
	if (e.opcode == EFFECT_OPCODE_PROJECTILE) {
		return true;
	}
	if (e.opcode == EFFECT_OPCODE_MULTI_EFFECT) {
		for (const EffectRecord &child : e.children) {
			if (effect_uses_projectile(child)) {
				return true;
			}
		}
	}
	if (e.opcode == EFFECT_OPCODE_MULTI_TARGET) {
		for (const EffectRecord &sub_effect : e.sub_effects) {
			if (effect_uses_projectile(sub_effect)) {
				return true;
			}
		}
	}
	return false;
}

} // namespace internal
} // namespace combat
} // namespace sim
