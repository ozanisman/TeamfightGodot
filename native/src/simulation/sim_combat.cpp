#include "sim_combat.hpp"

#include "sim_constants.hpp"
#include "sim_damage.hpp"
#include "sim_stats.hpp"
#include "sim_status.hpp"
#include "sim_targeting.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/string.hpp>

namespace sim {
namespace combat {
namespace {

constexpr size_t EFFECT_BUCKET_ON_ATTACK = 0;
constexpr size_t EFFECT_BUCKET_ON_DEFENSE = 1;
constexpr size_t EFFECT_BUCKET_ON_ALLY_DEFENSE = 2;
constexpr size_t EFFECT_BUCKET_ON_TICK = 3;
constexpr size_t EFFECT_BUCKET_POST_ATTACK = 4;
constexpr size_t EFFECT_BUCKET_POST_TAKE_DAMAGE = 5;
constexpr size_t EFFECT_BUCKET_ON_ABILITY = 6;
constexpr size_t EFFECT_BUCKET_ON_ULTIMATE = 7;
constexpr size_t EFFECT_BUCKET_POST_HEAL = 8;
constexpr size_t EFFECT_BUCKET_ON_TAKEDOWN = 9;

inline const StringName &sn_player() {
	static const StringName s("player");
	return s;
}
inline const StringName &sn_enemy() {
	static const StringName s("enemy");
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
inline const StringName &sn_auto() {
	static const StringName s("auto");
	return s;
}
inline const StringName &sn_physical() {
	static const StringName s("physical");
	return s;
}
inline const StringName &sn_passive() {
	static const StringName s("passive");
	return s;
}
inline const StringName &sn_cast_start() {
	static const StringName s("cast_start");
	return s;
}
inline const StringName &sn_on_attack() {
	static const StringName s("on_attack");
	return s;
}
inline const StringName &sn_on_defense() {
	static const StringName s("on_defense");
	return s;
}
inline const StringName &sn_on_ally_defense() {
	static const StringName s("on_ally_defense");
	return s;
}
inline const StringName &sn_on_tick() {
	static const StringName s("on_tick");
	return s;
}
inline const StringName &sn_post_attack() {
	static const StringName s("post_attack");
	return s;
}
inline const StringName &sn_post_take_damage() {
	static const StringName s("post_take_damage");
	return s;
}
inline const StringName &sn_on_ability() {
	static const StringName s("on_ability");
	return s;
}
inline const StringName &sn_on_ultimate() {
	static const StringName s("on_ultimate");
	return s;
}
inline const StringName &sn_post_heal() {
	static const StringName s("post_heal");
	return s;
}
inline const StringName &sn_on_takedown() {
	static const StringName s("on_takedown");
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
	status::heal_unit(world, source, target, amount, action_kind, allow_overheal, host.viewer_hooks, &host);
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

} // namespace

EffectContext build_context(UnitState &source, UnitState *target, UnitState *target_ally, double damage, const StringName &action_kind) {
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

int effect_bucket_index(const StringName &kind) {
	if (kind == sn_on_attack()) {
		return static_cast<int>(EFFECT_BUCKET_ON_ATTACK);
	}
	if (kind == sn_on_defense()) {
		return static_cast<int>(EFFECT_BUCKET_ON_DEFENSE);
	}
	if (kind == sn_on_ally_defense()) {
		return static_cast<int>(EFFECT_BUCKET_ON_ALLY_DEFENSE);
	}
	if (kind == sn_on_tick()) {
		return static_cast<int>(EFFECT_BUCKET_ON_TICK);
	}
	if (kind == sn_post_attack()) {
		return static_cast<int>(EFFECT_BUCKET_POST_ATTACK);
	}
	if (kind == sn_post_take_damage()) {
		return static_cast<int>(EFFECT_BUCKET_POST_TAKE_DAMAGE);
	}
	if (kind == sn_on_ability()) {
		return static_cast<int>(EFFECT_BUCKET_ON_ABILITY);
	}
	if (kind == sn_on_ultimate()) {
		return static_cast<int>(EFFECT_BUCKET_ON_ULTIMATE);
	}
	if (kind == sn_post_heal()) {
		return static_cast<int>(EFFECT_BUCKET_POST_HEAL);
	}
	if (kind == sn_on_takedown()) {
		return static_cast<int>(EFFECT_BUCKET_ON_TAKEDOWN);
	}
	return -1;
}

const std::vector<EffectRecord> &collect_effects(const SimWorld &world, const UnitState &unit, const StringName &kind) {
	static const std::vector<EffectRecord> EMPTY_EFFECTS;
	const int bucket = effect_bucket_index(kind);
	if (bucket >= 0 && bucket < 10) {
		return uc(world, unit).passive_effects[static_cast<size_t>(bucket)];
	}
	return EMPTY_EFFECTS;
}

bool effect_record_contains_opcode(const EffectRecord &effect, EffectOpcode opcode) {
	const int64_t want = static_cast<int64_t>(opcode);
	if (effect.opcode == want) {
		return true;
	}
	for (const EffectRecord &child : effect.children) {
		if (effect_record_contains_opcode(child, opcode)) {
			return true;
		}
	}
	return false;
}

void run_post_attack_effects(SimWorld &world, SimHostCallbacks &host, UnitState &source, UnitState &target, double damage, const EffectContext &context) {
	if (source.stealth_remaining > 0.0 && source.stealth_break_on_attack) {
		source.stealth_remaining = 0.0;
		source.stealth_break_on_attack = false;
		source.stealth_break_on_ability = false;
		source.stealth_break_on_damage_taken = false;
	}
	const std::vector<EffectRecord> &post_attack_effects = uc(world, source).passive_effects[EFFECT_BUCKET_POST_ATTACK];
	if (post_attack_effects.empty()) {
		return;
	}
	EffectContext effect_context = context;
	effect_context.damage = damage;
	effect_context.action_kind = sn_passive();
	for (const EffectRecord &effect : post_attack_effects) {
		if (host.execute_effect != nullptr) {
			host.execute_effect(host, effect, effect_context);
		}
	}
}

void run_post_heal_effects(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState &target,
		double heal_amount,
		double heal_gained,
		const EffectContext &base_context) {
	const std::vector<EffectRecord> &post_heal_effects = uc(world, target).passive_effects[EFFECT_BUCKET_POST_HEAL];
	if (post_heal_effects.empty()) {
		return;
	}
	EffectContext effect_context = base_context;
	effect_context.heal_amount = heal_amount;
	effect_context.heal_gained = heal_gained;
	effect_context.action_kind = sn_passive();
	effect_context.source = &source;
	effect_context.target = &target;
	for (const EffectRecord &effect : post_heal_effects) {
		if (host.execute_effect != nullptr) {
			host.execute_effect(host, effect, effect_context);
		}
	}
}

void run_on_takedown_effects(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &participant,
		UnitState &victim,
		double damage_dealt,
		bool is_kill,
		const EffectContext &base_context) {
	const std::vector<EffectRecord> &takedown_effects = uc(world, participant).passive_effects[EFFECT_BUCKET_ON_TAKEDOWN];
	if (takedown_effects.empty()) {
		return;
	}
	EffectContext effect_context = base_context;
	effect_context.takedown_target_id = victim.instance_id;
	effect_context.takedown_damage_dealt = damage_dealt;
	effect_context.is_takedown_kill = is_kill;
	effect_context.damage = damage_dealt;
	effect_context.action_kind = sn_passive();
	effect_context.source = &participant;
	effect_context.target = &victim;
	for (const EffectRecord &effect : takedown_effects) {
		if (host.execute_effect != nullptr) {
			host.execute_effect(host, effect, effect_context);
		}
	}
}

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
	const double max_mana = get_effective_max_mana(unit);
	if (max_mana <= 0.0 || unit.mana < max_mana) {
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
		uc(world, unit).abilities += 1;
	} else {
		unit.mana = Math::max(0.0, unit.mana - get_effective_max_mana(unit));
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
		unit.ability_cooldown = get_effective_ability_cd(unit);
	}
	if (effect_uses_projectile(effect) && (target == nullptr || !target->alive || target->stealth_remaining > 0.0)) {
		if (action_kind == sn_ability()) {
			unit.ability_cooldown = 0.0;
		} else if (action_kind == sn_ultimate()) {
			const double effective_max_mana = get_effective_max_mana(unit);
			unit.mana = Math::min(effective_max_mana, unit.mana + effective_max_mana);
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
	if (unit.disarm_remaining > 0.0) {
		return;
	}
	if (target.stealth_remaining > 0.0) {
		return;
	}
	uc(world, unit).auto_attacks += 1;
	unit.attack_count += 1;
	double damage = get_effective_attack_damage(unit);
	damage = damage::apply_attack_modifiers(world, host, unit, target, distance, damage);
	if (get_effective_attack_range(unit) > RANGED_THRESHOLD) {
		ProjectileState projectile;
		projectile.source_id = unit.instance_id;
		projectile.target_id = target.instance_id;
		projectile.damage = damage;
		projectile.damage_type = sn_physical();
		projectile.stun_duration = DEFAULT_PROJECTILE_STUN_DURATION;
		projectile.radius = get_effective_projectile_radius(unit);
		projectile.speed = Math::max(0.0001, get_effective_projectile_speed(unit));
		projectile.pos_x = unit.pos_x;
		projectile.pos_y = unit.pos_y;
		projectile.action_kind = sn_auto();
		projectile.reason = String("Auto Attack");
		projectiles.push_back(projectile);
		emit_trace(host, StringName("projectile"), unit.instance_id, target.instance_id, damage);
	} else {
		EffectContext context = build_context(unit, &target, nullptr, damage, sn_auto());
		const double dealt = damage::apply_damage(world, host, unit, target, damage, sn_physical(), sn_auto(), context);
		emit_trace(host, StringName("auto_melee"), unit.instance_id, target.instance_id, dealt);
		run_post_attack_effects(world, host, unit, target, dealt, context);
		const double life_steal = get_effective_life_steal(unit);
		if (life_steal > 0.0) {
			const double old_hp = unit.hp;
			const double heal_amount = dealt * life_steal;
			heal_with_hooks(world, host, unit, unit, heal_amount, sn_auto(), false);
			const double heal_gained = unit.hp - old_hp;
			run_post_heal_effects(world, host, unit, unit, heal_amount, heal_gained, context);
		}
	}
	const double mana_gain = get_effective_mana_per_attack(unit);
	if (mana_gain > 0.0) {
		const double max_mana = get_effective_max_mana(unit);
		unit.mana = Math::min(max_mana, unit.mana + mana_gain);
	}
	const double attack_speed = Math::max(0.0001, get_effective_attack_speed(unit));
	unit.attack_cooldown = 1.0 / attack_speed;
	unit.attack_period = unit.attack_cooldown;
}

void resolve_projectile(SimWorld &world, SimHostCallbacks &host, const CombatHostHooks &hooks, const ProjectileState &projectile) {
	UnitState *source = unit_by_id(world, projectile.source_id);
	UnitState *target = unit_by_id(world, projectile.target_id);
	if (source == nullptr || target == nullptr || !target->alive) {
		return;
	}
	const double damage = projectile.damage;
	const StringName damage_type = projectile.damage_type;
	const StringName action_kind = projectile.action_kind;
	EffectContext context = build_context(*source, target, nullptr, damage, action_kind);
	const double dealt = damage::apply_damage(world, host, *source, *target, damage, damage_type, action_kind, context);
	run_post_attack_effects(world, host, *source, *target, dealt, context);
	if (projectile.action_kind == sn_auto()) {
		const double life_steal = get_effective_life_steal(*source);
		if (life_steal > 0.0) {
			const double old_hp = source->hp;
			const double heal_amount = dealt * life_steal;
			heal_with_hooks(world, host, *source, *source, heal_amount, sn_auto(), false);
			const double heal_gained = source->hp - old_hp;
			run_post_heal_effects(world, host, *source, *source, heal_amount, heal_gained, context);
		}
	}
	if (projectile.stun_duration > 0.0 && target->alive) {
		status::apply_stun(world, *source, *target, projectile.stun_duration);
	}
}

void update_projectiles(
		SimWorld &world,
		SimHostCallbacks &host,
		const CombatHostHooks &hooks,
		ProjectileBuffers &buffers,
		const ProjectileMatchState &match) {
	using std::swap;
	buffers.active_projectiles.clear();
	swap(buffers.active_projectiles, buffers.projectiles);
	buffers.scratch_projectiles.clear();
	for (const ProjectileState &data : buffers.active_projectiles) {
		UnitState *target = unit_by_id(world, data.target_id);
		if (target == nullptr || !target->alive) {
			continue;
		}
		const double dist = distance_between_coords(data.pos_x, data.pos_y, target->pos_x, target->pos_y);
		const double move_dist = data.speed * world.tick_rate;
		if (dist <= move_dist + data.radius) {
			const ProjectileState hit = data;
			resolve_projectile(world, host, hooks, hit);
			if (match.sudden_death_ticks > 0 && match.player_kills != match.enemy_kills) {
				buffers.scratch_projectiles.clear();
				break;
			}
			continue;
		}
		if (dist > EPSILON) {
			ProjectileState next = data;
			const double inv = 1.0 / dist;
			next.pos_x = data.pos_x + (target->pos_x - data.pos_x) * inv * move_dist;
			next.pos_y = data.pos_y + (target->pos_y - data.pos_y) * inv * move_dist;
			buffers.scratch_projectiles.push_back(next);
		} else {
			buffers.scratch_projectiles.push_back(data);
		}
	}
	if (!buffers.projectiles.empty()) {
		buffers.scratch_projectiles.insert(
				buffers.scratch_projectiles.end(),
				buffers.projectiles.begin(),
				buffers.projectiles.end());
	}
	swap(buffers.projectiles, buffers.scratch_projectiles);
	buffers.scratch_projectiles.clear();
}

} // namespace combat
} // namespace sim
