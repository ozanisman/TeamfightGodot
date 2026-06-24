#include "sim_combat.hpp"
#include "sim_combat_internal.hpp"
#include "sim_effects_exec.hpp"
#include "sim_effects_exec_internal.hpp"

#include "sim_constants.hpp"
#include "sim_stats.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim {
namespace combat {

using namespace internal;

void resolve_projectile(SimWorld &world, SimHostCallbacks &host, const CombatHostHooks &hooks, const ProjectileState &projectile) {
	(void)hooks;
	UnitState *source = unit_by_id(world, projectile.source_id);
	UnitState *target = unit_by_id(world, projectile.target_id);
	if (source == nullptr || target == nullptr || !target->alive) {
		sim::effects::execution::abandon_deferred_projectile(world, host, projectile);
		return;
	}
	EffectContext context = build_context(*source, target, nullptr, 0.0, projectile.action_kind);
	if (uc(world, *source).is_channeling) {
		context.channel_accumulated_damage = uc(world, *source).channel_accumulated_damage;
	}
	const double damage_before = context.damage;
	if (host.execute_effect != nullptr) {
		const Dictionary result = host.execute_effect(host, projectile.impact_effect, context);
		const double dealt = sim::effects::execution::internal::extract_dealt_damage_delta(damage_before, context, result);
		if (projectile.damage_accumulator_source_id != 0) {
			UnitState *accumulator = unit_by_id(world, projectile.damage_accumulator_source_id);
			if (accumulator != nullptr && accumulator->alive) {
				UnitStateCold &accumulator_cold = uc(world, *accumulator);
				if (accumulator_cold.is_channeling) {
					accumulator_cold.channel_accumulated_damage += dealt;
				}
				if (accumulator_cold.deferred_effect_outstanding_projectiles > 0) {
					accumulator_cold.deferred_effect_projectile_dealt_damage += dealt;
					accumulator_cold.deferred_effect_outstanding_projectiles -= 1;
					if (accumulator_cold.deferred_effect_outstanding_projectiles == 0) {
						sim::effects::execution::try_complete_deferred_projectile_chains(world, host, *accumulator);
					}
				}
			}
		}
	}
	if (context.knockback_applied && context.knockback_hook_depth == 0) {
		// Target is guaranteed non-null/alive by the early exit above.
		run_on_knockback_action_effects(world, host, *source, target, context);
	}
}

void update_projectiles(
		SimWorld &world,
		SimHostCallbacks &host,
		const CombatHostHooks &hooks,
		ProjectileBuffers &buffers,
		const ProjectileMatchState &match) {
	using std::swap;
	buffers.active_projectiles.clear();
	swap(buffers.active_projectiles, buffers.projectiles);
	buffers.scratch_projectiles.clear();
	for (size_t i = 0; i < buffers.active_projectiles.size(); ++i) {
		const ProjectileState &data = buffers.active_projectiles[i];
		UnitState *target = unit_by_id(world, data.target_id);
		if (target == nullptr || !target->alive) {
			sim::effects::execution::abandon_deferred_projectile(world, host, data);
			continue;
		}
		if (data.motion != sn_homing() || data.collision != sn_target_only() || data.on_target_lost != sn_drop()) {
			sim::effects::execution::abandon_deferred_projectile(world, host, data);
			continue;
		}
		const double dist = distance_between_coords(data.pos_x, data.pos_y, target->pos_x, target->pos_y);
		const double move_dist = data.speed * world.tick_rate;
		if (dist <= move_dist + data.radius) {
			const ProjectileState hit = data;
			resolve_projectile(world, host, hooks, hit);
			if (match.sudden_death_ticks > 0 && match.player_kills != match.enemy_kills) {
				for (size_t j = i + 1; j < buffers.active_projectiles.size(); ++j) {
					sim::effects::execution::abandon_deferred_projectile(world, host, buffers.active_projectiles[j]);
				}
				buffers.scratch_projectiles.clear();
				break;
			}
			continue;
		}
		if (dist > EPSILON) {
			ProjectileState next = data;
			const double inv = 1.0 / dist;
			next.pos_x = data.pos_x + (target->pos_x - data.pos_x) * inv * move_dist;
			next.pos_y = data.pos_y + (target->pos_y - data.pos_y) * inv * move_dist;
			buffers.scratch_projectiles.push_back(next);
		} else {
			buffers.scratch_projectiles.push_back(data);
		}
	}
	if (!buffers.projectiles.empty()) {
		buffers.scratch_projectiles.insert(
				buffers.scratch_projectiles.end(),
				buffers.projectiles.begin(),
				buffers.projectiles.end());
	}
	swap(buffers.projectiles, buffers.scratch_projectiles);
	buffers.scratch_projectiles.clear();
}

} // namespace combat
} // namespace sim
