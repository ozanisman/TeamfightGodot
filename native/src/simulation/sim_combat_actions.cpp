#include "sim_combat.hpp"
#include "sim_combat_internal.hpp"

#include "sim_constants.hpp"
#include "sim_damage.hpp"
#include "sim_stats.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/string.hpp>

namespace sim {
namespace combat {

using namespace internal;

namespace {

ProjectileState make_auto_projectile(UnitState &unit, UnitState &target, double damage) {
	ProjectileState projectile;
	projectile.source_id = unit.instance_id;
	projectile.target_id = target.instance_id;
	projectile.impact_effect.opcode = EFFECT_OPCODE_DAMAGE;
	projectile.impact_effect.scalar2 = damage;
	projectile.impact_effect.scalar3 = 1.0;
	projectile.impact_effect.damage_type = sn_physical();
	projectile.impact_effect.reason = String("Auto Attack");
	projectile.radius = unit.stats_dirty ? get_effective_projectile_radius(unit) : unit.cached_projectile_radius;
	projectile.speed = Math::max(0.0001, unit.stats_dirty ? get_effective_projectile_speed(unit) : unit.cached_projectile_speed);
	projectile.pos_x = unit.pos_x;
	projectile.pos_y = unit.pos_y;
	projectile.motion = sn_homing();
	projectile.collision = sn_target_only();
	projectile.on_target_lost = sn_drop();
	projectile.action_kind = sn_auto();
	projectile.reason = String("Auto Attack");
	return projectile;
}

} // namespace

bool try_cast_ability(SimWorld &world, SimHostCallbacks &host, const CombatHostHooks &hooks, UnitState &unit, UnitState &target, double distance) {
	(void)distance;
	if (unit.silence_remaining > 0.0 && unit.silence_blocks_abilities) {
		return false;
	}
	if (unit.root_remaining > 0.0 && unit.has_ability_effect && effect_record_contains_opcode(uc(world, unit).ability_effect, EFFECT_OPCODE_SELF_DASH)) {
		return false;
	}
	if (unit.ability_cooldown > 0.0) {
		return false;
	}
	if (uc(world, unit).is_channeling) {
		return false;
	}
	return start_cast(world, host, hooks, unit, target, distance, sn_ability());
}

bool try_cast_ultimate(SimWorld &world, SimHostCallbacks &host, const CombatHostHooks &hooks, UnitState &unit, UnitState &target, double distance) {
	(void)distance;
	if (unit.silence_remaining > 0.0 && unit.silence_blocks_ultimates) {
		return false;
	}
	if (unit.root_remaining > 0.0 && unit.has_ultimate_effect && effect_record_contains_opcode(uc(world, unit).ultimate_effect, EFFECT_OPCODE_SELF_DASH)) {
		return false;
	}
	if (uc(world, unit).is_channeling) {
		return false;
	}
	const double mana_cost = unit.stats_dirty ? get_effective_mana_cost(unit) : unit.cached_mana_cost;
	if (mana_cost <= 0.0 || unit.mana < mana_cost) {
		return false;
	}
	return start_cast(world, host, hooks, unit, target, distance, sn_ultimate());
}

bool start_cast(
		SimWorld &world,
		SimHostCallbacks &host,
		const CombatHostHooks &hooks,
		UnitState &unit,
		UnitState &target,
		double distance,
		const StringName &action_kind) {
	(void)distance;
	const bool has_effect = action_kind == sn_ability() ? unit.has_ability_effect : unit.has_ultimate_effect;
	if (!has_effect) {
		return false;
	}
	UnitState *target_ally = nullptr;
	if (hooks.select_ally_target != nullptr) {
		target_ally = hooks.select_ally_target(hooks.user_data, unit);
	}
	if (action_kind == sn_ability()) {
		ur(world, unit).abilities += 1;
	} else {
		unit.mana = Math::max(0.0, unit.mana - (unit.stats_dirty ? get_effective_mana_cost(unit) : unit.cached_mana_cost));
	}
	UnitStateCold &ucast = uc(world, unit);
	ucast.casting_kind = action_kind;
	ucast.casting_effect = action_kind == sn_ability() ? ucast.ability_effect : ucast.ultimate_effect;
	double windup = CASTING_WINDUP;
	if (ucast.casting_effect.windup >= 0.0) {
		windup = ucast.casting_effect.windup;
	}
	unit.casting_remaining = windup;
	unit.has_casting_effect = true;
	unit.casting_target_id = unit.target_id != 0 ? unit.target_id : target.instance_id;
	unit.casting_ally_target_id = unit.current_ally_target_id != 0 ? unit.current_ally_target_id : (target_ally == nullptr ? 0 : target_ally->instance_id);
	emit_trace(host, sn_cast_start(), unit.instance_id, target.instance_id, action_kind == sn_ultimate() ? 1.0 : 0.0);
	return true;
}

void resolve_cast(SimWorld &world, SimHostCallbacks &host, const CombatHostHooks &hooks, UnitState &unit) {
	(void)hooks;
	if (unit.stealth_remaining > 0.0 && unit.stealth_break_on_ability) {
		unit.stealth_remaining = 0.0;
		unit.stealth_break_on_attack = false;
		unit.stealth_break_on_ability = false;
		unit.stealth_break_on_damage_taken = false;
	}
	UnitStateCold &c = uc(world, unit);
	const EffectRecord effect = c.casting_effect;
	const StringName action_kind = c.casting_kind;
	UnitState *target = unit_by_id(world, unit.casting_target_id);
	UnitState *target_ally = unit_by_id(world, unit.casting_ally_target_id);
	const bool had_effect = unit.has_casting_effect;
	unit.casting_remaining = 0.0;
	c.casting_kind = StringName();
	c.casting_effect = EffectRecord();
	unit.has_casting_effect = false;
	unit.casting_target_id = 0;
	unit.casting_ally_target_id = 0;
	if (!had_effect) {
		return;
	}
	if (action_kind == sn_ability()) {
		unit.ability_cooldown = unit.stats_dirty ? get_effective_ability_cd(unit) : unit.cached_ability_cd;
	}
	if (effect_uses_projectile(effect) && (target == nullptr || !target->alive || target->stealth_remaining > 0.0)) {
		if (action_kind == sn_ability()) {
			unit.ability_cooldown = 0.0;
		} else if (action_kind == sn_ultimate()) {
			const double effective_mana_cost = unit.stats_dirty ? get_effective_mana_cost(unit) : unit.cached_mana_cost;
			unit.mana = Math::min(effective_mana_cost, unit.mana + effective_mana_cost);
		}
		return;
	}
	EffectContext context = build_context(unit, target, target_ally, 0.0, action_kind);
	if (host.execute_effect != nullptr) {
		host.execute_effect(host, effect, context);
	}
}

void perform_auto_attack(
		SimWorld &world,
		SimHostCallbacks &host,
		const CombatHostHooks &hooks,
		UnitState &unit,
		UnitState &target,
		double distance,
		std::vector<ProjectileState> &projectiles) {
	(void)hooks;
	if (unit.disarm_remaining > 0.0) {
		return;
	}
	if (target.stealth_remaining > 0.0) {
		return;
	}
	ur(world, unit).auto_attacks += 1;
	unit.attack_count += 1;
	double damage = unit.stats_dirty ? get_effective_attack_damage(unit) : unit.cached_attack_damage;
	damage = damage::apply_attack_modifiers(world, host, unit, target, distance, damage);
	if ((unit.stats_dirty ? get_effective_attack_range(unit) : unit.cached_attack_range) > RANGED_THRESHOLD) {
		ProjectileState projectile = make_auto_projectile(unit, target, damage);
		if (host.next_projectile_id != nullptr) {
			projectile.projectile_id = (*host.next_projectile_id)++;
		}
		projectiles.push_back(projectile);
		emit_trace(host, StringName("projectile"), unit.instance_id, target.instance_id, damage);
	} else {
		EffectContext context = build_context(unit, &target, nullptr, damage, sn_auto());
		const double dealt = damage::apply_damage(world, host, unit, target, damage, sn_physical(), sn_auto(), context);
		emit_trace(host, StringName("auto_melee"), unit.instance_id, target.instance_id, dealt);
		run_post_attack_effects(world, host, unit, target, dealt, context);
		const double life_steal = unit.stats_dirty ? get_effective_life_steal(unit) : unit.cached_life_steal;
		if (life_steal > 0.0) {
			const double old_hp = unit.hp;
			const double heal_amount = dealt * life_steal;
			heal_with_hooks(world, host, unit, unit, heal_amount, sn_auto(), false);
			const double heal_gained = unit.hp - old_hp;
			run_post_heal_effects(world, host, unit, unit, heal_amount, heal_gained, context);
		}
	}
	const double mana_gain = unit.stats_dirty ? get_effective_mana_per_attack(unit) : unit.cached_mana_per_attack;
	if (mana_gain > 0.0) {
		const double mana_cost = unit.stats_dirty ? get_effective_mana_cost(unit) : unit.cached_mana_cost;
		unit.mana = Math::min(mana_cost, unit.mana + mana_gain);
	}
	const double attack_speed = Math::max(0.0001, unit.stats_dirty ? get_effective_attack_speed(unit) : unit.cached_attack_speed);
	unit.attack_cooldown = 1.0 / attack_speed;
	unit.attack_period = unit.attack_cooldown;
}

} // namespace combat
} // namespace sim
