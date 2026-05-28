#ifndef SIM_VIEWER_HPP
#define SIM_VIEWER_HPP

#include "sim_aoe.hpp"
#include "sim_targeting.hpp"
#include "simulation_types.hpp"

namespace sim {

struct ViewerHooks {
	void *user_data = nullptr;

	void (*record_damage_fx)(
			void *user_data,
			const UnitState &source,
			const UnitState &target,
			double total_damage,
			const StringName &action_kind,
			const StringName &damage_type) = nullptr;

	void (*record_heal_fx)(void *user_data, const UnitState &target, double amount) = nullptr;

	void (*record_shield_fx)(void *user_data, const UnitState &target, double amount) = nullptr;

	void (*record_hot_status_fx)(
			void *user_data,
			const UnitState &target,
			double duration,
			const StringName &effect_type) = nullptr;

	void (*record_aoe_shape_fx)(
			void *user_data,
			const UnitState &source,
			const UnitState *target,
			const AoShapeParams &params,
			const StringName &kind) = nullptr;

	void (*record_passive_aoe_fx)(
			void *user_data,
			const UnitState &unit,
			double radius,
			const StringName &passive_id) = nullptr;

	void (*sync_targeting_frame_unit)(void *user_data, const UnitState &unit) = nullptr;
};

inline const UnitState *resolve_shape_target(const SimWorld &world, const UnitState *target, const EffectRecord &effect) {
	if (target != nullptr) {
		return target;
	}
	if (effect.aoe_shape_params.target_id != 0) {
		return targeting::unit_by_id(world, effect.aoe_shape_params.target_id);
	}
	return nullptr;
}

inline void record_aoe_shape_fx(
		const ViewerHooks *viewer,
		const SimWorld &world,
		const UnitState &source,
		const UnitState *target,
		const EffectRecord &effect,
		const StringName &kind) {
	if (viewer == nullptr || viewer->record_aoe_shape_fx == nullptr) {
		return;
	}
	const UnitState *shape_target = resolve_shape_target(world, target, effect);
	viewer->record_aoe_shape_fx(viewer->user_data, source, shape_target, effect.aoe_shape_params, kind);
}

} // namespace sim

#endif // SIM_VIEWER_HPP
