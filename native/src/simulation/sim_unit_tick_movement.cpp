#include "sim_unit_tick.hpp"
#include "sim_unit_tick_internal.hpp"

#include "sim_constants.hpp"
#include "sim_movement.hpp"
#include "sim_spatial.hpp"
#include "sim_stats.hpp"
#include "sim_targeting.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim {
namespace unit_tick {

using internal::SimProfileAccScope;

void separation(SimWorld &world, UnitState &unit, UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;
	const double tick_rate = world.tick_rate;

	SimProfileAccScope _uu_sep(profile_sim, profile.uu_separation);
	if (unit.stun_remaining <= 0.0 && unit.root_remaining <= 0.0) {
		const double move_speed = get_effective_move_speed(unit) * movement_speed_multiplier(unit);
		if (move_speed > 0.0) {
			const double attack_range = get_effective_attack_range(unit);
			const double radius = attack_range > SEPARATION_RANGE_THRESHOLD ? SEPARATION_RADIUS_RANGED : SEPARATION_RADIUS_MELEE;
			const double r2 = radius * radius;
			const double threshold_r2 = 4.0 * r2;
			const double ux = unit.pos_x;
			const double uy = unit.pos_y;
			double sep_x = 0.0;
			double sep_y = 0.0;
			const std::vector<int64_t> &ally_indices = alive_indices_for_team(world, unit.team);
			auto accumulate_separation_from_ally = [&](int64_t idx) {
				const UnitState &ally = world.units[idx];
				if (!ally.alive || ally.instance_id == unit.instance_id) {
					return;
				}
				const double ax = ally.pos_x;
				const double ay = ally.pos_y;
				const double dx = ux - ax;
				const double dy = uy - ay;
				const double d2 = dx * dx + dy * dy;
				if (d2 <= EPSILON || d2 >= threshold_r2) {
					return;
				}
				if (d2 >= r2) {
					return;
				}
				const double d = Math::sqrt(d2);
				const double force = (radius - d) / radius;
				sep_x += (dx / d) * force;
				sep_y += (dy / d) * force;
			};
			if (int64_t(ally_indices.size()) >= SPATIAL_SEPARATION_TEAM_THRESHOLD) {
				fill_buckets_for_indices(world, ally_indices);
				stamp_separation_candidates(world, ux, uy, radius, unit.team, unit.instance_id);
				for (int64_t idx : ally_indices) {
					if (!stamp_has(world, idx)) {
						continue;
					}
					accumulate_separation_from_ally(idx);
				}
			} else {
				for (int64_t idx : ally_indices) {
					accumulate_separation_from_ally(idx);
				}
			}
			if (!Math::is_zero_approx(sep_x) || !Math::is_zero_approx(sep_y)) {
				const double nudge_speed = move_speed * NUDGE_SPEED_MODIFIER * tick_rate;
				const double nx = sep_x * nudge_speed;
				const double ny = sep_y * nudge_speed;
				unit.pos_x = Math::clamp(ux + nx, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
				unit.pos_y = Math::clamp(uy + ny, WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
			}
		}
	}
}

bool movement(
		SimWorld &world,
		UnitState &unit,
		UnitState &target,
		const UnitStrategy &strategy,
		SimHostCallbacks &host,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;

	bool should_return = false;
	SimProfileAccScope _uu_move(profile_sim, profile.uu_movement);
	if (unit.root_remaining > 0.0) {
		should_return = true;
	} else {
		if (strategy.prefers_kiting && (unit.attack_cooldown > 0.0 || unit.combat.attack_speed == 0.0) && unit.taunt_remaining <= 0.0) {
			SimProfileAccScope _um_kit(profile_sim, profile.um_kiting);
			movement::KiteProfileCounters kite_profile{};
			if (profile_sim) {
				kite_profile.active = true;
				kite_profile.kiting_spatial = profile.um_kiting_spatial;
				kite_profile.kiting_brute = profile.um_kiting_brute;
			}
			if (movement::kite_from_enemies(world, host, unit, kite_profile.active ? &kite_profile : nullptr)) {
				should_return = true;
			}
		}
		if (!should_return) {
			SimProfileAccScope _um_tow(profile_sim, profile.um_toward);
			const double distance = distance_between(unit, target);
			const double actual_attack_range = targeting::attack_range(unit);

			if (distance > actual_attack_range) {
				movement::move_toward_target_with_range(world, unit, target, actual_attack_range);
			}
		}
	}
	return should_return;
}

} // namespace unit_tick
} // namespace sim
