#include "sim_combat.hpp"
#include "sim_combat_internal.hpp"
#include "sim_passive_hooks.hpp"

#include "sim_stats.hpp"

#include <godot_cpp/variant/string_name.hpp>

namespace sim {
namespace combat {

using namespace internal;
using namespace passive_hooks;

EffectContext build_context(UnitState &source, UnitState *target, UnitState *target_ally, double damage, const StringName &action_kind) {
	EffectContext context;
	context.suppress_reflect_chain = false;
	context.source = &source;
	context.target = target;
	context.target_ally = target_ally;
	context.damage = damage;
	context.action_kind = action_kind;
	recompute_distance(context);
	return context;
}

int effect_bucket_index(const StringName &kind) {
	if (kind == sn_on_attack()) {
		return static_cast<int>(EFFECT_BUCKET_ON_ATTACK);
	}
	if (kind == sn_on_defense()) {
		return static_cast<int>(EFFECT_BUCKET_ON_DEFENSE);
	}
	if (kind == sn_on_ally_defense()) {
		return static_cast<int>(EFFECT_BUCKET_ON_ALLY_DEFENSE);
	}
	if (kind == sn_on_tick()) {
		return static_cast<int>(EFFECT_BUCKET_ON_TICK);
	}
	if (kind == sn_post_attack()) {
		return static_cast<int>(EFFECT_BUCKET_POST_ATTACK);
	}
	if (kind == sn_post_take_damage()) {
		return static_cast<int>(EFFECT_BUCKET_POST_TAKE_DAMAGE);
	}
	if (kind == sn_on_ability()) {
		return static_cast<int>(EFFECT_BUCKET_ON_ABILITY);
	}
	if (kind == sn_on_ultimate()) {
		return static_cast<int>(EFFECT_BUCKET_ON_ULTIMATE);
	}
	if (kind == sn_post_heal()) {
		return static_cast<int>(EFFECT_BUCKET_POST_HEAL);
	}
	if (kind == sn_on_takedown()) {
		return static_cast<int>(EFFECT_BUCKET_ON_TAKEDOWN);
	}
	if (kind == sn_on_knockback()) {
		return static_cast<int>(EFFECT_BUCKET_ON_KNOCKBACK);
	}
	if (kind == sn_on_knockback_action()) {
		return static_cast<int>(EFFECT_BUCKET_ON_KNOCKBACK_ACTION);
	}
	return -1;
}

const std::vector<EffectRecord> &collect_effects(const SimWorld &world, const UnitState &unit, const StringName &kind) {
	static const std::vector<EffectRecord> EMPTY_EFFECTS;
	const int bucket = effect_bucket_index(kind);
	if (bucket >= 0 && bucket < static_cast<int>(EFFECT_BUCKET_COUNT)) {
		return uc(world, unit).passive_effects[static_cast<size_t>(bucket)];
	}
	return EMPTY_EFFECTS;
}

bool effect_record_contains_opcode(const EffectRecord &effect, EffectOpcode opcode) {
	const int64_t want = static_cast<int64_t>(opcode);
	if (effect.opcode == want) {
		return true;
	}
	for (const EffectRecord &child : effect.children) {
		if (effect_record_contains_opcode(child, opcode)) {
			return true;
		}
	}
	return false;
}

void run_post_attack_effects(SimWorld &world, SimHostCallbacks &host, UnitState &source, UnitState &target, double damage, EffectContext &context) {
	if (source.stealth_remaining > 0.0 && source.stealth_break_on_attack) {
		source.stealth_remaining = 0.0;
		source.stealth_break_on_attack = false;
		source.stealth_break_on_ability = false;
		source.stealth_break_on_damage_taken = false;
	}
	const std::vector<EffectRecord> &post_attack_effects = uc(world, source).passive_effects[EFFECT_BUCKET_POST_ATTACK];
	if (post_attack_effects.empty()) {
		return;
	}
	Event event;
	event.source = &source;
	event.target = &target;
	event.set_damage = true;
	event.damage = damage;
	event.inherit = ChainInherit::ActionChain;
	event.merge = MergePolicy::ToParent;
	run_bucket(world, host, post_attack_effects, event, &context);
}

void run_post_heal_effects(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double heal_amount,
		double heal_gained,
		const EffectContext &base_context) {
	const std::vector<EffectRecord> &post_heal_effects = uc(world, target).passive_effects[EFFECT_BUCKET_POST_HEAL];
	if (post_heal_effects.empty()) {
		return;
	}
	Event event;
	event.source = &source;
	event.target = &target;
	event.set_heal = true;
	event.heal_amount = heal_amount;
	event.heal_gained = heal_gained;
	event.inherit = ChainInherit::ActionChain;
	EffectContext parent = base_context;
	run_bucket(world, host, post_heal_effects, event, &parent);
}

void run_on_takedown_effects(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &participant,
		UnitState &victim,
		double damage_dealt,
		bool is_kill,
		const EffectContext &base_context) {
	const std::vector<EffectRecord> &takedown_effects = uc(world, participant).passive_effects[EFFECT_BUCKET_ON_TAKEDOWN];
	if (takedown_effects.empty()) {
		return;
	}
	Event event;
	event.source = &participant;
	event.target = &victim;
	event.set_damage = true;
	event.damage = damage_dealt;
	event.set_takedown = true;
	event.takedown_target_id = victim.instance_id;
	event.takedown_damage_dealt = damage_dealt;
	event.is_takedown_kill = is_kill;
	event.inherit = ChainInherit::ActionChain;
	EffectContext parent = base_context;
	run_bucket(world, host, takedown_effects, event, &parent);
}

void run_on_knockback_effects(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		EffectContext &base_context) {
	if (!source.alive) {
		return;
	}
	const std::vector<EffectRecord> &knockback_effects = uc(world, source).passive_effects[EFFECT_BUCKET_ON_KNOCKBACK];
	if (knockback_effects.empty()) {
		return;
	}
	Event event;
	event.source = &source;
	event.target = target;
	event.inherit = ChainInherit::ActionChain;
	event.merge = MergePolicy::ToParent;
	event.set_knockback_hook_depth = true;
	event.knockback_hook_depth = base_context.knockback_hook_depth + 1;
	run_bucket(world, host, knockback_effects, event, &base_context);
}

void run_on_knockback_action_effects(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectContext &base_context) {
	if (!source.alive) {
		return;
	}
	const std::vector<EffectRecord> &knockback_action_effects = uc(world, source).passive_effects[EFFECT_BUCKET_ON_KNOCKBACK_ACTION];
	if (knockback_action_effects.empty()) {
		return;
	}
	Event event;
	event.source = &source;
	event.target = target;
	event.inherit = ChainInherit::ActionChain;
	event.merge = MergePolicy::None;
	event.set_knockback_hook_depth = true;
	event.knockback_hook_depth = base_context.knockback_hook_depth + 1;
	EffectContext parent = base_context;
	run_bucket(world, host, knockback_action_effects, event, &parent);
}

} // namespace combat
} // namespace sim
