#ifndef SIM_PERIODIC_INTERNAL_HPP
#define SIM_PERIODIC_INTERNAL_HPP

#include "sim_periodic.hpp"
#include "sim_aoe.hpp"
#include "sim_status.hpp"
#include "sim_world.hpp"

namespace sim {
namespace periodic {
namespace internal {

const godot::StringName &sn_player();
const godot::StringName &sn_enemy();
const godot::StringName &sn_taunt();

UnitState *resolve_shape_target(SimWorld &world, UnitState *target, const EffectRecord &effect);
EffectRecord make_circle_self_aoe(double radius);

void sync_targeting_if_healed(SimHostCallbacks &host, UnitState &target, double gained);
void apply_hot_tick_heal(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double heal_per_tick,
		const godot::StringName &action_kind,
		bool allow_overheal);

template<typename Fn>
void for_each_unit_in_aoe_shape(
		SimWorld &world,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		const godot::StringName &team,
		int64_t exclude_instance_id,
		Fn &&fn) {
	const std::vector<int64_t> &indices = alive_indices_for_team(world, team);
	UnitState *shape_target = resolve_shape_target(world, target, effect);

	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &indices;
	shape_iter.spatial_team = team;
	shape_iter.exclude_instance_id = exclude_instance_id;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;

	for_each_unit_in_shape(
			world,
			shape_iter,
			std::forward<Fn>(fn),
			[&world](const UnitState &src, const AoShapeParams &params, const UnitState *target_override) {
				return status::resolve_aoe_direction(world, src, params, target_override);
			});
}

template<typename Fn>
void for_each_enemy_in_aoe_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, int64_t exclude_instance_id, Fn &&fn) {
	const godot::StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	for_each_unit_in_aoe_shape(world, source, target, effect, enemy_team, exclude_instance_id, std::forward<Fn>(fn));
}

template<typename Fn>
void for_each_ally_in_aoe_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, int64_t exclude_instance_id, Fn &&fn) {
	const godot::StringName ally_team = source.team == sn_player() ? sn_player() : sn_enemy();
	for_each_unit_in_aoe_shape(world, source, target, effect, ally_team, exclude_instance_id, std::forward<Fn>(fn));
}

} // namespace internal
} // namespace periodic
} // namespace sim

#endif
