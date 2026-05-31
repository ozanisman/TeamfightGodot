#include "sim_combat.hpp"
#include "sim_combat_internal.hpp"

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
		return;
	}
	EffectContext context = build_context(*source, target, nullptr, 0.0, projectile.action_kind);
	if (host.execute_effect != nullptr) {
		host.execute_effect(host, projectile.impact_effect, context);
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
	for (const ProjectileState &data : buffers.active_projectiles) {
		UnitState *target = unit_by_id(world, data.target_id);
		if (target == nullptr || !target->alive) {
			continue;
		}
		if (data.motion != sn_homing() || data.collision != sn_target_only() || data.on_target_lost != sn_drop()) {
			continue;
		}
		const double dist = distance_between_coords(data.pos_x, data.pos_y, target->pos_x, target->pos_y);
		const double move_dist = data.speed * world.tick_rate;
		if (dist <= move_dist + data.radius) {
			const ProjectileState hit = data;
			resolve_projectile(world, host, hooks, hit);
			if (match.sudden_death_ticks > 0 && match.player_kills != match.enemy_kills) {
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
