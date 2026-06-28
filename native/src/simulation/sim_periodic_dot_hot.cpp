#include "sim_periodic.hpp"

#include "sim_periodic_internal.hpp"

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

using namespace internal;

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

	const double source_attack_damage = source.stats_dirty ? get_effective_attack_damage(source) : source.cached_attack_damage;
	double damage_total = source_attack_damage * attack_damage_ratio;
	damage_total += (target.stats_dirty ? get_effective_max_hp(target) : target.cached_max_hp) * max_hp_ratio;
	damage_total += flat_amount;

	if (duration <= 0.0 || damage_total <= 0.0) {
		return;
	}

	const double tick_count = duration / tick_interval;
	const double tick_count_rounded = Math::floor(tick_count);
	if (tick_count != tick_count_rounded) {
		UtilityFunctions::push_warning(vformat("DoT effect '%s' has non-divisible duration (%f) for tick_interval (%f). tick_count is %f, rounded to %f. Duration adjusted from %f to %f.", String(effect_type), duration, tick_interval, tick_count, tick_count_rounded, duration, tick_count_rounded * tick_interval));
	}
	if (tick_count_rounded < 1.0) {
		UtilityFunctions::push_error(vformat("DoT effect '%s' has duration (%f) too short for tick_interval (%f). tick_count is %f (should be >= 1).", String(effect_type), duration, tick_interval, tick_count));
		return;
	}
	const double adjusted_duration = tick_count_rounded * tick_interval;

	auto &periodic_effects = uc(world, target).periodic_effects;

	if (stacking_mode != StringName("separate")) {
		for (auto &existing : periodic_effects) {
			if (existing.effect_type == effect_type && existing.source_instance_id == source.instance_id && existing.reason == reason) {
				if (stacking_mode == StringName("refresh")) {
					existing.damage_total = damage_total;
					existing.total_attack_damage_ratio = attack_damage_ratio;
					existing.total_max_hp_ratio = max_hp_ratio;
					existing.total_flat_amount = flat_amount;
					existing.remaining_duration = adjusted_duration;
					existing.original_tick_count = tick_count_rounded + 1.0;
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
					existing.remaining_duration = existing.remaining_duration + adjusted_duration;
					if (existing.tick_interval <= 0.0) {
						existing.tick_interval = tick_interval;
					}
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

	const double per_tick = damage_total / (tick_count_rounded + 1.0);
	if (per_tick > 0.0) {
		EffectContext context = combat::build_context(source, &target, nullptr, per_tick, action_kind);
		damage::apply_damage(world, host, source, target, per_tick, damage_type, action_kind, context);
	}

	const double adjusted_damage_total = per_tick * (tick_count_rounded + 1.0);

	UnitStateCold::PeriodicEffect new_effect;
	new_effect.effect_type = effect_type;
	new_effect.damage_total = adjusted_damage_total;
	new_effect.heal_total = 0.0;
	new_effect.remaining_duration = adjusted_duration;
	new_effect.tick_interval = tick_interval;
	new_effect.original_tick_count = tick_count_rounded + 1.0;
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

	double heal_total = (target.stats_dirty ? get_effective_max_hp(target) : target.cached_max_hp) * max_hp_ratio;
	heal_total += target.hp * current_hp_ratio;
	heal_total += ((target.stats_dirty ? get_effective_max_hp(target) : target.cached_max_hp) - target.hp) * missing_hp_ratio;
	heal_total += flat_amount;

	if (duration <= 0.0 || heal_total <= 0.0) {
		return;
	}

	if (host.viewer_record_hot_status_fx != nullptr) {
		host.viewer_record_hot_status_fx(host.user_data, target, duration, effect_type);
	}

	const double tick_count = duration / tick_interval;
	const double tick_count_rounded = Math::floor(tick_count);
	if (tick_count != tick_count_rounded) {
		UtilityFunctions::push_warning(vformat("HoT effect '%s' has non-divisible duration (%f) for tick_interval (%f). tick_count is %f, rounded to %f. Duration adjusted from %f to %f.", String(effect_type), duration, tick_interval, tick_count, tick_count_rounded, duration, tick_count_rounded * tick_interval));
	}
	if (tick_count_rounded < 1.0) {
		UtilityFunctions::push_error(vformat("HoT effect '%s' has duration (%f) too short for tick_interval (%f). tick_count is %f (should be >= 1).", String(effect_type), duration, tick_interval, tick_count));
		return;
	}
	const double adjusted_duration = tick_count_rounded * tick_interval;

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
					existing.remaining_duration = adjusted_duration;
					existing.original_tick_count = tick_count_rounded + 1.0;
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
					existing.remaining_duration = existing.remaining_duration + adjusted_duration;
					if (existing.tick_interval <= 0.0) {
						existing.tick_interval = tick_interval;
					}
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

	const double per_tick = heal_total / (tick_count_rounded + 1.0);
	if (per_tick > 0.0) {
		apply_hot_tick_heal(world, host, source, target, per_tick, action_kind, allow_overheal);
	}

	const double adjusted_heal_total = per_tick * (tick_count_rounded + 1.0);

	UnitStateCold::PeriodicEffect new_effect;
	new_effect.effect_type = effect_type;
	new_effect.damage_total = 0.0;
	new_effect.heal_total = adjusted_heal_total;
	new_effect.remaining_duration = adjusted_duration;
	new_effect.tick_interval = tick_interval;
	new_effect.original_tick_count = tick_count_rounded + 1.0;
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
						double damage_total = (source->stats_dirty ? get_effective_attack_damage(*source) : source->cached_attack_damage) * effect.total_attack_damage_ratio;
						damage_total += (unit.stats_dirty ? get_effective_max_hp(unit) : unit.cached_max_hp) * effect.total_max_hp_ratio;
						damage_total += effect.total_flat_amount;
						const double remaining_ticks = effect.remaining_duration / effect.tick_interval;
						damage_per_tick = damage_total / (remaining_ticks + 1.0);
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
						double heal_total = (unit.stats_dirty ? get_effective_max_hp(unit) : unit.cached_max_hp) * effect.total_max_hp_ratio;
						heal_total += unit.hp * effect.total_current_hp_ratio;
						heal_total += ((unit.stats_dirty ? get_effective_max_hp(unit) : unit.cached_max_hp) - unit.hp) * effect.total_missing_hp_ratio;
						heal_total += effect.total_flat_amount;
						const double remaining_ticks = effect.remaining_duration / effect.tick_interval;
						heal_per_tick = heal_total / (remaining_ticks + 1.0);
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
			periodic_effects[index] = periodic_effects.back();
			periodic_effects.pop_back();
		} else {
			periodic_effects[index] = effect;
			++index;
		}
	}
}

} // namespace periodic
} // namespace sim
