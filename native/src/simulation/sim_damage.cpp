#include "sim_damage.hpp"

#include "sim_constants.hpp"
#include "sim_spatial.hpp"
#include "sim_stats.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim::damage {
namespace {

constexpr size_t EFFECT_BUCKET_ON_ATTACK = 0;
constexpr size_t EFFECT_BUCKET_ON_DEFENSE = 1;
constexpr size_t EFFECT_BUCKET_ON_ALLY_DEFENSE = 2;
constexpr size_t EFFECT_BUCKET_ON_ABILITY = 6;
constexpr size_t EFFECT_BUCKET_ON_ULTIMATE = 7;
constexpr size_t EFFECT_BUCKET_POST_TAKE_DAMAGE = 5;

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
inline const StringName &sn_physical() {
	static const StringName s("physical");
	return s;
}
inline const StringName &sn_magic() {
	static const StringName s("magic");
	return s;
}
inline const StringName &sn_true() {
	static const StringName s("true");
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

EffectContext build_context(
		UnitState &source,
		UnitState *target,
		UnitState *target_ally,
		double damage,
		const StringName &action_kind) {
	EffectContext context;
	context.suppress_reflect_chain = false;
	context.source = &source;
	context.target = target;
	context.target_ally = target_ally;
	context.damage = damage;
	context.action_kind = action_kind;
	if (target != nullptr) {
		context.distance = distance_between_coords(source.pos_x, source.pos_y, target->pos_x, target->pos_y);
	}
	return context;
}

UnitState *unit_by_id(SimWorld &world, int64_t instance_id) {
	auto it = world.unit_index_map.find(instance_id);
	if (it == world.unit_index_map.end()) {
		return nullptr;
	}
	const int64_t index = it->second;
	if (index < 0 || index >= int64_t(world.units.size())) {
		return nullptr;
	}
	return &world.units[static_cast<size_t>(index)];
}

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
		return !uc(world, target).reflect_buffs.empty() || !uc(world, target).passive_reflect_entries.empty();
	}
	return false;
}

void run_post_take_damage_passives(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &target,
		double total_damage,
		const EffectContext &context) {
	const std::vector<EffectRecord> &post_take_damage_effects = uc(world, target).passive_effects[EFFECT_BUCKET_POST_TAKE_DAMAGE];
	EffectContext post_context = context;
	post_context.source = &target;
	post_context.target = nullptr;
	post_context.damage = total_damage;
	post_context.action_kind = sn_passive();
	if (target.stealth_remaining > 0.0 && target.stealth_break_on_damage_taken) {
		target.stealth_remaining = 0.0;
		target.stealth_break_on_attack = false;
		target.stealth_break_on_ability = false;
		target.stealth_break_on_damage_taken = false;
	}
	if (host.execute_effect == nullptr) {
		return;
	}
	for (const EffectRecord &effect : post_take_damage_effects) {
		host.execute_effect(host.user_data, effect, post_context);
	}
}

} // namespace

double evaluate_multiplier_effect(
		SimWorld &world,
		SimHostCallbacks &host,
		const EffectRecord &effect,
		const EffectContext &context,
		double current_value) {
	(void)world;
	(void)current_value;
	switch (effect.opcode) {
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER:
			return effect.scalar0;
		case EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER: {
			if (effect.scalar0 > 0.0 && context.source != nullptr) {
				const double hp_ratio = context.source->hp / Math::max(0.0001, get_effective_max_hp(*context.source));
				if (hp_ratio > effect.scalar0) {
					return effect.scalar2;
				}
			}
			if (effect.scalar1 > 0.0 && context.target != nullptr) {
				const double target_hp = context.target->hp;
				const double target_max_hp = Math::max(0.0001, get_effective_max_hp(*context.target));
				if (target_hp / target_max_hp <= effect.scalar1) {
					return effect.scalar2;
				}
			}
			return 1.0;
		}
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
			return context.distance > effect.scalar0 ? effect.scalar1 : 1.0;
		case EFFECT_OPCODE_AUTO_DODGE:
			if (host.randf != nullptr && host.randf(host.user_data) < effect.scalar0) {
				return effect.scalar1;
			}
			return effect.scalar2;
		case EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER:
			if (context.target == nullptr) {
				return 1.0;
			}
			return target_has_status(world, *context.target, effect.damage_type) ? effect.scalar0 : 1.0;
		default:
			return 1.0;
	}
}

double defense_multiplier(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &target,
		UnitState &source,
		double damage,
		const StringName &action_kind) {
	double multiplier = 1.0;
	EffectContext context = build_context(source, &target, nullptr, damage, action_kind);
	const std::vector<EffectRecord> &effects = uc(world, target).passive_effects[EFFECT_BUCKET_ON_DEFENSE];
	for (const EffectRecord &effect : effects) {
		if (effect.opcode == EFFECT_OPCODE_AUTO_DODGE) {
			continue;
		}
		if (effect.opcode == EFFECT_OPCODE_STAT_MODIFIER) {
			EffectContext stat_context = context;
			stat_context.source = &target;
			stat_context.target = &target;
			if (host.execute_effect != nullptr) {
				host.execute_effect(host.user_data, effect, stat_context);
			}
			continue;
		}
		multiplier *= evaluate_multiplier_effect(world, host, effect, context, multiplier);
	}
	return multiplier;
}

double auto_dodge_multiplier(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &target,
		UnitState &source,
		double damage) {
	double multiplier = 1.0;
	EffectContext context = build_context(source, &target, nullptr, damage, sn_auto());
	const std::vector<EffectRecord> &effects = uc(world, target).passive_effects[EFFECT_BUCKET_ON_DEFENSE];
	for (const EffectRecord &effect : effects) {
		if (effect.opcode != EFFECT_OPCODE_AUTO_DODGE) {
			continue;
		}
		multiplier *= evaluate_multiplier_effect(world, host, effect, context, multiplier);
	}
	return multiplier;
}

double apply_attack_modifiers(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &unit,
		UnitState &target,
		double distance,
		double damage) {
	(void)distance;
	EffectContext context = build_context(unit, &target, nullptr, damage, sn_passive());
	const std::vector<EffectRecord> &effects = uc(world, unit).passive_effects[EFFECT_BUCKET_ON_ATTACK];
	double modified_damage = damage;
	for (const EffectRecord &effect : effects) {
		modified_damage *= evaluate_multiplier_effect(world, host, effect, context, modified_damage);
	}
	return modified_damage;
}

double apply_ability_modifiers(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &unit,
		UnitState *target,
		double damage) {
	EffectContext context = build_context(unit, target, nullptr, damage, sn_ability());
	const std::vector<EffectRecord> &effects = uc(world, unit).passive_effects[EFFECT_BUCKET_ON_ABILITY];
	double modified_damage = damage;
	for (const EffectRecord &effect : effects) {
		modified_damage *= evaluate_multiplier_effect(world, host, effect, context, modified_damage);
	}
	return modified_damage;
}

double apply_ultimate_modifiers(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &unit,
		UnitState *target,
		double damage) {
	EffectContext context = build_context(unit, target, nullptr, damage, sn_ultimate());
	const std::vector<EffectRecord> &effects = uc(world, unit).passive_effects[EFFECT_BUCKET_ON_ULTIMATE];
	double modified_damage = damage;
	for (const EffectRecord &effect : effects) {
		modified_damage *= evaluate_multiplier_effect(world, host, effect, context, modified_damage);
	}
	return modified_damage;
}

double apply_damage(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double damage,
		const StringName &damage_type,
		const StringName &action_kind,
		const EffectContext &context) {
	if (damage <= 0.0) {
		return 0.0;
	}

	const bool action_is_auto = action_kind == sn_auto();
	const bool action_is_ability = action_kind == sn_ability();
	const bool action_is_ultimate = action_kind == sn_ultimate();
	const bool action_is_passive = action_kind == sn_passive();
	const bool damage_is_physical = damage_type == sn_physical();
	const bool damage_is_magic = damage_type == sn_magic();
	double pre_res = damage;

	if (damage_type != sn_true()) {
		pre_res *= defense_multiplier(world, host, target, source, damage, action_kind);
	}

	if (action_is_auto) {
		pre_res *= auto_dodge_multiplier(world, host, target, source, damage);
	}

	double ally_defense_reduction = trigger_ally_defense_effects(world, host, target, source, pre_res, damage_type, action_kind, context);
	pre_res -= ally_defense_reduction;
	if (pre_res < 0.0) {
		pre_res = 0.0;
	}

	double final_damage = pre_res;
	if (damage_is_physical) {
		const double armor = get_effective_armor(target);
		final_damage *= (1.0 - armor);
	} else if (damage_is_magic) {
		const double mr = get_effective_magic_resist(target);
		final_damage *= (1.0 - mr);
	}
	const double incoming = Math::max(0.0, final_damage);

	if (incoming <= 0.0) {
		return 0.0;
	}
	const double shield_before = target.shield;
	const double absorbed = Math::min(shield_before, incoming);
	target.shield = Math::max(0.0, shield_before - absorbed);
	const double hp_loss = Math::max(0.0, incoming - absorbed);
	target.hp = Math::max(0.0, target.hp - hp_loss);
	uc(world, target).damage_received += hp_loss;
	uc(world, target).damage_mitigated += Math::max(0.0, pre_res - final_damage);
	const double total_damage = absorbed + hp_loss;
	const double max_hp = get_effective_max_hp(target);
	if (max_hp > 0.0 && hp_loss > max_hp * THREAT_BURST_THRESHOLD) {
		target.perceived_threat += (hp_loss / max_hp) * THREAT_BURST_MULTIPLIER;
	}
	if (host.sync_targeting_frame_unit != nullptr) {
		host.sync_targeting_frame_unit(host.user_data, target);
	}
	if (source.instance_id != target.instance_id) {
		uc(world, source).damage_dealt += total_damage;
		if (action_is_auto) {
			uc(world, source).damage_dealt_auto += total_damage;
		} else if (action_is_ability) {
			uc(world, source).damage_dealt_ability += total_damage;
		} else if (action_is_ultimate) {
			uc(world, source).damage_dealt_ultimate += total_damage;
		} else if (action_is_passive) {
			uc(world, source).damage_dealt_passive += total_damage;
		}
	}
	if (hp_loss > 0.0 || absorbed > 0.0) {
		touch_damage_source(world, target, source.instance_id, total_damage);
		uc(world, target).last_hit_time = world.time;
	}
	if (total_damage > 1e-9 && host.viewer_record_damage_fx != nullptr) {
		host.viewer_record_damage_fx(host.user_data, source, target, total_damage, action_kind, damage_type);
	}
	maybe_apply_reflect_damage(world, host, source, target, total_damage, damage_type, context);
	if (target.hp <= 0.0) {
		run_post_take_damage_passives(world, host, target, total_damage, context);
		if (host.handle_death != nullptr) {
			host.handle_death(host.user_data, source, target);
		}
		return total_damage;
	}
	run_post_take_damage_passives(world, host, target, total_damage, context);
	return total_damage;
}

void maybe_apply_reflect_damage(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &attacker,
		UnitState &defender,
		double total_damage_applied,
		const StringName &damage_type,
		const EffectContext &context) {
	if (context.suppress_reflect_chain) {
		return;
	}
	if (!attacker.alive || !defender.alive || attacker.instance_id == defender.instance_id) {
		return;
	}
	if (total_damage_applied <= 1e-9) {
		return;
	}

	for (const PassiveReflectEntry &entry : uc(world, defender).passive_reflect_entries) {
		bool applies = false;
		if (entry.damage_type == StringName("all")) {
			applies = true;
		} else if (entry.damage_type == StringName("physical") && damage_type == sn_physical()) {
			applies = true;
		}

		if (!applies) {
			continue;
		}

		const double reflected = total_damage_applied * entry.percentage;
		if (reflected > 1e-9) {
			EffectContext bounce = context;
			bounce.suppress_reflect_chain = true;
			bounce.source = &defender;
			bounce.target = &attacker;
			apply_damage(world, host, defender, attacker, reflected, damage_type, entry.action_kind, bounce);
		}
	}

	for (const UnitStateCold::ReflectBuff &buff : uc(world, defender).reflect_buffs) {
		if (buff.remaining_duration <= 0.0) {
			continue;
		}

		bool applies = false;
		if (buff.damage_type == StringName("all")) {
			applies = true;
		} else if (buff.damage_type == StringName("physical") && damage_type == sn_physical()) {
			applies = true;
		}

		if (!applies) {
			continue;
		}

		const double reflected = total_damage_applied * buff.percentage;
		if (reflected <= 1e-9) {
			continue;
		}

		UnitState *damage_source = unit_by_id(world, buff.source_instance_id);
		if (damage_source == nullptr || !damage_source->alive) {
			damage_source = &defender;
		}

		EffectContext bounce = context;
		bounce.suppress_reflect_chain = true;
		bounce.source = damage_source;
		bounce.target = &attacker;
		apply_damage(world, host, *damage_source, attacker, reflected, damage_type, buff.action_kind, bounce);
	}
}

void touch_damage_source(SimWorld &world, UnitState &target, int64_t source_id, double incoming_damage) {
	if (source_id <= 0 || incoming_damage <= 0.0) {
		return;
	}
	if (source_id == target.instance_id) {
		return;
	}
	UnitStateCold::DamageSourceEntry &entry = uc(world, target).damage_sources[source_id];
	entry.damage += incoming_damage;
	entry.last_time = world.time;
}

double trigger_ally_defense_effects(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &target,
		UnitState &source,
		double damage,
		const StringName &damage_type,
		const StringName &action_kind,
		const EffectContext &context) {
	if (context.suppress_reflect_chain) {
		return 0.0;
	}

	double total_redirected = 0.0;
	const StringName target_team = target.team;
	const std::vector<int64_t> &ally_indices = alive_indices_for_team(world, target_team);

	for (int64_t ally_index : ally_indices) {
		UnitState &ally = world.units[static_cast<size_t>(ally_index)];
		if (!ally.alive || ally.instance_id == target.instance_id) {
			continue;
		}

		const std::vector<EffectRecord> &ally_defense_effects = uc(world, ally).passive_effects[EFFECT_BUCKET_ON_ALLY_DEFENSE];
		if (ally_defense_effects.empty()) {
			continue;
		}

		const double ally_radius = uc(world, ally).on_ally_defense_radius;
		if (ally_radius <= 0.0) {
			continue;
		}
		const double dx = ally.pos_x - target.pos_x;
		const double dy = ally.pos_y - target.pos_y;
		const double dist_sq = dx * dx + dy * dy;
		const double radius_sq = ally_radius * ally_radius;

		if (dist_sq > radius_sq) {
			continue;
		}

		EffectContext ally_context = context;
		ally_context.source = &ally;
		ally_context.target = &target;
		ally_context.target_ally = nullptr;
		ally_context.damage = damage;
		ally_context.action_kind = action_kind;
		ally_context.distance = distance_between_coords(ally.pos_x, ally.pos_y, target.pos_x, target.pos_y);

		for (const EffectRecord &effect : ally_defense_effects) {
			if (effect.opcode == EFFECT_OPCODE_REDIRECT_DAMAGE) {
				const double redirect_ratio = effect.scalar0;
				const double reduction_ratio = effect.scalar1;
				const double redirect_cap = effect.scalar2;

				double redirected_damage = damage * redirect_ratio;
				if (redirect_cap > 0.0) {
					const double max_redirect = redirect_cap;
					if (redirected_damage > max_redirect) {
						redirected_damage = max_redirect;
					}
				}

				total_redirected += redirected_damage;

				const double mitigated_damage = redirected_damage * (1.0 - reduction_ratio);

				if (mitigated_damage > 1e-9) {
					const double mitigated_amount = redirected_damage - mitigated_damage;
					uc(world, ally).damage_mitigated += mitigated_amount;

					EffectContext redirect_context = context;
					redirect_context.source = &source;
					redirect_context.target = &ally;
					redirect_context.damage = mitigated_damage;
					redirect_context.action_kind = action_kind;
					redirect_context.suppress_reflect_chain = true;
					redirect_context.distance = distance_between_coords(source.pos_x, source.pos_y, ally.pos_x, ally.pos_y);
					apply_damage(world, host, source, ally, mitigated_damage, damage_type, redirect_context.action_kind, redirect_context);
				}
			} else if (host.execute_effect != nullptr) {
				host.execute_effect(host.user_data, effect, ally_context);
			}
		}
	}

	return total_redirected;
}

} // namespace sim::damage
