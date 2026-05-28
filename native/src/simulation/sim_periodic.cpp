#include "sim_periodic.hpp"

#include "sim_viewer.hpp"
#include "sim_combat.hpp"
#include "sim_constants.hpp"
#include "sim_damage.hpp"
#include "sim_movement.hpp"
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
namespace {

inline const StringName &sn_player() {
	static const StringName s("player");
	return s;
}

inline const StringName &sn_enemy() {
	static const StringName s("enemy");
	return s;
}

inline const StringName &sn_taunt() {
	static const StringName s("taunt");
	return s;
}

UnitState *resolve_shape_target(SimWorld &world, UnitState *target, const EffectRecord &effect) {
	if (target != nullptr) {
		return target;
	}
	if (effect.aoe_shape_params.target_id == 0) {
		return nullptr;
	}
	return targeting::unit_by_id(world, effect.aoe_shape_params.target_id);
}

EffectRecord make_circle_self_aoe(double radius) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	return effect;
}

template<typename Fn>
void for_each_unit_in_aoe_shape(
		SimWorld &world,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		const StringName &team,
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
	const StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
	for_each_unit_in_aoe_shape(world, source, target, effect, enemy_team, exclude_instance_id, std::forward<Fn>(fn));
}

template<typename Fn>
void for_each_ally_in_aoe_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, int64_t exclude_instance_id, Fn &&fn) {
	const StringName ally_team = source.team == sn_player() ? sn_player() : sn_enemy();
	for_each_unit_in_aoe_shape(world, source, target, effect, ally_team, exclude_instance_id, std::forward<Fn>(fn));
}

void sync_targeting_if_healed(SimHostCallbacks &host, UnitState &target, double gained) {
	if (gained > 1e-9 && host.sync_targeting_frame_unit != nullptr) {
		host.sync_targeting_frame_unit(host.user_data, target);
	}
}

void apply_hot_tick_heal(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double heal_per_tick,
		const StringName &action_kind,
		bool allow_overheal) {
	EffectContext context = combat::build_context(source, &target, nullptr, 0.0, action_kind);
	const double gained = status::heal_unit(world, source, target, heal_per_tick, action_kind, allow_overheal);
	sync_targeting_if_healed(host, target, gained);
	combat::run_post_heal_effects(world, host, source, target, heal_per_tick, gained, context);
}

} // namespace

void apply_dot(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
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
		const StringName &action_kind,
		bool is_dynamic) {
	if (!target.alive) {
		return;
	}

	if (tick_interval <= 0.0) {
		UtilityFunctions::push_error(vformat("DoT effect '%s' has invalid tick_interval: %f. Must be > 0.", String(effect_type), tick_interval));
		return;
	}

	const double source_attack_damage = get_effective_attack_damage(source);
	double damage_total = source_attack_damage * attack_damage_ratio;
	damage_total += get_effective_max_hp(target) * max_hp_ratio;
	damage_total += flat_amount;

	if (duration <= 0.0 || damage_total <= 0.0) {
		return;
	}

	const double tick_count = duration / tick_interval;
	const double tick_count_rounded = Math::round(tick_count);
	if (Math::abs(tick_count - tick_count_rounded) > 0.0001) {
		UtilityFunctions::push_error(vformat("DoT effect '%s' has non-divisible duration (%f) and tick_interval (%f). tick_count is %f (should be integer).", String(effect_type), duration, tick_interval, tick_count));
		return;
	}

	auto &periodic_effects = uc(world, target).periodic_effects;

	if (stacking_mode != StringName("separate")) {
		for (auto &existing : periodic_effects) {
			if (existing.effect_type == effect_type && existing.source_instance_id == source.instance_id && existing.reason == reason) {
				if (stacking_mode == StringName("refresh")) {
					existing.damage_total = damage_total;
					existing.total_attack_damage_ratio = attack_damage_ratio;
					existing.total_max_hp_ratio = max_hp_ratio;
					existing.total_flat_amount = flat_amount;
					existing.remaining_duration = duration;
					existing.original_tick_count = tick_count;
					existing.calculation_mode = is_dynamic ? StringName("dynamic") : StringName("fixed");
					return;
				}
				if (stacking_mode == StringName("extend")) {
					double remaining_damage = 0.0;
					double damage_scaling_factor = 0.0;
					if (existing.original_tick_count > 0.0 && existing.damage_total > 0.0) {
						const double per_tick_damage = existing.damage_total / existing.original_tick_count;
						const double remaining_ticks = existing.remaining_duration / existing.tick_interval;
						remaining_damage = per_tick_damage * remaining_ticks;
						damage_scaling_factor = remaining_damage / existing.damage_total;
					}

					existing.damage_total = remaining_damage + damage_total;
					existing.total_attack_damage_ratio = existing.total_attack_damage_ratio * damage_scaling_factor + attack_damage_ratio;
					existing.total_max_hp_ratio = existing.total_max_hp_ratio * damage_scaling_factor + max_hp_ratio;
					existing.total_flat_amount = existing.total_flat_amount * damage_scaling_factor + flat_amount;
					existing.remaining_duration = existing.remaining_duration + duration;
					if (existing.tick_interval <= 0.0) {
						existing.tick_interval = tick_interval;
					}
					existing.original_tick_count = existing.remaining_duration / existing.tick_interval;
					return;
				}
			}
		}
	}

	if (stacking_mode == StringName("separate") && max_stacks > 0) {
		int count = 0;
		for (const auto &existing : periodic_effects) {
			if (existing.effect_type == effect_type && existing.source_instance_id == source.instance_id && existing.reason == reason) {
				count++;
			}
		}
		if (count >= max_stacks) {
			return;
		}
	}

	const double per_tick = damage_total / (tick_count + 1.0);
	if (per_tick > 0.0) {
		EffectContext context = combat::build_context(source, &target, nullptr, per_tick, action_kind);
		damage::apply_damage(world, host, source, target, per_tick, damage_type, action_kind, context);
	}

	const double adjusted_damage_total = per_tick * tick_count;

	UnitStateCold::PeriodicEffect new_effect;
	new_effect.effect_type = effect_type;
	new_effect.damage_total = adjusted_damage_total;
	new_effect.heal_total = 0.0;
	new_effect.remaining_duration = duration;
	new_effect.tick_interval = tick_interval;
	new_effect.original_tick_count = tick_count;
	new_effect.tick_accumulator = 0.0;
	new_effect.source_instance_id = source.instance_id;
	new_effect.damage_type = damage_type;
	new_effect.stacking_mode = stacking_mode;
	new_effect.reason = reason;
	new_effect.allow_overheal = false;
	new_effect.stack_count = 1;
	new_effect.max_stacks = max_stacks;
	new_effect.action_kind = action_kind;
	new_effect.total_attack_damage_ratio = attack_damage_ratio;
	new_effect.total_max_hp_ratio = max_hp_ratio;
	new_effect.total_flat_amount = flat_amount;
	new_effect.calculation_mode = is_dynamic ? StringName("dynamic") : StringName("fixed");
	periodic_effects.push_back(new_effect);
}

void apply_hot(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
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
		const StringName &action_kind,
		bool is_dynamic) {
	if (!target.alive) {
		return;
	}

	if (tick_interval <= 0.0) {
		UtilityFunctions::push_error(vformat("HoT effect '%s' has invalid tick_interval: %f. Must be > 0.", String(effect_type), tick_interval));
		return;
	}

	double heal_total = get_effective_max_hp(target) * max_hp_ratio;
	heal_total += target.hp * current_hp_ratio;
	heal_total += (get_effective_max_hp(target) - target.hp) * missing_hp_ratio;
	heal_total += flat_amount;

	if (duration <= 0.0 || heal_total <= 0.0) {
		return;
	}

	if (host.viewer_record_hot_status_fx != nullptr) {
		host.viewer_record_hot_status_fx(host.user_data, target, duration, effect_type);
	}

	const double tick_count = duration / tick_interval;
	const double tick_count_rounded = Math::round(tick_count);
	if (Math::abs(tick_count - tick_count_rounded) > 0.0001) {
		UtilityFunctions::push_error(vformat("HoT effect '%s' has non-divisible duration (%f) and tick_interval (%f). tick_count is %f (should be integer).", String(effect_type), duration, tick_interval, tick_count));
		return;
	}

	auto &periodic_effects = uc(world, target).periodic_effects;
	if (stacking_mode != StringName("separate")) {
		for (auto &existing : periodic_effects) {
			if (existing.effect_type == effect_type && existing.source_instance_id == source.instance_id && existing.reason == reason) {
				if (stacking_mode == StringName("refresh")) {
					existing.heal_total = heal_total;
					existing.total_max_hp_ratio = max_hp_ratio;
					existing.total_current_hp_ratio = current_hp_ratio;
					existing.total_missing_hp_ratio = missing_hp_ratio;
					existing.total_flat_amount = flat_amount;
					existing.allow_overheal = allow_overheal;
					existing.remaining_duration = duration;
					existing.original_tick_count = tick_count;
					existing.calculation_mode = is_dynamic ? StringName("dynamic") : StringName("fixed");
					return;
				}
				if (stacking_mode == StringName("extend")) {
					double remaining_heal = 0.0;
					double heal_scaling_factor = 0.0;
					if (existing.original_tick_count > 0.0 && existing.heal_total > 0.0) {
						const double per_tick_heal = existing.heal_total / existing.original_tick_count;
						const double remaining_ticks = existing.remaining_duration / existing.tick_interval;
						remaining_heal = per_tick_heal * remaining_ticks;
						heal_scaling_factor = remaining_heal / existing.heal_total;
					}

					existing.heal_total = remaining_heal + heal_total;
					existing.total_max_hp_ratio = existing.total_max_hp_ratio * heal_scaling_factor + max_hp_ratio;
					existing.total_current_hp_ratio = existing.total_current_hp_ratio * heal_scaling_factor + current_hp_ratio;
					existing.total_missing_hp_ratio = existing.total_missing_hp_ratio * heal_scaling_factor + missing_hp_ratio;
					existing.total_flat_amount = existing.total_flat_amount * heal_scaling_factor + flat_amount;
					existing.remaining_duration = existing.remaining_duration + duration;
					if (existing.tick_interval <= 0.0) {
						existing.tick_interval = tick_interval;
					}
					existing.original_tick_count = existing.remaining_duration / existing.tick_interval;
					return;
				}
			}
		}
	}

	if (stacking_mode == StringName("separate") && max_stacks > 0) {
		int count = 0;
		for (const auto &existing : periodic_effects) {
			if (existing.effect_type == effect_type && existing.source_instance_id == source.instance_id && existing.reason == reason) {
				count++;
			}
		}
		if (count >= max_stacks) {
			return;
		}
	}

	const double per_tick = heal_total / (tick_count + 1.0);
	if (per_tick > 0.0) {
		apply_hot_tick_heal(world, host, source, target, per_tick, action_kind, allow_overheal);
	}

	const double adjusted_heal_total = per_tick * tick_count;

	UnitStateCold::PeriodicEffect new_effect;
	new_effect.effect_type = effect_type;
	new_effect.damage_total = 0.0;
	new_effect.heal_total = adjusted_heal_total;
	new_effect.remaining_duration = duration;
	new_effect.tick_interval = tick_interval;
	new_effect.original_tick_count = tick_count;
	new_effect.tick_accumulator = 0.0;
	new_effect.source_instance_id = source.instance_id;
	new_effect.damage_type = StringName();
	new_effect.stacking_mode = stacking_mode;
	new_effect.reason = reason;
	new_effect.allow_overheal = allow_overheal;
	new_effect.stack_count = 1;
	new_effect.max_stacks = max_stacks;
	new_effect.action_kind = action_kind;
	new_effect.total_max_hp_ratio = max_hp_ratio;
	new_effect.total_current_hp_ratio = current_hp_ratio;
	new_effect.total_missing_hp_ratio = missing_hp_ratio;
	new_effect.total_flat_amount = flat_amount;
	new_effect.calculation_mode = is_dynamic ? StringName("dynamic") : StringName("fixed");
	periodic_effects.push_back(new_effect);
}

void tick_periodic_effects(SimWorld &world, SimHostCallbacks &host, UnitState &unit, double delta) {
	auto &periodic_effects = uc(world, unit).periodic_effects;
	size_t index = 0;
	while (index < periodic_effects.size()) {
		UnitStateCold::PeriodicEffect effect = periodic_effects[index];
		effect.tick_accumulator += delta;

		while (effect.tick_accumulator >= effect.tick_interval) {
			effect.tick_accumulator -= effect.tick_interval;

			double tick_count = effect.original_tick_count;
			if (tick_count <= 0.0) {
				if (effect.tick_interval <= 0.0) {
					index++;
					continue;
				}
				tick_count = effect.remaining_duration / effect.tick_interval;
				if (tick_count <= 0.0) {
					index++;
					continue;
				}
			}

			double damage_per_tick = 0.0;
			if (effect.damage_total > 0.0) {
				if (effect.calculation_mode == StringName("dynamic")) {
					UnitState *source = targeting::unit_by_id(world, effect.source_instance_id);
					if (source != nullptr) {
						double damage_total = get_effective_attack_damage(*source) * effect.total_attack_damage_ratio;
						damage_total += get_effective_max_hp(unit) * effect.total_max_hp_ratio;
						damage_total += effect.total_flat_amount;
						damage_per_tick = damage_total / (tick_count + 1.0);
					} else {
						damage_per_tick = effect.damage_total / tick_count;
					}
				} else {
					damage_per_tick = effect.damage_total / tick_count;
				}
			}

			double heal_per_tick = 0.0;
			if (effect.heal_total > 0.0) {
				if (effect.calculation_mode == StringName("dynamic")) {
					UnitState *source = targeting::unit_by_id(world, effect.source_instance_id);
					if (source != nullptr) {
						double heal_total = get_effective_max_hp(unit) * effect.total_max_hp_ratio;
						heal_total += unit.hp * effect.total_current_hp_ratio;
						heal_total += (get_effective_max_hp(unit) - unit.hp) * effect.total_missing_hp_ratio;
						heal_total += effect.total_flat_amount;
						heal_per_tick = heal_total / (tick_count + 1.0);
					} else {
						heal_per_tick = effect.heal_total / tick_count;
					}
				} else {
					heal_per_tick = effect.heal_total / tick_count;
				}
			}

			if (damage_per_tick > 0.0) {
				UnitState *source = targeting::unit_by_id(world, effect.source_instance_id);
				if (source != nullptr) {
					EffectContext context = combat::build_context(*source, &unit, nullptr, damage_per_tick, effect.action_kind);
					damage::apply_damage(world, host, *source, unit, damage_per_tick, effect.damage_type, effect.action_kind, context);
					if (!unit.alive) {
						return;
					}
				} else {
					UtilityFunctions::push_error(vformat("[DEBUG] Skipping DoT application: source is null (source_instance_id=%d, damage_per_tick=%.2f)", effect.source_instance_id, damage_per_tick));
				}
			}

			if (heal_per_tick > 0.0) {
				UnitState *source = targeting::unit_by_id(world, effect.source_instance_id);
				if (source != nullptr) {
					apply_hot_tick_heal(world, host, *source, unit, heal_per_tick, effect.action_kind, effect.allow_overheal);
					if (!unit.alive) {
						return;
					}
				} else {
					UtilityFunctions::push_error(vformat("[DEBUG] Skipping HoT application: source is null (source_instance_id=%d, heal_per_tick=%.2f)", effect.source_instance_id, heal_per_tick));
				}
			}
		}

		effect.remaining_duration -= delta;
		if (index >= periodic_effects.size()) {
			continue;
		}

		if (effect.remaining_duration <= 0.0) {
			periodic_effects.erase(periodic_effects.begin() + static_cast<std::ptrdiff_t>(index));
		} else {
			periodic_effects[index] = effect;
			++index;
		}
	}
}

void cleanse_dots(SimWorld &world, UnitState &unit, const StringName &effect_type_filter) {
	auto &periodic_effects = uc(world, unit).periodic_effects;
	if (effect_type_filter.is_empty()) {
		periodic_effects.erase(
				std::remove_if(periodic_effects.begin(), periodic_effects.end(), [](const UnitStateCold::PeriodicEffect &e) { return e.damage_total > 0.0; }),
				periodic_effects.end());
	} else {
		periodic_effects.erase(
				std::remove_if(
						periodic_effects.begin(),
						periodic_effects.end(),
						[&effect_type_filter](const UnitStateCold::PeriodicEffect &e) { return e.damage_total > 0.0 && e.effect_type == effect_type_filter; }),
				periodic_effects.end());
	}
}

void clear_periodic_effects(SimWorld &world, UnitState &unit) {
	uc(world, unit).periodic_effects.clear();
}

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
		const double tenacity = get_effective_tenacity(unit);
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
		double target_damage = get_effective_max_hp(unit) * max_hp_ratio;
		target_damage += get_effective_attack_damage(source) * damage_ratio;
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
	const double tenacity = get_effective_tenacity(target);
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

void apply_reflect_buff(
		SimWorld &world,
		UnitState &source,
		UnitState &target,
		double pct,
		double duration,
		const StringName &action_kind,
		const StringName &damage_type,
		const String &reason) {
	(void)action_kind;
	if (duration <= 0.0 || pct <= 0.0) {
		return;
	}
	const double p = Math::clamp(pct, 0.0, 1.0);

	UnitStateCold::ReflectBuff new_buff;
	new_buff.percentage = p;
	new_buff.remaining_duration = duration;
	new_buff.action_kind = action_kind;
	new_buff.source_instance_id = source.instance_id;
	new_buff.damage_type = damage_type;
	new_buff.reason = reason;

	uc(world, target).reflect_buffs.push_back(new_buff);
}

void apply_aoe_reflect(SimWorld &world, SimHostCallbacks &host, UnitState &source, double radius, double pct, double duration, bool all_damage_types) {
	(void)host;
	apply_aoe_reflect_shape(
			world,
			host,
			source,
			nullptr,
			make_circle_self_aoe(radius),
			pct,
			duration,
			all_damage_types,
			StringName("ability"),
			String("aoe_reflect"));
}

void apply_aoe_reflect_shape(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double pct,
		double duration,
		bool all_damage_types,
		const StringName &action_kind,
		const String &reason) {
	if (effect.aoe_shape_params.radius <= 0.0 || duration <= 0.0 || pct <= 0.0) {
		return;
	}
	record_aoe_shape_fx(host.viewer_hooks, world, source, target, effect, StringName("aoe_reflect"));
	const StringName damage_type = all_damage_types ? StringName("all") : StringName("physical");
	for_each_ally_in_aoe_shape(world, source, target, effect, 0, [&](UnitState &ally) {
		apply_reflect_buff(world, source, ally, pct, duration, action_kind, damage_type, reason);
	});
}

} // namespace periodic
} // namespace sim
