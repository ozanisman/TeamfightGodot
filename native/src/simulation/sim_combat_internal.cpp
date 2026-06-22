#include "sim_combat_internal.hpp"

#include "sim_effect_kinds.inl.hpp"
#include "sim_stats.hpp"
#include "sim_status.hpp"
#include "sim_targeting.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

namespace sim {
namespace combat {
namespace internal {

using namespace effect_kinds;

const StringName &sn_player() {
	static const StringName s("player");
	return s;
}
const StringName &sn_enemy() {
	static const StringName s("enemy");
	return s;
}
const StringName &sn_ability() {
	static const StringName s("ability");
	return s;
}
const StringName &sn_ultimate() {
	static const StringName s("ultimate");
	return s;
}
const StringName &sn_auto() {
	static const StringName s("auto");
	return s;
}
const StringName &sn_physical() {
	static const StringName s("physical");
	return s;
}
const StringName &sn_passive() {
	static const StringName s("passive");
	return s;
}
const StringName &sn_cast_start() {
	static const StringName s("cast_start");
	return s;
}
const StringName &sn_on_attack() {
	static const StringName s("on_attack");
	return s;
}
const StringName &sn_on_defense() {
	static const StringName s("on_defense");
	return s;
}
const StringName &sn_on_ally_defense() {
	static const StringName s("on_ally_defense");
	return s;
}
const StringName &sn_on_tick() {
	static const StringName s("on_tick");
	return s;
}
const StringName &sn_post_attack() {
	static const StringName s("post_attack");
	return s;
}
const StringName &sn_post_take_damage() {
	static const StringName s("post_take_damage");
	return s;
}
const StringName &sn_on_ability() {
	static const StringName s("on_ability");
	return s;
}
const StringName &sn_on_ultimate() {
	static const StringName s("on_ultimate");
	return s;
}
const StringName &sn_post_heal() {
	static const StringName s("post_heal");
	return s;
}
const StringName &sn_on_takedown() {
	static const StringName s("on_takedown");
	return s;
}
const StringName &sn_homing() {
	static const StringName s("homing");
	return s;
}
const StringName &sn_target_only() {
	static const StringName s("target_only");
	return s;
}
const StringName &sn_drop() {
	static const StringName s("drop");
	return s;
}

UnitState *unit_by_id(SimWorld &world, int64_t instance_id) {
	return targeting::unit_by_id(world, instance_id);
}

void heal_with_hooks(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double amount,
		const StringName &action_kind,
		bool allow_overheal) {
	status::heal_unit(world, source, target, amount, action_kind, allow_overheal, &host);
}

void emit_trace(SimHostCallbacks &host, const StringName &kind, int64_t src_id, int64_t tgt_id, double val) {
	if (host.emit_trace != nullptr) {
		host.emit_trace(host.user_data, kind, src_id, tgt_id, val);
	}
}

bool effect_uses_projectile(const EffectRecord &e) {
	if (e.opcode == EFFECT_OPCODE_PROJECTILE) {
		return true;
	}
	if (e.opcode == EFFECT_OPCODE_MULTI_EFFECT) {
		for (const EffectRecord &child : e.children) {
			if (effect_uses_projectile(child)) {
				return true;
			}
		}
	}
	if (e.opcode == EFFECT_OPCODE_MULTI_TARGET) {
		for (const EffectRecord &sub_effect : e.sub_effects) {
			if (effect_uses_projectile(sub_effect)) {
				return true;
			}
		}
	}
	return false;
}

namespace {

bool effect_target_self(const EffectRecord &e) {
	switch (e.opcode) {
		case EFFECT_OPCODE_DAMAGE:
		case EFFECT_OPCODE_DAMAGE_OVER_TIME:
		case EFFECT_OPCODE_HEAL:
		case EFFECT_OPCODE_SHIELD:
		case EFFECT_OPCODE_HEAL_OVER_TIME:
		case EFFECT_OPCODE_STAT_MODIFIER:
			return e.int0 != 0;
		case EFFECT_OPCODE_STEALTH:
		case EFFECT_OPCODE_AOE_HEAL_OVER_TIME:
			return e.int3 != 0;
		case EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME:
			return e.int2 != 0;
		default:
			return false;
	}
}

void apply_leaf_cast_range_spec(const EffectRecord &e, EffectCastRangeSpec &spec) {
	switch (e.opcode) {
		// Single-target ally effects: need a deliverable ally unless self-cast.
		case EFFECT_OPCODE_HEAL:
		case EFFECT_OPCODE_SHIELD:
		case EFFECT_OPCODE_HEAL_OVER_TIME:
			if (!effect_target_self(e)) {
				spec.needs_single_ally = true;
			}
			break;

		// Single-target enemy damage: need an enemy unless self-cast.
		case EFFECT_OPCODE_DAMAGE:
		case EFFECT_OPCODE_DAMAGE_OVER_TIME:
		case EFFECT_OPCODE_CONSUME_STACKS_DAMAGE:
			if (effect_target_self(e)) {
				break;
			}
			spec.needs_enemy = true;
			break;

		// Projectile always homes on an enemy target.
		case EFFECT_OPCODE_PROJECTILE:
			spec.needs_enemy = true;
			break;

		// Single-target enemy CC and enemy-only debuffs.
		case EFFECT_OPCODE_STUN:
		case EFFECT_OPCODE_SLOW:
		case EFFECT_OPCODE_ROOT:
		case EFFECT_OPCODE_SILENCE:
		case EFFECT_OPCODE_DISARM:
		case EFFECT_OPCODE_KNOCKBACK:
		case EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT:
			spec.needs_enemy = true;
			break;

		// Stat modifier: enemy debuff unless self-cast.
		case EFFECT_OPCODE_STAT_MODIFIER:
			if (!effect_target_self(e)) {
				spec.needs_enemy = true;
			}
			break;

		// Self/passive/buff effects that never require a target.
		case EFFECT_OPCODE_AUTO_DODGE:
		case EFFECT_OPCODE_REDIRECT_DAMAGE:
		case EFFECT_OPCODE_REFLECT_DAMAGE:
		case EFFECT_OPCODE_MANA_REGEN:
		case EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN:
		case EFFECT_OPCODE_MANA_RESTORE_ON_HIT:
		case EFFECT_OPCODE_EVERY_N_ATTACKS_STUN:
		case EFFECT_OPCODE_REFLECT:
		case EFFECT_OPCODE_KNOCKBACK_SHIELD:
		case EFFECT_OPCODE_STEALTH:
		case EFFECT_OPCODE_SET_STACKS:
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER:
		case EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER:
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
		case EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER:
			break;

		// Position/movement/self-scaling effects that skip proximity checks.
		case EFFECT_OPCODE_SUMMON_ALLY:
		case EFFECT_OPCODE_SELF_DASH:
		case EFFECT_OPCODE_DAMAGE_BASED_HEAL:
		case EFFECT_OPCODE_DAMAGE_BASED_SHIELD:
		case EFFECT_OPCODE_CONSUME_STACKS_HEAL:
		case EFFECT_OPCODE_CONSUME_STACKS_SHIELD:
			spec.skips_proximity = true;
			break;

		// Enemy AOE effects: require an enemy in range. Cast range is gated by
		// the ability's explicit cast_range field, not the shape radius.
		case EFFECT_OPCODE_AOE_TAUNT:
		case EFFECT_OPCODE_AOE_DAMAGE:
		case EFFECT_OPCODE_AOE_SLOW:
		case EFFECT_OPCODE_AOE_ROOT:
		case EFFECT_OPCODE_AOE_SILENCE:
		case EFFECT_OPCODE_AOE_DISARM:
		case EFFECT_OPCODE_AOE_KNOCKBACK:
		case EFFECT_OPCODE_AOE_STUN:
		case EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME:
			spec.needs_enemy = true;
			break;

		// Ally AOE effects: no enemy proximity requirement.
		case EFFECT_OPCODE_AOE_REFLECT:
		case EFFECT_OPCODE_AOE_HEAL_OVER_TIME:
			break;

		default:
			// Unknown opcode: default to needing an enemy and warn so newly added
			// opcodes are conservatively gated rather than silently allowed at any range.
			spec.needs_enemy = true;
			UtilityFunctions::push_warning(vformat("Unknown effect opcode %d in cast range spec; assuming needs_enemy.", e.opcode));
			break;
	}
}

} // namespace

EffectCastRangeSpec compile_cast_range_spec(const EffectRecord &e) {
	EffectCastRangeSpec spec;
	if (e.opcode == EFFECT_OPCODE_MULTI_EFFECT) {
		for (const EffectRecord &child : e.children) {
			spec.merge(compile_cast_range_spec(child));
		}
		return spec;
	}
	if (e.opcode == EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER) {
		for (const EffectRecord &child : e.children) {
			spec.merge(compile_cast_range_spec(child));
		}
		return spec;
	}
	if (e.opcode == EFFECT_OPCODE_CHANNEL) {
		// Channel cast range is determined by its tick sub-effect; optional
		// post-complete/interrupt effects do not affect whether the cast can start.
		if (!e.children.empty()) {
			spec.merge(compile_cast_range_spec(e.children[0]));
		}
		return spec;
	}
	if (e.opcode == EFFECT_OPCODE_MULTI_TARGET) {
		if (e.team_filter == sn_ally()) {
			spec.skips_proximity = true;
		} else {
			// Empty team_filter (auto-detect) defaults to enemy for cast-range gating;
			// the runtime still resolves the actual filter per effect logic.
			spec.needs_enemy = true;
		}
		return spec;
	}
	apply_leaf_cast_range_spec(e, spec);
	return spec;
}

bool is_in_cast_range(
		SimWorld &world,
		const UnitState &caster,
		const EffectCastRangeSpec &spec,
		const UnitState *enemy_target,
		int64_t current_ally_target_id,
		double cast_range) {
	// A pure skip-proximity effect (summon, self-AOE, ally multi_target) may ignore range.
	// If any actual target gate is required, that gate takes precedence over any skip flag.
	const bool has_target_gate = spec.needs_enemy || spec.needs_single_ally;
	if (!has_target_gate && spec.skips_proximity) {
		return true;
	}
	// cast_range < 0.0: use the caster's effective attack range.
	// cast_range == 0.0: no range gate (target existence still required).
	// cast_range > 0.0: gate at that distance.
	double effective_cast_range;
	if (cast_range < 0.0) {
		effective_cast_range = targeting::effective_attack_range(caster);
	} else if (cast_range == 0.0) {
		effective_cast_range = -1.0; // sentinel: skip distance check
	} else {
		effective_cast_range = cast_range;
	}
	if (spec.needs_enemy) {
		if (enemy_target == nullptr || !enemy_target->alive) {
			return false;
		}
		if (effective_cast_range >= 0.0 && distance_between(caster, *enemy_target) > effective_cast_range) {
			return false;
		}
	}
	if (spec.needs_single_ally) {
		if (current_ally_target_id == 0) {
			return false;
		}
		const UnitState *ally = targeting::unit_by_id(world, current_ally_target_id);
		if (ally == nullptr || !ally->alive) {
			return false;
		}
		if (effective_cast_range >= 0.0 && distance_between(caster, *ally) > effective_cast_range) {
			return false;
		}
	}
	return true;
}

int64_t snapshot_ally_cast_target_id(int64_t current_ally_target_id, const EffectCastRangeSpec &spec) {
	if (!spec.needs_single_ally) {
		return 0;
	}
	return current_ally_target_id;
}

} // namespace internal
} // namespace combat
} // namespace sim
