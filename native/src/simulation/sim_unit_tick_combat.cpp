#include "sim_unit_tick.hpp"
#include "sim_unit_tick_internal.hpp"

#include "sim_combat.hpp"
#include "sim_targeting.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim {
namespace unit_tick {

using internal::SimProfileAccScope;
using internal::sync_targeting_frame_unit;

void threat_and_assist(
		SimWorld &world,
		UnitState &unit,
		const UnitStrategy &strategy,
		SimHostCallbacks &host,
		const UnitTickHostHooks &tick_hooks,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;

	SimProfileAccScope _uu_ta(profile_sim, profile.uu_threat_and_assist);
	const double old_threat = unit.perceived_threat;
	unit.perceived_threat = Math::max(0.0, unit.perceived_threat - strategy.threat_decay_rate * world.tick_rate);
	if (Math::abs(unit.perceived_threat - old_threat) >= 0.001) {
		sync_targeting_frame_unit(host, unit);
	}

	if (tick_hooks.prune_assist_window != nullptr) {
		tick_hooks.prune_assist_window(tick_hooks.user_data, unit);
	}
}

TargetingResult targeting(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks &host,
		const UnitTickHostHooks &tick_hooks,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;
	TargetingResult result;

	SimProfileAccScope _uu_tgt(profile_sim, profile.uu_targeting);
	if (tick_hooks.select_ally_target != nullptr) {
		result.target_ally = tick_hooks.select_ally_target(tick_hooks.user_data, unit);
	}
	unit.current_ally_target_id = result.target_ally == nullptr ? 0 : result.target_ally->instance_id;
	if (tick_hooks.select_enemy_target != nullptr) {
		result.target = tick_hooks.select_enemy_target(tick_hooks.user_data, unit, profile_sim);
	}
	if (result.target == nullptr) {
		unit.target_id = 0;
		unit.target_index = -1;
		sync_targeting_frame_unit(host, unit);
		result.stop = true;
	}
	return result;
}

bool combat_actions(
		SimWorld &world,
		UnitState &unit,
		UnitState &target,
		SimHostCallbacks &host,
		const combat::CombatHostHooks &combat_hooks,
		std::vector<ProjectileState> &projectiles,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;

	SimProfileAccScope _uu_combat(profile_sim, profile.uu_combat);
	{
		SimProfileAccScope _uc_dc(profile_sim, profile.uc_distance_calc);
		const double effective_range = targeting::effective_attack_range(unit);
		const double dx = target.pos_x - unit.pos_x;
		const double dy = target.pos_y - unit.pos_y;
		const double dist_sq = dx * dx + dy * dy;
		const double range_sq = effective_range * effective_range;
		const bool in_contact = (dist_sq <= range_sq);
		const double distance = Math::sqrt(dist_sq);

		const bool can_cast_ultimate = in_contact || !unit.ultimate_requires_target_in_range;
		const bool can_cast_ability = in_contact || !unit.ability_requires_target_in_range;

		if (can_cast_ultimate) {
			SimProfileAccScope _uc_ab(profile_sim, profile.uc_ability);
			if (combat::try_cast_ultimate(world, host, combat_hooks, unit, target, distance)) {
				return true;
			}
		}
		if (can_cast_ability) {
			SimProfileAccScope _uc_ab2(profile_sim, profile.uc_ability);
			if (combat::try_cast_ability(world, host, combat_hooks, unit, target, distance)) {
				return true;
			}
		}
		if (in_contact) {
			if (unit.attack_cooldown <= 0.0) {
				if (!uc(world, unit).is_channeling) {
					if (unit.combat.attack_speed > 0.0) {
						SimProfileAccScope _uc_aa(profile_sim, profile.uc_auto_attack);
						combat::perform_auto_attack(world, host, combat_hooks, unit, target, distance, projectiles);
						return true;
					}
				}
			}
		}
	}
	return false;
}

} // namespace unit_tick
} // namespace sim
