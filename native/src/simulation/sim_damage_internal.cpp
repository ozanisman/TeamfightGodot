#include "sim_damage_internal.hpp"

#include "sim_stats.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim {
namespace damage {
namespace internal {

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
const StringName &sn_physical() {
	static const StringName s("physical");
	return s;
}
const StringName &sn_magic() {
	static const StringName s("magic");
	return s;
}
const StringName &sn_true() {
	static const StringName s("true");
	return s;
}

static const StringName &sn_slow() {
	static const StringName s("slow");
	return s;
}
static const StringName &sn_root() {
	static const StringName s("root");
	return s;
}
static const StringName &sn_silence() {
	static const StringName s("silence");
	return s;
}
static const StringName &sn_disarm() {
	static const StringName s("disarm");
	return s;
}
static const StringName &sn_stealth() {
	static const StringName s("stealth");
	return s;
}
static const StringName &sn_stun() {
	static const StringName s("stun");
	return s;
}
static const StringName &sn_reflect() {
	static const StringName s("reflect");
	return s;
}

EffectContext build_context(
		UnitState &source,
		UnitState *target,
		UnitState *target_ally,
		double damage,
		const StringName &action_kind) {
	EffectContext context;
	context.suppress_reflect_chain = false;
	context.source = &source;
	context.target = target;
	context.target_ally = target_ally;
	context.damage = damage;
	context.action_kind = action_kind;
	if (target != nullptr) {
		context.distance = distance_between_coords(source.pos_x, source.pos_y, target->pos_x, target->pos_y);
	}
	return context;
}

UnitState *unit_by_id(SimWorld &world, int64_t instance_id) {
	auto it = world.unit_index_map.find(instance_id);
	if (it == world.unit_index_map.end()) {
		return nullptr;
	}
	const int64_t index = it->second;
	if (index < 0 || index >= int64_t(world.units.size())) {
		return nullptr;
	}
	return &world.units[static_cast<size_t>(index)];
}

bool target_has_status(const SimWorld &world, const UnitState &target, const StringName &status_kind) {
	if (status_kind == sn_slow()) {
		return target.slow_remaining > 0.0;
	}
	if (status_kind == sn_root()) {
		return target.root_remaining > 0.0;
	}
	if (status_kind == sn_silence()) {
		return target.silence_remaining > 0.0;
	}
	if (status_kind == sn_disarm()) {
		return target.disarm_remaining > 0.0;
	}
	if (status_kind == sn_stealth()) {
		return target.stealth_remaining > 0.0;
	}
	if (status_kind == sn_stun()) {
		return target.stun_remaining > 0.0;
	}
	if (status_kind == sn_reflect()) {
		return !uc(world, target).reflect_buffs.empty() || !uc(world, target).passive_reflect_entries.empty();
	}
	return false;
}

void run_post_take_damage_passives(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &target,
		double total_damage,
		const EffectContext &context) {
	const std::vector<EffectRecord> &post_take_damage_effects = uc(world, target).passive_effects[EFFECT_BUCKET_POST_TAKE_DAMAGE];
	EffectContext post_context = context;
	post_context.source = &target;
	post_context.target = nullptr;
	post_context.damage = total_damage;
	post_context.action_kind = sn_passive();
	if (target.stealth_remaining > 0.0 && target.stealth_break_on_damage_taken) {
		target.stealth_remaining = 0.0;
		target.stealth_break_on_attack = false;
		target.stealth_break_on_ability = false;
		target.stealth_break_on_damage_taken = false;
	}
	if (host.execute_effect == nullptr) {
		return;
	}
	for (const EffectRecord &effect : post_take_damage_effects) {
		host.execute_effect(host, effect, post_context);
	}
}

} // namespace internal
} // namespace damage
} // namespace sim
