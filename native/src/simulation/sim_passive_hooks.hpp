#ifndef SIM_PASSIVE_HOOKS_HPP
#define SIM_PASSIVE_HOOKS_HPP

#include "sim_world.hpp"

#include <cstdint>
#include <vector>

namespace sim {
namespace combat {
namespace passive_hooks {

/// Chain fields copied from a parent action context into hook scratch.
enum class ChainInherit : uint8_t {
	None = 0,
	Damage = 1 << 0,
	AccumulatedResults = 1 << 1,
	KnockbackApplied = 1 << 2,
	ActionChain = Damage | AccumulatedResults | KnockbackApplied,
};

/// Whether hook scratch chain state is merged back into the parent context after execution.
enum class MergePolicy : uint8_t {
	None = 0,
	ToParent = 1,
};

/// Declarative payload for a passive hook execution pass.
struct Event {
	UnitState *source = nullptr;
	UnitState *target = nullptr;
	UnitState *target_ally = nullptr;

	/// When false, action_kind is set to passive.
	bool preserve_action_kind = false;
	StringName action_kind;

	bool set_damage = false;
	double damage = 0.0;

	bool set_heal = false;
	double heal_amount = 0.0;
	double heal_gained = 0.0;

	bool set_takedown = false;
	int64_t takedown_target_id = 0;
	double takedown_damage_dealt = 0.0;
	bool is_takedown_kill = false;

	bool set_knockback_hook_depth = false;
	int32_t knockback_hook_depth = 0;

	ChainInherit inherit = ChainInherit::None;
	MergePolicy merge = MergePolicy::None;
};

void apply_isolation_defaults(EffectContext &ctx, const Event &event);
void recompute_distance(EffectContext &ctx);
EffectContext build_scratch_context(const Event &event, const EffectContext *parent);
void merge_scratch_to_parent(const EffectContext &scratch, double scratch_damage_before, EffectContext &parent);
void run_bucket(
		SimWorld &world,
		SimHostCallbacks &host,
		const std::vector<EffectRecord> &effects,
		const Event &event,
		EffectContext *parent);

} // namespace passive_hooks
} // namespace combat
} // namespace sim

#endif // SIM_PASSIVE_HOOKS_HPP
