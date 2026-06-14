#include "sim_periodic.hpp"

#include "sim_periodic_internal.hpp"

#include "sim_viewer.hpp"
#include "sim_combat.hpp"
#include "sim_constants.hpp"
#include "sim_damage.hpp"
#include "sim_movement.hpp"
#include "sim_spatial.hpp"
#include "sim_stats.hpp"
#include "sim_status.hpp"
#include "sim_targeting.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <algorithm>

#define X(name, def, min_val, max_val) using sim::get_effective_##name;
STAT_LIST
#undef X

namespace sim {
namespace periodic {

using namespace internal;

void apply_aoe_dot(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		double radius,
		double attack_damage_ratio,
		double max_hp_ratio,
		double flat_amount,
		double duration,
		double tick_interval,
		const StringName &damage_type,
		const StringName &stacking_mode,
		int max_stacks,
		const StringName &effect_type,
		const String &reason,
		bool target_self,
		const StringName &action_kind,
		bool is_dynamic) {
	EffectRecord effect = make_circle_self_aoe(radius);
	effect.damage_type = damage_type;
	effect.stacking_mode = stacking_mode;
	effect.effect_type = effect_type;
	apply_aoe_dot_shape(
			world,
			host,
			source,
			nullptr,
			effect,
			attack_damage_ratio,
			max_hp_ratio,
			flat_amount,
			duration,
			tick_interval,
			damage_type,
			stacking_mode,
			max_stacks,
			effect_type,
			reason,
			target_self,
			action_kind,
			is_dynamic);
}

void apply_aoe_dot_shape(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double attack_damage_ratio,
		double max_hp_ratio,
		double flat_amount,
		double duration,
		double tick_interval,
		const StringName &damage_type,
		const StringName &stacking_mode,
		int max_stacks,
		const StringName &effect_type,
		const String &reason,
		bool target_self,
		const StringName &action_kind,
		bool is_dynamic) {
	record_aoe_shape_fx(host.viewer_hooks, world, source, target, effect, StringName("aoe_dot"));
	const int64_t exclude_instance_id = target_self ? 0 : source.instance_id;
	for_each_enemy_in_aoe_shape(world, source, target, effect, exclude_instance_id, [&](UnitState &unit) {
		apply_dot(
				world,
				host,
				source,
				unit,
				attack_damage_ratio,
				max_hp_ratio,
				flat_amount,
				duration,
				tick_interval,
				damage_type,
				stacking_mode,
				max_stacks,
				effect_type,
				reason,
				action_kind,
				is_dynamic);
	});
}

void apply_aoe_hot(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		double radius,
		double max_hp_ratio,
		double current_hp_ratio,
		double missing_hp_ratio,
		double flat_amount,
		double duration,
		double tick_interval,
		const StringName &stacking_mode,
		int max_stacks,
		bool allow_overheal,
		const StringName &effect_type,
		const String &reason,
		bool target_self,
		const StringName &action_kind,
		bool is_dynamic) {
	EffectRecord effect = make_circle_self_aoe(radius);
	effect.stacking_mode = stacking_mode;
	effect.effect_type = effect_type;
	apply_aoe_hot_shape(
			world,
			host,
			source,
			nullptr,
			effect,
			max_hp_ratio,
			current_hp_ratio,
			missing_hp_ratio,
			flat_amount,
			duration,
			tick_interval,
			stacking_mode,
			max_stacks,
			allow_overheal,
			effect_type,
			reason,
			target_self,
			action_kind,
			is_dynamic);
}

void apply_aoe_hot_shape(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double max_hp_ratio,
		double current_hp_ratio,
		double missing_hp_ratio,
		double flat_amount,
		double duration,
		double tick_interval,
		const StringName &stacking_mode,
		int max_stacks,
		bool allow_overheal,
		const StringName &effect_type,
		const String &reason,
		bool target_self,
		const StringName &action_kind,
		bool is_dynamic) {
	record_aoe_shape_fx(host.viewer_hooks, world, source, target, effect, StringName("aoe_hot"));
	const int64_t exclude_instance_id = target_self ? 0 : source.instance_id;
	for_each_ally_in_aoe_shape(world, source, target, effect, exclude_instance_id, [&](UnitState &unit) {
		apply_hot(
				world,
				host,
				source,
				unit,
				max_hp_ratio,
				current_hp_ratio,
				missing_hp_ratio,
				flat_amount,
				duration,
				tick_interval,
				stacking_mode,
				max_stacks,
				allow_overheal,
				effect_type,
				reason,
				action_kind,
				is_dynamic);
	});
}

void apply_aoe_taunt(SimWorld &world, UnitState &source, double radius, double duration) {
	apply_aoe_taunt_shape(world, source, nullptr, make_circle_self_aoe(radius), duration);
}

void apply_aoe_taunt_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration, const SimHostCallbacks *host) {
	record_aoe_shape_fx(host != nullptr ? host->viewer_hooks : nullptr, world, source, target, effect, StringName("aoe_taunt"));
	for_each_enemy_in_aoe_shape(world, source, target, effect, 0, [&](UnitState &unit) {
		const double tenacity = unit.stats_dirty ? get_effective_tenacity(unit) : unit.cached_tenacity;
		const double effective_duration = duration * (1.0 - tenacity);
		if (effective_duration > 0.0) {
			unit.taunt_target_id = source.instance_id;
			unit.taunt_remaining = Math::max(unit.taunt_remaining, effective_duration);
			unit.forced_target_id = source.instance_id;
			unit.forced_target_remaining = Math::max(unit.forced_target_remaining, effective_duration);
			uc(world, unit).forced_target_kind = sn_taunt();
		}
	});
}

double apply_aoe_damage(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &center_source,
		double damage,
		double radius,
		const StringName &damage_type,
		const StringName &reason,
		const StringName &action_kind) {
	(void)reason;
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Target;
	effect.aoe_shape_params.radius = radius;
	effect.damage_type = damage_type;
	return apply_aoe_damage_shape(world, host, source, &center_source, effect, damage, damage_type, action_kind);
}

double apply_aoe_damage_shape(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double damage,
		const StringName &damage_type,
		const StringName &action_kind) {
	record_aoe_shape_fx(host.viewer_hooks, world, source, target, effect, StringName("aoe_damage"));
	double total_damage = 0.0;
	for_each_enemy_in_aoe_shape(world, source, target, effect, 0, [&](UnitState &unit) {
		EffectContext context = combat::build_context(source, &unit, nullptr, damage, action_kind);
		total_damage += damage::apply_damage(world, host, source, unit, damage, damage_type, action_kind, context);
	});
	return total_damage;
}

double apply_aoe_damage_shape_per_target(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double damage_ratio,
		double flat_amount,
		double max_hp_ratio,
		double splash_ratio,
		const StringName &damage_type,
		const StringName &action_kind) {
	record_aoe_shape_fx(host.viewer_hooks, world, source, target, effect, StringName("aoe_damage"));
	double total_damage = 0.0;
	for_each_enemy_in_aoe_shape(world, source, target, effect, 0, [&](UnitState &unit) {
		double target_damage = (unit.stats_dirty ? get_effective_max_hp(unit) : unit.cached_max_hp) * max_hp_ratio;
		target_damage += (source.stats_dirty ? get_effective_attack_damage(source) : source.cached_attack_damage) * damage_ratio;
		target_damage += flat_amount;
		if (splash_ratio != 1.0) {
			target_damage *= splash_ratio;
		}
		EffectContext context = combat::build_context(source, &unit, nullptr, target_damage, action_kind);
		total_damage += damage::apply_damage(world, host, source, unit, target_damage, damage_type, action_kind, context);
	});
	return total_damage;
}

bool apply_knockback(SimWorld &world, SimHostCallbacks &host, UnitState &source, UnitState &target, double distance, bool away_from_source) {
	if (distance <= 0.0 || !target.alive) {
		return false;
	}
	const double tenacity = target.stats_dirty ? get_effective_tenacity(target) : target.cached_tenacity;
	const double effective_distance = distance * (1.0 - tenacity);
	if (effective_distance <= EPSILON) {
		return false;
	}
	const double old_x = target.pos_x;
	const double old_y = target.pos_y;
	const double sx = source.pos_x;
	const double sy = source.pos_y;
	const double tx = target.pos_x;
	const double ty = target.pos_y;
	const double dx = tx - sx;
	const double dy = ty - sy;
	const double dist = distance_between_coords(sx, sy, tx, ty);
	double nx = 0.0;
	double ny = 0.0;
	if (dist <= EPSILON) {
		nx = (tx >= WORLD_SIZE * 0.5) ? 1.0 : -1.0;
		ny = 0.0;
	} else {
		nx = dx / dist;
		ny = dy / dist;
	}
	if (!away_from_source) {
		nx = -nx;
		ny = -ny;
	}
	const double new_x = tx + nx * effective_distance;
	const double new_y = ty + ny * effective_distance;
	const Vector2 valid = movement::find_valid_dash_position(world, tx, ty, new_x, new_y, effective_distance, target.instance_id);
	target.pos_x = Math::clamp(static_cast<double>(valid.x), WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	target.pos_y = Math::clamp(static_cast<double>(valid.y), WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX);
	on_unit_position_changed(world, target);
	if (host.sync_targeting_frame_unit != nullptr) {
		host.sync_targeting_frame_unit(host.user_data, target);
	}
	return Math::abs(target.pos_x - old_x) > EPSILON || Math::abs(target.pos_y - old_y) > EPSILON;
}

bool apply_aoe_knockback(SimWorld &world, SimHostCallbacks &host, UnitState &source, double radius, double distance, bool away_from_source) {
	return apply_aoe_knockback_shape(world, host, source, nullptr, make_circle_self_aoe(radius), distance, away_from_source);
}

bool apply_aoe_knockback_shape(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double distance,
		bool away_from_source) {
	if (effect.aoe_shape_params.radius <= 0.0 || distance <= 0.0) {
		return false;
	}
	record_aoe_shape_fx(host.viewer_hooks, world, source, target, effect, StringName("aoe_knockback"));
	bool applied = false;
	for_each_enemy_in_aoe_shape(world, source, target, effect, 0, [&](UnitState &unit) {
		applied = apply_knockback(world, host, source, unit, distance, away_from_source) || applied;
	});
	return applied;
}


} // namespace periodic
} // namespace sim
