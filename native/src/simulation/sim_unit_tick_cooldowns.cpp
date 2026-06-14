#include "sim_unit_tick.hpp"
#include "sim_unit_tick_internal.hpp"

#include "sim_constants.hpp"
#include "sim_stats_modifiers.hpp"

#include "../stat_definitions.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim {
namespace unit_tick {

using internal::SimProfileAccScope;

void cooldowns_and_cc(
		SimWorld &world,
		UnitState &unit,
		const UnitTickHostHooks &tick_hooks,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;
	const double tick_rate = world.tick_rate;
	UnitStateCold &cold = uc(world, unit);

	SimProfileAccScope _uu_cc(profile_sim, profile.uu_cooldowns_cc);

	if (unit.attack_cooldown > 0.0) {
		SimProfileAccScope _ucc_acd(profile_sim, profile.ucc_attack_cd);
		if (!cold.is_channeling && !(unit.casting_remaining >= 0.0 && unit.has_casting_effect)) {
			unit.attack_cooldown -= tick_rate;
			if (unit.attack_cooldown < 0.0) {
				unit.attack_cooldown = 0.0;
			}
		}
	}
	if (unit.ability_cooldown > 0.0) {
		SimProfileAccScope _ucc_abcd(profile_sim, profile.ucc_ability_cd);
		unit.ability_cooldown -= tick_rate;
		if (unit.ability_cooldown < 0.0) {
			unit.ability_cooldown = 0.0;
		}
	}
	if (unit.retarget_timer > 0.0) {
		SimProfileAccScope _ucc_ret(profile_sim, profile.ucc_retarget);
		unit.retarget_timer -= tick_rate;
		if (unit.retarget_timer < 0.0) {
			unit.retarget_timer = 0.0;
		}
	}
	if (unit.target_switch_lock_timer > 0.0) {
		SimProfileAccScope _ucc_tsw(profile_sim, profile.ucc_target_switch);
		unit.target_switch_lock_timer -= tick_rate;
		if (unit.target_switch_lock_timer < 0.0) {
			unit.target_switch_lock_timer = 0.0;
		}
	}
	if (unit.stun_remaining > 0.0) {
		SimProfileAccScope _ucc_stun(profile_sim, profile.ucc_stun);
		unit.hard_cc_seconds += Math::min(unit.stun_remaining, tick_rate);
		unit.stun_remaining -= tick_rate;
		if (unit.stun_remaining < 0.0) {
			unit.stun_remaining = 0.0;
		}
	}
	if (!cold.slow_buffs.empty()) {
		SimProfileAccScope _ucc_slow(profile_sim, profile.ucc_slow);
		auto &slow_buffs = cold.slow_buffs;
		size_t index = 0;
		while (index < slow_buffs.size()) {
			slow_buffs[index].remaining_duration = Math::max(0.0, slow_buffs[index].remaining_duration - tick_rate);
			if (slow_buffs[index].remaining_duration <= 0.0) {
				slow_buffs[index] = slow_buffs.back();
				slow_buffs.pop_back();
			} else {
				++index;
			}
		}
	}
	if (unit.root_remaining > 0.0) {
		SimProfileAccScope _ucc_root(profile_sim, profile.ucc_root);
		unit.root_remaining -= tick_rate;
		if (unit.root_remaining < 0.0) {
			unit.root_remaining = 0.0;
		}
	}
	if (unit.silence_ability_remaining > 0.0 || unit.silence_ultimate_remaining > 0.0) {
		SimProfileAccScope _ucc_sil(profile_sim, profile.ucc_silence);
		unit.silence_ability_remaining -= tick_rate;
		if (unit.silence_ability_remaining < 0.0) {
			unit.silence_ability_remaining = 0.0;
		}
	}
	unit.silence_ultimate_remaining -= tick_rate;
	if (unit.silence_ultimate_remaining < 0.0) {
		unit.silence_ultimate_remaining = 0.0;
	}
	unit.silence_remaining = Math::max(unit.silence_ability_remaining, unit.silence_ultimate_remaining);
	unit.silence_blocks_abilities = unit.silence_ability_remaining > 0.0;
	unit.silence_blocks_ultimates = unit.silence_ultimate_remaining > 0.0;
	if (unit.disarm_remaining > 0.0) {
		SimProfileAccScope _ucc_dis(profile_sim, profile.ucc_disarm);
		unit.disarm_remaining -= tick_rate;
		if (unit.disarm_remaining < 0.0) {
			unit.disarm_remaining = 0.0;
		}
	}
	if (unit.stealth_remaining > 0.0) {
		SimProfileAccScope _ucc_ste(profile_sim, profile.ucc_stealth);
		unit.stealth_remaining -= tick_rate;
		if (unit.stealth_remaining <= 0.0) {
			unit.stealth_remaining = 0.0;
			unit.stealth_break_on_attack = false;
			unit.stealth_break_on_ability = false;
			unit.stealth_break_on_damage_taken = false;
		}
	}
	if (unit.shield > 0.0) {
		SimProfileAccScope _ucc_shi(profile_sim, profile.ucc_shield);
		unit.shield *= (1.0 - SHIELD_DECAY_RATE * tick_rate);
		if (unit.shield < 0.01) {
			unit.shield = 0.0;
		}
	}
	if (!cold.reflect_buffs.empty()) {
		SimProfileAccScope _ucc_ref(profile_sim, profile.ucc_reflect);
		auto &reflect_buffs = cold.reflect_buffs;
		size_t index = 0;
		while (index < reflect_buffs.size()) {
			reflect_buffs[index].remaining_duration = Math::max(0.0, reflect_buffs[index].remaining_duration - tick_rate);
			if (reflect_buffs[index].remaining_duration <= 0.0) {
				reflect_buffs[index] = reflect_buffs.back();
				reflect_buffs.pop_back();
			} else {
				++index;
			}
		}
	}
	if (unit.taunt_remaining > 0.0) {
		SimProfileAccScope _ucc_tau(profile_sim, profile.ucc_taunt);
		unit.taunt_remaining = Math::max(0.0, unit.taunt_remaining - tick_rate);
		if (unit.taunt_remaining <= 0.0) {
			unit.taunt_remaining = 0.0;
			unit.taunt_target_id = 0;
		}
	}
	if (unit.forced_target_remaining > 0.0) {
		SimProfileAccScope _ucc_ft(profile_sim, profile.ucc_forced_target);
		unit.forced_target_remaining = Math::max(0.0, unit.forced_target_remaining - tick_rate);
	}
	if (unit.last_kite_timer > 0.0) {
		unit.last_kite_timer = Math::max(0.0, unit.last_kite_timer - tick_rate);
	}

	bool has_temporary_stat_modifiers = !unit.stat_modifiers.is_empty() || !unit.stat_stacks.is_empty();

	if (!unit.stat_stacks.is_empty()) {
		stats_modifiers::update_stacks(unit, tick_rate, world.time);
	}

	if (has_temporary_stat_modifiers) {
		stats_modifiers::update_stat_modifier_durations(unit, tick_rate);
		stats_modifiers::clear_expired_stat_modifiers(unit);
	}
	if (unit.forced_target_remaining <= 0.0) {
		unit.forced_target_remaining = 0.0;
		unit.forced_target_id = 0;
		cold.forced_target_kind = StringName();
	}
}

} // namespace unit_tick
} // namespace sim
