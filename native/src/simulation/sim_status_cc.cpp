#include "sim_status.hpp"
#include "sim_status_internal.hpp"

#include "sim_stats.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim {
namespace status {

using namespace internal;

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
		const UnitStateCold &cold = uc(world, target);
		return !cold.reflect_buffs.empty() || !cold.passive_reflect_entries.empty();
	}
	return false;
}

void apply_stun(SimWorld &world, UnitState &source, UnitState &target, double duration) {
	if (duration <= 0.0) {
		return;
	}
	const double tenacity = get_effective_tenacity(target);
	const double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	target.stun_remaining = Math::max(target.stun_remaining, effective_duration);
	uc(world, source).stuns += 1;
}

void apply_slow(SimWorld &world, UnitState &source, UnitState &target, double slow_percentage, double duration) {
	(void)source;
	(void)world;
	if (duration <= 0.0 || slow_percentage <= 0.0) {
		return;
	}
	const double tenacity = get_effective_tenacity(target);
	const double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	const double pct = Math::clamp(slow_percentage, 0.0, 1.0);
	const double mult = Math::clamp(1.0 - pct, SLOW_MOVEMENT_MULTIPLIER_MIN, 1.0);
	target.slow_remaining = Math::max(target.slow_remaining, effective_duration);
	target.slow_move_mult = Math::min(target.slow_move_mult, mult);
}

void apply_root(SimWorld &world, UnitState &source, UnitState &target, double duration) {
	(void)source;
	(void)world;
	if (duration <= 0.0) {
		return;
	}
	const double tenacity = get_effective_tenacity(target);
	const double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	target.root_remaining = Math::max(target.root_remaining, effective_duration);
}

void apply_silence(SimWorld &world, UnitState &source, UnitState &target, double duration, bool block_abilities, bool block_ultimate) {
	(void)source;
	(void)world;
	if (duration <= 0.0) {
		return;
	}
	const double tenacity = get_effective_tenacity(target);
	const double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	if (block_abilities) {
		target.silence_ability_remaining = Math::max(target.silence_ability_remaining, effective_duration);
	}
	if (block_ultimate) {
		target.silence_ultimate_remaining = Math::max(target.silence_ultimate_remaining, effective_duration);
	}
	target.silence_remaining = Math::max(target.silence_ability_remaining, target.silence_ultimate_remaining);
	target.silence_blocks_abilities = target.silence_ability_remaining > 0.0;
	target.silence_blocks_ultimates = target.silence_ultimate_remaining > 0.0;
}

void apply_disarm(SimWorld &world, UnitState &source, UnitState &target, double duration) {
	(void)source;
	(void)world;
	if (duration <= 0.0) {
		return;
	}
	const double tenacity = get_effective_tenacity(target);
	const double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	target.disarm_remaining = Math::max(target.disarm_remaining, effective_duration);
}

void apply_stealth(SimWorld &world, UnitState &source, UnitState &target, double duration, bool break_on_attack, bool break_on_ability, bool break_on_damage_taken) {
	(void)source;
	(void)world;
	if (duration <= 0.0) {
		return;
	}
	target.stealth_remaining = duration;
	target.stealth_break_on_attack = break_on_attack;
	target.stealth_break_on_ability = break_on_ability;
	target.stealth_break_on_damage_taken = break_on_damage_taken;
}

} // namespace status
} // namespace sim
