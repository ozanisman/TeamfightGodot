#include "sim_passive_hooks.hpp"

#include "sim_combat_internal.hpp"
#include "sim_effects_compile.hpp"
#include "sim_stats.hpp"

#include <godot_cpp/variant/dictionary.hpp>

namespace sim {
namespace combat {
namespace passive_hooks {

namespace {

using sim::effects::compile::result_slot_key;

void merge_accumulated_results(Dictionary &target, const Dictionary &source) {
	if (source.is_empty()) {
		return;
	}
	const Array keys = source.keys();
	for (int64_t i = 0; i < keys.size(); ++i) {
		const Variant key = keys[i];
		target[key] = source[key];
	}
}

void inherit_chain_fields(EffectContext &scratch, const EffectContext &parent, ChainInherit inherit) {
	const uint8_t flags = static_cast<uint8_t>(inherit);
	if ((flags & static_cast<uint8_t>(ChainInherit::Damage)) != 0) {
		scratch.damage = parent.damage;
	}
	if ((flags & static_cast<uint8_t>(ChainInherit::AccumulatedResults)) != 0) {
		scratch.accumulated_results = parent.accumulated_results.duplicate(true);
	}
	if ((flags & static_cast<uint8_t>(ChainInherit::KnockbackApplied)) != 0) {
		scratch.knockback_applied = parent.knockback_applied;
	}
}

} // namespace

void apply_isolation_defaults(EffectContext &ctx, const Event &event) {
	ctx.channel_remaining_duration = 0.0;
	ctx.channel_tick_count = 0;
	ctx.channel_accumulated_damage = 0.0;
	ctx.channel_completed = false;
	ctx.suppress_reflect_chain = false;
	ctx.target_ally = nullptr;
	ctx.use_heal_gained = false;
	ctx.multi_effect_has_remaining_siblings = false;
	ctx.track_spawned_projectile_for_deferred_chain = false;

	if (!event.set_heal) {
		ctx.heal_amount = 0.0;
		ctx.heal_gained = 0.0;
	}
	if (!event.set_takedown) {
		ctx.takedown_target_id = 0;
		ctx.takedown_damage_dealt = 0.0;
		ctx.is_takedown_kill = false;
	}
	if (!event.set_knockback_hook_depth) {
		ctx.knockback_hook_depth = 0;
	}
}

void recompute_distance(EffectContext &ctx) {
	if (ctx.source != nullptr && ctx.target != nullptr) {
		ctx.distance = distance_between_coords(
				ctx.source->pos_x,
				ctx.source->pos_y,
				ctx.target->pos_x,
				ctx.target->pos_y);
	} else {
		ctx.distance = 0.0;
	}
}

EffectContext build_scratch_context(const Event &event, const EffectContext *parent) {
	EffectContext scratch;
	apply_isolation_defaults(scratch, event);

	if (parent != nullptr && event.inherit != ChainInherit::None) {
		inherit_chain_fields(scratch, *parent, event.inherit);
	}

	scratch.source = event.source;
	scratch.target = event.target;
	scratch.target_ally = event.target_ally;

	if (event.set_damage) {
		scratch.damage = event.damage;
	}
	if (event.set_heal) {
		scratch.heal_amount = event.heal_amount;
		scratch.heal_gained = event.heal_gained;
	}
	if (event.set_takedown) {
		scratch.takedown_target_id = event.takedown_target_id;
		scratch.takedown_damage_dealt = event.takedown_damage_dealt;
		scratch.is_takedown_kill = event.is_takedown_kill;
	}
	if (event.set_knockback_hook_depth) {
		scratch.knockback_hook_depth = event.knockback_hook_depth;
	}

	if (event.preserve_action_kind) {
		scratch.action_kind = event.action_kind;
	} else {
		scratch.action_kind = internal::sn_passive();
	}

	recompute_distance(scratch);
	return scratch;
}

void merge_scratch_to_parent(const EffectContext &scratch, const double scratch_damage_before, EffectContext &parent) {
	if (scratch.damage > scratch_damage_before) {
		parent.damage += scratch.damage - scratch_damage_before;
	}
	parent.knockback_applied = parent.knockback_applied || scratch.knockback_applied;
	merge_accumulated_results(parent.accumulated_results, scratch.accumulated_results);
}

void run_bucket(
		SimWorld &world,
		SimHostCallbacks &host,
		const std::vector<EffectRecord> &effects,
		const Event &event,
		EffectContext *parent) {
	(void)world;
	if (effects.empty() || host.execute_effect == nullptr) {
		return;
	}

	EffectContext scratch = build_scratch_context(event, parent);
	const double scratch_damage_before = scratch.damage;

	for (const EffectRecord &effect : effects) {
		Dictionary result = host.execute_effect(host, effect, scratch);
		const StringName &effect_key = result_slot_key(effect);
		if (!effect_key.is_empty()) {
			scratch.accumulated_results[effect_key] = result;
		}
	}

	if (parent != nullptr && event.merge == MergePolicy::ToParent) {
		merge_scratch_to_parent(scratch, scratch_damage_before, *parent);
	}
}

} // namespace passive_hooks
} // namespace combat
} // namespace sim
