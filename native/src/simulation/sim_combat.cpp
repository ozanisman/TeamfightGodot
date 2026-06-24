#include "sim_combat.hpp"
#include "sim_combat_internal.hpp"

#include "sim_stats.hpp"

#include <godot_cpp/variant/string_name.hpp>

namespace sim {
namespace combat {

using namespace internal;

EffectContext build_context(UnitState &source, UnitState *target, UnitState *target_ally, double damage, const StringName &action_kind) {
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
	EffectContext effect_context = context;
	effect_context.damage = damage;
	effect_context.action_kind = sn_passive();
	for (const EffectRecord &effect : post_attack_effects) {
		if (host.execute_effect != nullptr) {
			host.execute_effect(host, effect, effect_context);
		}
	}
	context.knockback_applied = context.knockback_applied || effect_context.knockback_applied;
	if (effect_context.damage > damage) {
		context.damage += effect_context.damage - damage;
	}
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
	EffectContext effect_context = base_context;
	effect_context.heal_amount = heal_amount;
	effect_context.heal_gained = heal_gained;
	effect_context.action_kind = sn_passive();
	effect_context.source = &source;
	effect_context.target = &target;
	for (const EffectRecord &effect : post_heal_effects) {
		if (host.execute_effect != nullptr) {
			host.execute_effect(host, effect, effect_context);
		}
	}
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
	EffectContext effect_context = base_context;
	effect_context.takedown_target_id = victim.instance_id;
	effect_context.takedown_damage_dealt = damage_dealt;
	effect_context.is_takedown_kill = is_kill;
	effect_context.damage = damage_dealt;
	effect_context.action_kind = sn_passive();
	effect_context.source = &participant;
	effect_context.target = &victim;
	for (const EffectRecord &effect : takedown_effects) {
		if (host.execute_effect != nullptr) {
			host.execute_effect(host, effect, effect_context);
		}
	}
}

static void sanitize_knockback_effect_context(EffectContext &effect_context) {
	// Clear action-specific fields (distance, heal, etc.) but keep knockback_applied and damage.
	// knockback_applied tells hook effects they are running in a knockback context.
	// damage is preserved so effects can build on the cumulative damage of the triggering action.
	// knockback_hook_depth is the actual recursion guard.
	// TODO: Only damage is cumulative across effects. If the design is extended to heal/distance/other
	// EffectContext fields, update this sanitizer and the relevant opcodes to preserve/accumulate them.
	effect_context.distance = 0.0;
	effect_context.target_ally = nullptr;
	effect_context.heal_amount = 0.0;
	effect_context.heal_gained = 0.0;
	effect_context.use_heal_gained = false;
	effect_context.takedown_target_id = 0;
	effect_context.takedown_damage_dealt = 0.0;
	effect_context.is_takedown_kill = false;
	effect_context.suppress_reflect_chain = false;
}

void run_on_knockback_effects(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectContext &base_context) {
	if (!source.alive) {
		return;
	}
	const std::vector<EffectRecord> &knockback_effects = uc(world, source).passive_effects[EFFECT_BUCKET_ON_KNOCKBACK];
	if (knockback_effects.empty()) {
		return;
	}
	EffectContext effect_context = base_context;
	effect_context.action_kind = sn_passive();
	effect_context.source = &source;
	effect_context.target = target;
	sanitize_knockback_effect_context(effect_context);
	// Bump depth so nested knockbacks do not re-enter the same hooks; knockback_applied stays true to preserve context.
	effect_context.knockback_hook_depth = base_context.knockback_hook_depth + 1;
	for (const EffectRecord &effect : knockback_effects) {
		if (host.execute_effect != nullptr) {
			host.execute_effect(host, effect, effect_context);
		}
	}
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
	EffectContext effect_context = base_context;
	effect_context.action_kind = sn_passive();
	effect_context.source = &source;
	effect_context.target = target;
	sanitize_knockback_effect_context(effect_context);
	// Bump depth so nested knockbacks do not re-enter the same hooks; knockback_applied stays true to preserve context.
	effect_context.knockback_hook_depth = base_context.knockback_hook_depth + 1;
	for (const EffectRecord &effect : knockback_action_effects) {
		if (host.execute_effect != nullptr) {
			host.execute_effect(host, effect, effect_context);
		}
	}
}

} // namespace combat
} // namespace sim
