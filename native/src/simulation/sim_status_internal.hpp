#ifndef SIM_STATUS_INTERNAL_HPP
#define SIM_STATUS_INTERNAL_HPP

#include "sim_status.hpp"
#include "sim_world.hpp"

#include <vector>

namespace sim {
namespace status {
namespace internal {

const godot::StringName &sn_player();
const godot::StringName &sn_enemy();
const godot::StringName &sn_auto();
const godot::StringName &sn_ability();
const godot::StringName &sn_ultimate();
const godot::StringName &sn_passive();
const godot::StringName &sn_slow();
const godot::StringName &sn_root();
const godot::StringName &sn_silence();
const godot::StringName &sn_disarm();
const godot::StringName &sn_stealth();
const godot::StringName &sn_stun();
const godot::StringName &sn_reflect();

void record_shielding_by_action_kind(UnitStateCold &source_cold, double amount, const godot::StringName &action_kind);
void record_healing_by_action_kind(UnitStateCold &source_cold, double gained, const godot::StringName &action_kind);
void record_benefactor(SimWorld &world, UnitState &source, UnitState &target);

EffectRecord make_circle_self_aoe(double radius);

UnitState *resolve_shape_target(SimWorld &world, UnitState *target, const EffectRecord &effect);

template<typename Fn>
void for_each_enemy_in_aoe_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, Fn &&fn) {
	const godot::StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	const std::vector<int64_t> &enemy_indices = sim::alive_indices_for_team(world, enemy_team);
	UnitState *shape_target = resolve_shape_target(world, target, effect);

	AoShapeIterationParams shape_iter;
	shape_iter.shape_params = effect.aoe_shape_params;
	shape_iter.indices = &enemy_indices;
	shape_iter.spatial_team = enemy_team;
	shape_iter.exclude_instance_id = 0;
	shape_iter.source = &source;
	shape_iter.target_override = shape_target;

	for_each_unit_in_shape(
			world,
			shape_iter,
			std::forward<Fn>(fn),
			[&world](const UnitState &src, const AoShapeParams &params, const UnitState *target_override) {
				return resolve_aoe_direction(world, src, params, target_override);
			});
}

} // namespace internal
} // namespace status
} // namespace sim

#endif
