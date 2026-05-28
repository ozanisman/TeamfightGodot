#include "sim_status.hpp"
#include "sim_status_internal.hpp"

#include "sim_constants.hpp"
#include "sim_targeting.hpp"
#include "sim_viewer.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace sim {
namespace status {

using namespace internal;

Vector2 resolve_aoe_direction(const SimWorld &world, const UnitState &source, const AoShapeParams &params, const UnitState *target_override) {
	Vector2 forward(1.0, 0.0);

	const UnitState *target = target_override;
	if (target == nullptr && params.target_id != 0) {
		target = targeting::unit_by_id(world, params.target_id);
	}

	if (target != nullptr) {
		const Vector2 diff(target->pos_x - source.pos_x, target->pos_y - source.pos_y);
		if (diff.length_squared() > EPSILON * EPSILON) {
			forward = diff.normalized();
		}
	} else {
		const int64_t source_target_id = source.target_id;
		if (source_target_id != 0) {
			const UnitState *source_target = targeting::unit_by_id(world, source_target_id);
			if (source_target != nullptr) {
				const Vector2 diff(source_target->pos_x - source.pos_x, source_target->pos_y - source.pos_y);
				if (diff.length_squared() > EPSILON * EPSILON) {
					forward = diff.normalized();
				}
			}
		}

		if (forward == Vector2(1.0, 0.0)) {
			const StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
			const std::vector<int64_t> &enemy_indices = sim::alive_indices_for_team(world, enemy_team);
			if (!enemy_indices.empty()) {
				double cx = 0.0;
				double cy = 0.0;
				for (const int64_t idx : enemy_indices) {
					const UnitState &u = world.units[static_cast<size_t>(idx)];
					cx += u.pos_x;
					cy += u.pos_y;
				}
				cx /= double(enemy_indices.size());
				cy /= double(enemy_indices.size());
				const Vector2 diff(cx - source.pos_x, cy - source.pos_y);
				if (diff.length_squared() > EPSILON * EPSILON) {
					forward = diff.normalized();
				}
			}
		}
	}

	if (!Math::is_zero_approx(params.rotation_radians)) {
		forward = forward.rotated(params.rotation_radians);
	}

	return forward;
}

void apply_aoe_slow(SimWorld &world, UnitState &source, double radius, double slow_percentage, double duration) {
	apply_aoe_slow_shape(world, source, nullptr, make_circle_self_aoe(radius), slow_percentage, duration);
}

void apply_aoe_slow_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double slow_percentage, double duration, const SimHostCallbacks *host) {
	record_aoe_shape_fx(host != nullptr ? host->viewer_hooks : nullptr, world, source, target, effect, StringName("aoe_slow"));
	for_each_enemy_in_aoe_shape(world, source, target, effect, [&](UnitState &unit) {
		apply_slow(world, source, unit, slow_percentage, duration);
	});
}

void apply_aoe_root(SimWorld &world, UnitState &source, double radius, double duration) {
	apply_aoe_root_shape(world, source, nullptr, make_circle_self_aoe(radius), duration);
}

void apply_aoe_root_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration, const SimHostCallbacks *host) {
	record_aoe_shape_fx(host != nullptr ? host->viewer_hooks : nullptr, world, source, target, effect, StringName("aoe_root"));
	for_each_enemy_in_aoe_shape(world, source, target, effect, [&](UnitState &unit) {
		apply_root(world, source, unit, duration);
	});
}

void apply_aoe_silence(SimWorld &world, UnitState &source, double radius, double duration, bool block_abilities, bool block_ultimate) {
	apply_aoe_silence_shape(world, source, nullptr, make_circle_self_aoe(radius), duration, block_abilities, block_ultimate);
}

void apply_aoe_silence_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration, bool block_abilities, bool block_ultimate, const SimHostCallbacks *host) {
	record_aoe_shape_fx(host != nullptr ? host->viewer_hooks : nullptr, world, source, target, effect, StringName("aoe_silence"));
	for_each_enemy_in_aoe_shape(world, source, target, effect, [&](UnitState &unit) {
		apply_silence(world, source, unit, duration, block_abilities, block_ultimate);
	});
}

void apply_aoe_disarm(SimWorld &world, UnitState &source, double radius, double duration) {
	apply_aoe_disarm_shape(world, source, nullptr, make_circle_self_aoe(radius), duration);
}

void apply_aoe_disarm_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration, const SimHostCallbacks *host) {
	record_aoe_shape_fx(host != nullptr ? host->viewer_hooks : nullptr, world, source, target, effect, StringName("aoe_disarm"));
	for_each_enemy_in_aoe_shape(world, source, target, effect, [&](UnitState &unit) {
		apply_disarm(world, source, unit, duration);
	});
}

void apply_aoe_stun(SimWorld &world, UnitState &source, double radius, double duration) {
	apply_aoe_stun_shape(world, source, nullptr, make_circle_self_aoe(radius), duration);
}

void apply_aoe_stun_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration, const SimHostCallbacks *host) {
	record_aoe_shape_fx(host != nullptr ? host->viewer_hooks : nullptr, world, source, target, effect, StringName("aoe_stun"));
	for_each_enemy_in_aoe_shape(world, source, target, effect, [&](UnitState &unit) {
		apply_stun(world, source, unit, duration);
	});
}

} // namespace status
} // namespace sim
