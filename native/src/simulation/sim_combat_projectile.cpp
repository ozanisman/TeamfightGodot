#include "sim_combat.hpp"
#include "sim_combat_internal.hpp"

#include "sim_constants.hpp"
#include "sim_damage.hpp"
#include "sim_stats.hpp"
#include "sim_status.hpp"

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
	const double damage = projectile.damage;
	const StringName damage_type = projectile.damage_type;
	const StringName action_kind = projectile.action_kind;
	EffectContext context = build_context(*source, target, nullptr, damage, action_kind);
	const double dealt = damage::apply_damage(world, host, *source, *target, damage, damage_type, action_kind, context);
	run_post_attack_effects(world, host, *source, *target, dealt, context);
	if (projectile.action_kind == sn_auto()) {
		const double life_steal = get_effective_life_steal(*source);
		if (life_steal > 0.0) {
			const double old_hp = source->hp;
			const double heal_amount = dealt * life_steal;
			heal_with_hooks(world, host, *source, *source, heal_amount, sn_auto(), false);
			const double heal_gained = source->hp - old_hp;
			run_post_heal_effects(world, host, *source, *source, heal_amount, heal_gained, context);
		}
	}
	if (projectile.stun_duration > 0.0 && target->alive) {
		status::apply_stun(world, *source, *target, projectile.stun_duration);
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
