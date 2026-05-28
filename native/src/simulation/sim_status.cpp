#include "sim_status.hpp"

#include "sim_constants.hpp"
#include "sim_stats.hpp"
#include "sim_targeting.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace sim {
namespace status {
namespace {

inline const StringName &sn_player() {
	static const StringName s("player");
	return s;
}

inline const StringName &sn_enemy() {
	static const StringName s("enemy");
	return s;
}

inline const StringName &sn_auto() {
	static const StringName s("auto");
	return s;
}

inline const StringName &sn_ability() {
	static const StringName s("ability");
	return s;
}

inline const StringName &sn_ultimate() {
	static const StringName s("ultimate");
	return s;
}

inline const StringName &sn_passive() {
	static const StringName s("passive");
	return s;
}

inline const StringName &sn_slow() {
	static const StringName s("slow");
	return s;
}

inline const StringName &sn_root() {
	static const StringName s("root");
	return s;
}

inline const StringName &sn_silence() {
	static const StringName s("silence");
	return s;
}

inline const StringName &sn_disarm() {
	static const StringName s("disarm");
	return s;
}

inline const StringName &sn_stealth() {
	static const StringName s("stealth");
	return s;
}

inline const StringName &sn_stun() {
	static const StringName s("stun");
	return s;
}

inline const StringName &sn_reflect() {
	static const StringName s("reflect");
	return s;
}

void record_shielding_by_action_kind(UnitStateCold &source_cold, double amount, const StringName &action_kind) {
	source_cold.shielding_done += amount;
	if (action_kind == sn_auto()) {
		source_cold.shielding_done_auto += amount;
	} else if (action_kind == sn_ability()) {
		source_cold.shielding_done_ability += amount;
	} else if (action_kind == sn_ultimate()) {
		source_cold.shielding_done_ultimate += amount;
	} else if (action_kind == sn_passive()) {
		source_cold.shielding_done_passive += amount;
	}
}

void record_healing_by_action_kind(UnitStateCold &source_cold, double gained, const StringName &action_kind) {
	source_cold.healing_done += gained;
	if (action_kind == sn_auto()) {
		source_cold.healing_done_auto += gained;
	} else if (action_kind == sn_ability()) {
		source_cold.healing_done_ability += gained;
	} else if (action_kind == sn_ultimate()) {
		source_cold.healing_done_ultimate += gained;
	} else if (action_kind == sn_passive()) {
		source_cold.healing_done_passive += gained;
	}
}

void record_benefactor(SimWorld &world, UnitState &source, UnitState &target) {
	if (source.instance_id != target.instance_id) {
		uc(world, target).recent_benefactors[source.instance_id] = world.time;
	}
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

template<typename Fn>
void for_each_enemy_in_aoe_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, Fn &&fn) {
	const StringName enemy_team = source.team == sn_player() ? sn_enemy() : sn_player();
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

EffectRecord make_circle_self_aoe(double radius) {
	EffectRecord effect;
	effect.aoe_shape_params.shape = AoShapeKind::Circle;
	effect.aoe_shape_params.anchor = AoAnchorKind::Self;
	effect.aoe_shape_params.radius = radius;
	return effect;
}

} // namespace

bool target_has_status(const SimWorld &world, const UnitState &target, const StringName &status_kind) {
	if (status_kind == sn_slow()) {
		return target.slow_remaining > 0.0;
	}
	if (status_kind == sn_root()) {
		return target.root_remaining > 0.0;
	}
	if (status_kind == sn_silence()) {
		return target.silence_remaining > 0.0;
	}
	if (status_kind == sn_disarm()) {
		return target.disarm_remaining > 0.0;
	}
	if (status_kind == sn_stealth()) {
		return target.stealth_remaining > 0.0;
	}
	if (status_kind == sn_stun()) {
		return target.stun_remaining > 0.0;
	}
	if (status_kind == sn_reflect()) {
		const UnitStateCold &cold = uc(world, target);
		return !cold.reflect_buffs.empty() || !cold.passive_reflect_entries.empty();
	}
	return false;
}

void apply_stun(SimWorld &world, UnitState &source, UnitState &target, double duration) {
	if (duration <= 0.0) {
		return;
	}
	const double tenacity = get_effective_tenacity(target);
	const double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	target.stun_remaining = Math::max(target.stun_remaining, effective_duration);
	uc(world, source).stuns += 1;
}

void apply_slow(SimWorld &world, UnitState &source, UnitState &target, double slow_percentage, double duration) {
	(void)source;
	(void)world;
	if (duration <= 0.0 || slow_percentage <= 0.0) {
		return;
	}
	const double tenacity = get_effective_tenacity(target);
	const double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	const double pct = Math::clamp(slow_percentage, 0.0, 1.0);
	const double mult = Math::clamp(1.0 - pct, SLOW_MOVEMENT_MULTIPLIER_MIN, 1.0);
	target.slow_remaining = Math::max(target.slow_remaining, effective_duration);
	target.slow_move_mult = Math::min(target.slow_move_mult, mult);
}

void apply_root(SimWorld &world, UnitState &source, UnitState &target, double duration) {
	(void)source;
	(void)world;
	if (duration <= 0.0) {
		return;
	}
	const double tenacity = get_effective_tenacity(target);
	const double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	target.root_remaining = Math::max(target.root_remaining, effective_duration);
}

void apply_silence(SimWorld &world, UnitState &source, UnitState &target, double duration, bool block_abilities, bool block_ultimate) {
	(void)source;
	(void)world;
	if (duration <= 0.0) {
		return;
	}
	const double tenacity = get_effective_tenacity(target);
	const double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	if (block_abilities) {
		target.silence_ability_remaining = Math::max(target.silence_ability_remaining, effective_duration);
	}
	if (block_ultimate) {
		target.silence_ultimate_remaining = Math::max(target.silence_ultimate_remaining, effective_duration);
	}
	target.silence_remaining = Math::max(target.silence_ability_remaining, target.silence_ultimate_remaining);
	target.silence_blocks_abilities = target.silence_ability_remaining > 0.0;
	target.silence_blocks_ultimates = target.silence_ultimate_remaining > 0.0;
}

void apply_disarm(SimWorld &world, UnitState &source, UnitState &target, double duration) {
	(void)source;
	(void)world;
	if (duration <= 0.0) {
		return;
	}
	const double tenacity = get_effective_tenacity(target);
	const double effective_duration = duration * (1.0 - tenacity);
	if (effective_duration <= 0.0) {
		return;
	}
	target.disarm_remaining = Math::max(target.disarm_remaining, effective_duration);
}

void apply_stealth(SimWorld &world, UnitState &source, UnitState &target, double duration, bool break_on_attack, bool break_on_ability, bool break_on_damage_taken) {
	(void)source;
	(void)world;
	if (duration <= 0.0) {
		return;
	}
	target.stealth_remaining = duration;
	target.stealth_break_on_attack = break_on_attack;
	target.stealth_break_on_ability = break_on_ability;
	target.stealth_break_on_damage_taken = break_on_damage_taken;
}

void add_shield(SimWorld &world, UnitState &source, UnitState &target, double amount, const StringName &action_kind) {
	if (amount <= 0.0) {
		return;
	}
	target.shield += amount;
	record_shielding_by_action_kind(uc(world, source), amount, action_kind);
	record_benefactor(world, source, target);
}

double heal_unit(SimWorld &world, UnitState &source, UnitState &target, double amount, const StringName &action_kind, bool allow_overheal) {
	if (amount <= 0.0) {
		return 0.0;
	}

	const double max_hp = get_effective_max_hp(target);
	const double old_hp = target.hp;
	const double new_hp = allow_overheal ? old_hp + amount : Math::min(max_hp, old_hp + amount);

	target.hp = new_hp;
	const double gained = new_hp - old_hp;

	if (gained > 1e-9) {
		record_healing_by_action_kind(uc(world, source), gained, action_kind);
		record_benefactor(world, source, target);
	}
	return gained;
}

void restore_mana(SimWorld &world, UnitState &source, UnitState &target, double amount) {
	(void)source;
	(void)world;
	if (amount <= 0.0) {
		return;
	}
	const double max_mana = get_effective_max_mana(target);
	target.mana = Math::min(max_mana, target.mana + amount);
}

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

void apply_aoe_slow_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double slow_percentage, double duration) {
	for_each_enemy_in_aoe_shape(world, source, target, effect, [&](UnitState &unit) {
		apply_slow(world, source, unit, slow_percentage, duration);
	});
}

void apply_aoe_root(SimWorld &world, UnitState &source, double radius, double duration) {
	apply_aoe_root_shape(world, source, nullptr, make_circle_self_aoe(radius), duration);
}

void apply_aoe_root_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration) {
	for_each_enemy_in_aoe_shape(world, source, target, effect, [&](UnitState &unit) {
		apply_root(world, source, unit, duration);
	});
}

void apply_aoe_silence(SimWorld &world, UnitState &source, double radius, double duration, bool block_abilities, bool block_ultimate) {
	apply_aoe_silence_shape(world, source, nullptr, make_circle_self_aoe(radius), duration, block_abilities, block_ultimate);
}

void apply_aoe_silence_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration, bool block_abilities, bool block_ultimate) {
	for_each_enemy_in_aoe_shape(world, source, target, effect, [&](UnitState &unit) {
		apply_silence(world, source, unit, duration, block_abilities, block_ultimate);
	});
}

void apply_aoe_disarm(SimWorld &world, UnitState &source, double radius, double duration) {
	apply_aoe_disarm_shape(world, source, nullptr, make_circle_self_aoe(radius), duration);
}

void apply_aoe_disarm_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration) {
	for_each_enemy_in_aoe_shape(world, source, target, effect, [&](UnitState &unit) {
		apply_disarm(world, source, unit, duration);
	});
}

void apply_aoe_stun(SimWorld &world, UnitState &source, double radius, double duration) {
	apply_aoe_stun_shape(world, source, nullptr, make_circle_self_aoe(radius), duration);
}

void apply_aoe_stun_shape(SimWorld &world, UnitState &source, UnitState *target, const EffectRecord &effect, double duration) {
	for_each_enemy_in_aoe_shape(world, source, target, effect, [&](UnitState &unit) {
		apply_stun(world, source, unit, duration);
	});
}

} // namespace status
} // namespace sim
