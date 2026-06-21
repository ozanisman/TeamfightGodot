#include "sim_unit_tick.hpp"
#include "sim_unit_tick_internal.hpp"

#include "sim_constants.hpp"
#include "sim_targeting.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim {
namespace unit_tick {

using internal::SimProfileAccScope;

void update_unit(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks &host,
		const channel::ChannelHostHooks &channel_hooks,
		const combat::CombatHostHooks &combat_hooks,
		const UnitTickHostHooks &tick_hooks,
		const UnitTickMatchState &match,
		UnitTickProfileCounters &profile,
		std::vector<ProjectileState> &projectiles) {
	const bool profile_sim = profile.profile_sim;

	if (!unit.alive) {
		SimProfileAccScope _uu_dead(profile_sim, profile.uu_dead_respawn);
		unit.respawn_timer = Math::max(0.0, unit.respawn_timer - world.tick_rate);
		if (unit.respawn_timer <= 0.0 && tick_hooks.respawn_unit != nullptr) {
			// Minions do not respawn
			static const StringName sn_minion("minion");
			if (uc(world, unit).role_id != sn_minion) {
				tick_hooks.respawn_unit(tick_hooks.user_data, unit);
			}
		}
		return;
	}

	if (tick_hooks.strategy_for_unit == nullptr) {
		return;
	}
	const UnitStrategy &strategy = tick_hooks.strategy_for_unit(tick_hooks.user_data, unit);

	cooldowns_and_cc(world, unit, tick_hooks, profile);
	if (match.sudden_death_ticks > 0) {
		apply_sudden_death_overtime(world, match);
	}
	separation(world, unit, profile);
	threat_and_assist(world, unit, strategy, host, tick_hooks, profile);

	if (regen_and_periodic(world, unit, host, channel_hooks, tick_hooks, profile)) {
		return;
	}
	if (casting(world, unit, host, combat_hooks, profile)) {
		return;
	}

	const TargetingResult targets = targeting(world, unit, host, tick_hooks, profile);
	if (targets.stop || targets.target == nullptr) {
		return;
	}
	UnitState *move_target = targets.target;
	double standoff = -1.0;
	if (unit.is_support_role && targets.target_ally != nullptr) {
		const double attack_range = targeting::attack_range(unit);
		if (internal::support_outside_ally_standoff(world, unit)) {
			move_target = targets.target_ally;
			standoff = attack_range * SUPPORT_ALLY_STANDOFF_RATIO;
		} else if (internal::support_should_advance_on_enemy(unit, *targets.target)) {
			move_target = targets.target;
			standoff = attack_range;
		} else {
			move_target = targets.target_ally;
			standoff = attack_range * SUPPORT_ALLY_STANDOFF_RATIO;
		}
	}
	if (combat_actions(world, unit, *targets.target, host, combat_hooks, projectiles, profile)) {
		return;
	}
	if (movement(world, unit, *move_target, strategy, host, profile, standoff)) {
		return;
	}
}

} // namespace unit_tick
} // namespace sim
