extends RefCounted
class_name CombatEffect

const CombatData = preload("res://scripts/combat_data.gd")

enum Kind {
	MULTI,
	DAMAGE,
	HEAL,
	SHIELD,
	STUN,
	PROJECTILE,
	SELF_AOE_DAMAGE,
	SELF_AOE_TAUNT,
	CONSTANT_MULTIPLIER,
	TARGET_HP_THRESHOLD_MULTIPLIER,
	DISTANCE_THRESHOLD_MULTIPLIER,
	SELF_HP_THRESHOLD_MULTIPLIER,
	DAMAGE_BASED_HEAL,
	MANA_RESTORE_ON_HIT,
	DRAIN_TARGET_MANA_ON_HIT,
	MANA_REGEN,
	POST_DAMAGE_MANA_GAIN,
	POST_ATTACK_SPLASH,
	EVERY_N_ATTACKS_STUN,
	DODGE,
	SELF_DAMAGE,
	SELF_SHIELD,
}

var kind: Kind = Kind.MULTI
var reason: String = ""
var damage_multiplier: float = 1.0
var damage_type: String = "physical"
var flat_amount: float = 0.0
var multiplier: float = 1.0
var threshold: float = 0.0
var damage_ratio: float = 0.0
var stun_duration: float = 0.0
var trigger_on_hit: bool = false
var projectile_speed: float = CombatData.DEFAULT_PROJECTILE_SPEED
var projectile_radius: float = CombatData.DEFAULT_PROJECTILE_RADIUS
var splash_radius: float = 0.0
var splash_ratio: float = 0.0
var radius: float = 0.0
var every_n: int = 0
var dodge_chance: float = 0.0
var on_dodge_multiplier: float = 0.0
var on_hit_multiplier: float = 1.0
var max_mana_ratio: float = 0.0
var children: Array = []


func setup_multi(effects: Array, new_reason: String = "") -> Object:
	kind = Kind.MULTI
	reason = new_reason
	children = effects.duplicate()
	return self


func setup_damage(new_multiplier: float, new_damage_type: String = "physical", new_reason: String = "", new_stun_duration: float = 0.0, new_trigger_on_hit: bool = false) -> Object:
	kind = Kind.DAMAGE
	damage_multiplier = new_multiplier
	damage_type = new_damage_type
	reason = new_reason
	stun_duration = new_stun_duration
	trigger_on_hit = new_trigger_on_hit
	return self


func setup_heal(new_flat_amount: float = 0.0, max_hp_ratio: float = 0.0, new_reason: String = "") -> Object:
	kind = Kind.HEAL
	flat_amount = new_flat_amount
	damage_ratio = max_hp_ratio
	reason = new_reason
	return self


func setup_shield(new_flat_amount: float = 0.0, max_hp_ratio: float = 0.0, new_reason: String = "") -> Object:
	kind = Kind.SHIELD
	flat_amount = new_flat_amount
	damage_ratio = max_hp_ratio
	reason = new_reason
	return self


func setup_stun(new_duration: float, new_reason: String = "") -> Object:
	kind = Kind.STUN
	stun_duration = new_duration
	reason = new_reason
	return self


func setup_projectile(new_multiplier: float, new_damage_type: String = "physical", new_reason: String = "", new_projectile_speed: float = CombatData.DEFAULT_PROJECTILE_SPEED, new_projectile_radius: float = CombatData.DEFAULT_PROJECTILE_RADIUS, new_stun_duration: float = 0.0, new_splash_radius: float = 0.0, new_splash_ratio: float = 0.0) -> Object:
	kind = Kind.PROJECTILE
	damage_multiplier = new_multiplier
	damage_type = new_damage_type
	reason = new_reason
	projectile_speed = new_projectile_speed
	projectile_radius = new_projectile_radius
	stun_duration = new_stun_duration
	splash_radius = new_splash_radius
	splash_ratio = new_splash_ratio
	return self


func setup_self_aoe_damage(new_radius: float, new_multiplier: float = 1.0, new_damage_type: String = "physical", new_reason: String = "") -> Object:
	kind = Kind.SELF_AOE_DAMAGE
	radius = new_radius
	damage_multiplier = new_multiplier
	damage_type = new_damage_type
	reason = new_reason
	return self


func setup_self_aoe_taunt(new_radius: float, new_duration: float, new_reason: String = "") -> Object:
	kind = Kind.SELF_AOE_TAUNT
	radius = new_radius
	stun_duration = new_duration
	reason = new_reason
	return self


func setup_constant_multiplier(new_multiplier: float) -> Object:
	kind = Kind.CONSTANT_MULTIPLIER
	multiplier = new_multiplier
	return self


func setup_target_hp_threshold_multiplier(new_threshold: float, new_multiplier: float) -> Object:
	kind = Kind.TARGET_HP_THRESHOLD_MULTIPLIER
	threshold = new_threshold
	multiplier = new_multiplier
	return self


func setup_distance_threshold_multiplier(new_threshold: float, new_multiplier: float) -> Object:
	kind = Kind.DISTANCE_THRESHOLD_MULTIPLIER
	threshold = new_threshold
	multiplier = new_multiplier
	return self


func setup_self_hp_threshold_multiplier(new_threshold: float, new_multiplier: float) -> Object:
	kind = Kind.SELF_HP_THRESHOLD_MULTIPLIER
	threshold = new_threshold
	multiplier = new_multiplier
	return self


func setup_damage_based_heal(new_damage_ratio: float) -> Object:
	kind = Kind.DAMAGE_BASED_HEAL
	damage_ratio = new_damage_ratio
	return self


func setup_mana_restore_on_hit(new_flat_amount: float) -> Object:
	kind = Kind.MANA_RESTORE_ON_HIT
	flat_amount = new_flat_amount
	return self


func setup_drain_target_mana_on_hit(new_flat_amount: float) -> Object:
	kind = Kind.DRAIN_TARGET_MANA_ON_HIT
	flat_amount = new_flat_amount
	return self


func setup_mana_regen(new_flat_amount: float, new_max_mana_ratio: float = 0.0) -> Object:
	kind = Kind.MANA_REGEN
	flat_amount = new_flat_amount
	max_mana_ratio = new_max_mana_ratio
	return self


func setup_post_damage_mana_gain(new_damage_ratio: float) -> Object:
	kind = Kind.POST_DAMAGE_MANA_GAIN
	damage_ratio = new_damage_ratio
	return self


func setup_post_attack_splash(new_radius: float, new_ratio: float, new_damage_type: String = "physical", new_reason: String = "", new_threshold_multiplier: float = 0.0) -> Object:
	kind = Kind.POST_ATTACK_SPLASH
	radius = new_radius
	splash_ratio = new_ratio
	damage_type = new_damage_type
	reason = new_reason
	threshold = new_threshold_multiplier
	return self


func setup_every_n_attacks_stun(new_every_n: int, new_duration: float, new_reason: String = "") -> Object:
	kind = Kind.EVERY_N_ATTACKS_STUN
	every_n = new_every_n
	stun_duration = new_duration
	reason = new_reason
	return self


func setup_dodge(new_chance: float, new_on_dodge_multiplier: float = 0.0, new_on_hit_multiplier: float = 1.0) -> Object:
	kind = Kind.DODGE
	dodge_chance = clampf(new_chance, 0.0, 1.0)
	on_dodge_multiplier = new_on_dodge_multiplier
	on_hit_multiplier = new_on_hit_multiplier
	return self


func setup_self_damage(new_damage_ratio: float, new_reason: String = "") -> Object:
	kind = Kind.SELF_DAMAGE
	damage_ratio = new_damage_ratio
	reason = new_reason
	return self


func setup_self_shield(new_shield_ratio: float, new_reason: String = "") -> Object:
	kind = Kind.SELF_SHIELD
	damage_ratio = new_shield_ratio
	reason = new_reason
	return self


func apply_on_attack(context: Object) -> float:
	match kind:
		Kind.CONSTANT_MULTIPLIER:
			return multiplier
		Kind.TARGET_HP_THRESHOLD_MULTIPLIER:
			if context.target != null and is_instance_valid(context.target):
				var target_hp_ratio := float(context.target.get("hp")) / maxf(float(context.target.get("max_hp")), CombatData.EPSILON)
				if target_hp_ratio < threshold:
					return multiplier
			return 1.0
		Kind.DISTANCE_THRESHOLD_MULTIPLIER:
			if context.target != null and is_instance_valid(context.target):
				var dist := Vector2(context.unit.get("world_pos")).distance_to(Vector2(context.target.get("world_pos")))
				if dist > threshold:
					return multiplier
			return 1.0
		Kind.SELF_HP_THRESHOLD_MULTIPLIER:
			var self_hp_ratio := float(context.unit.get("hp")) / maxf(float(context.unit.get("max_hp")), CombatData.EPSILON)
			if self_hp_ratio > threshold:
				return multiplier
			return 1.0
		Kind.MULTI:
			var total := 1.0
			for child in children:
				total *= child.apply_on_attack(context)
			return total
		_:
			return 1.0


func apply_on_defense(context: Object) -> float:
	match kind:
		Kind.CONSTANT_MULTIPLIER:
			return multiplier
		Kind.DODGE:
			if randf() < dodge_chance:
				return on_dodge_multiplier
			return on_hit_multiplier
		Kind.MULTI:
			var total := 1.0
			for child in children:
				total *= child.apply_on_defense(context)
			return total
		_:
			return 1.0


func apply_on_tick(context: Object) -> void:
	match kind:
		Kind.MANA_REGEN:
			var amount := maxf(0.0, flat_amount + (float(context.unit.get("max_mana")) * max_mana_ratio))
			context.unit.set("mana", minf(float(context.unit.get("max_mana")), float(context.unit.get("mana")) + amount))
		Kind.MULTI:
			for child in children:
				child.apply_on_tick(context)


func apply_post_attack(context: Object) -> void:
	match kind:
		Kind.DAMAGE_BASED_HEAL:
			if context.damage > 0.0:
				context.unit.call("heal", context.damage * damage_ratio, context.world, int(context.unit.get("instance_id")))
		Kind.MANA_RESTORE_ON_HIT:
			context.unit.set("mana", minf(float(context.unit.get("max_mana")), float(context.unit.get("mana")) + flat_amount))
		Kind.DRAIN_TARGET_MANA_ON_HIT:
			if context.target != null and is_instance_valid(context.target):
				context.target.set("mana", maxf(0.0, float(context.target.get("mana")) - flat_amount))
		Kind.POST_ATTACK_SPLASH:
			if context.target == null or not bool(context.target.call("is_alive")):
				return
			if threshold > 0.0 and context.damage <= float(context.unit.get("attack_damage")) * threshold:
				return
			if radius <= 0.0 or splash_ratio <= 0.0:
				return
			var center: Vector2 = context.target.get("world_pos")
			var total_aoe_dmg := 0.0
			for enemy in context.world.call("nearby_units", center, radius, "", int(context.unit.get("instance_id"))):
				if enemy == context.target or String(enemy.get("team")) == String(context.unit.get("team")):
					continue
				var absorbed_loss: Dictionary = enemy.call("take_damage", context.damage * splash_ratio, context.world, damage_type, int(context.unit.get("instance_id")))
				var absorbed := float(absorbed_loss.get("absorbed", 0.0))
				var hp_loss := float(absorbed_loss.get("hp_loss", 0.0))
				var dealt := absorbed + hp_loss
				total_aoe_dmg += dealt
				context.world.call("record_event", {
					"timestamp": float(context.world.get("time")),
					"attacker_id": int(context.unit.get("instance_id")),
					"attacker_name": String(context.unit.get("display_name")),
					"target_id": int(enemy.get("instance_id")),
					"target_name": String(enemy.get("display_name")),
					"damage": dealt,
					"shield_absorbed": absorbed,
					"distance": center.distance_to(Vector2(enemy.get("world_pos"))),
					"target_hp_after": float(enemy.get("hp")),
					"target_shield_after": float(enemy.get("shield")),
					"target_selection_reason": reason,
				})
			context.unit.set("damage_dealt", float(context.unit.get("damage_dealt")) + total_aoe_dmg)
		Kind.EVERY_N_ATTACKS_STUN:
			if every_n <= 0:
				return
			var attack_count := int(context.unit.get("attack_count"))
			attack_count += 1
			context.unit.set("attack_count", attack_count)
			if attack_count >= every_n and context.target != null and bool(context.target.call("is_alive")):
				context.target.call("apply_stun", stun_duration, context.world, int(context.unit.get("instance_id")))
				context.unit.set("attack_count", 0)
		Kind.MULTI:
			for child in children:
				child.apply_post_attack(context)


func apply_post_take_damage(context: Object) -> void:
	match kind:
		Kind.POST_DAMAGE_MANA_GAIN:
			if context.damage > 0.0:
				context.unit.set("mana", minf(float(context.unit.get("max_mana")), float(context.unit.get("mana")) + context.damage * damage_ratio))
		Kind.MULTI:
			for child in children:
				child.apply_post_take_damage(context)


func execute_active(context: Object) -> Dictionary:
	match kind:
		Kind.MULTI:
			var result: Dictionary = {}
			for child in children:
				var child_result: Dictionary = child.execute_active(context)
				result = _merge_results(result, child_result)
			return result
		Kind.DAMAGE:
			if context.target == null or not bool(context.target.call("is_alive")):
				return {}
			var damage := float(context.unit.get("attack_damage")) * damage_multiplier
			var damage_result: Dictionary = context.target.call("take_damage", damage, context.world, damage_type, int(context.unit.get("instance_id")))
			var absorbed := float(damage_result.get("absorbed", 0.0))
			var hp_loss := float(damage_result.get("hp_loss", 0.0))
			var total_dmg := absorbed + hp_loss
			context.damage = total_dmg
			if stun_duration > 0.0:
				context.target.call("apply_stun", stun_duration, context.world, int(context.unit.get("instance_id")))
			if trigger_on_hit:
				context.unit.call("on_attack_hit", context.target, total_dmg, context.world, false)
			return _make_event(context, context.target, total_dmg, absorbed)
		Kind.HEAL:
			var heal_target: Node2D = context.target_ally if context.target_ally != null else context.unit
			if heal_target == null:
				return {}
			var heal_amount := flat_amount
			if damage_ratio > 0.0:
				heal_amount = float(heal_target.get("max_hp")) * damage_ratio
			heal_target.call("heal", heal_amount, context.world, int(context.unit.get("instance_id")))
			return _make_event(context, heal_target, 0.0, 0.0, heal_amount)
		Kind.SHIELD:
			var shield_target: Node2D = context.target_ally if context.target_ally != null else context.unit
			if shield_target == null:
				return {}
			var shield_amount := flat_amount
			if damage_ratio > 0.0:
				shield_amount = float(shield_target.get("max_hp")) * damage_ratio
			shield_target.call("add_shield", shield_amount, context.world, int(context.unit.get("instance_id")))
			return _make_event(context, shield_target, 0.0, 0.0, 0.0, shield_amount)
		Kind.STUN:
			if context.target == null or not bool(context.target.call("is_alive")):
				return {}
			context.target.call("apply_stun", stun_duration, context.world, int(context.unit.get("instance_id")))
			return _make_event(context, context.target, 0.0, 0.0)
		Kind.PROJECTILE:
			return {
				"use_projectile": true,
				"damage": float(context.unit.get("attack_damage")) * damage_multiplier,
				"damage_type": damage_type,
				"reason": reason if not reason.is_empty() else "Ability",
				"projectile_speed": projectile_speed,
				"projectile_radius": projectile_radius,
				"stun_duration": stun_duration,
				"splash_radius": splash_radius,
				"splash_ratio": splash_ratio,
			}
		Kind.SELF_AOE_DAMAGE:
			var center: Vector2 = context.unit.get("world_pos")
			var total_aoe_dmg := 0.0
			var primary_dmg := 0.0
			if radius <= 0.0:
				return {}
			var damage_amount := float(context.unit.get("attack_damage")) * damage_multiplier
			for enemy in context.world.call("nearby_units", center, radius, "", int(context.unit.get("instance_id"))):
				if String(enemy.get("team")) == String(context.unit.get("team")):
					continue
				var absorbed_loss: Dictionary = enemy.call("take_damage", damage_amount, context.world, damage_type, int(context.unit.get("instance_id")))
				var absorbed := float(absorbed_loss.get("absorbed", 0.0))
				var hp_loss := float(absorbed_loss.get("hp_loss", 0.0))
				var dealt := absorbed + hp_loss
				total_aoe_dmg += dealt
				if context.target != null and int(enemy.get("instance_id")) == int(context.target.get("instance_id")):
					primary_dmg = dealt
				else:
					context.world.call("record_event", {
						"timestamp": float(context.world.get("time")),
						"attacker_id": int(context.unit.get("instance_id")),
						"attacker_name": String(context.unit.get("display_name")),
						"target_id": int(enemy.get("instance_id")),
						"target_name": String(enemy.get("display_name")),
						"damage": dealt,
						"shield_absorbed": absorbed,
						"distance": center.distance_to(Vector2(enemy.get("world_pos"))),
						"target_hp_after": float(enemy.get("hp")),
						"target_shield_after": float(enemy.get("shield")),
						"target_selection_reason": reason,
					})
			context.unit.set("damage_dealt", float(context.unit.get("damage_dealt")) + maxf(0.0, total_aoe_dmg - primary_dmg))
			if total_aoe_dmg <= 0.0:
				return {}
			return {"damage": primary_dmg, "total_impact": total_aoe_dmg, "reason": reason}
		Kind.SELF_AOE_TAUNT:
			if radius <= 0.0:
				return {}
			var taunted_count := 0
			var center: Vector2 = context.unit.get("world_pos")
			for enemy in context.world.call("nearby_units", center, radius, "", int(context.unit.get("instance_id"))):
				if String(enemy.get("team")) == String(context.unit.get("team")):
					continue
				enemy.call("apply_taunt", stun_duration, int(context.unit.get("instance_id")))
				taunted_count += 1
			return {"damage": 0.0, "reason": reason, "taunted_count": taunted_count}
		Kind.SELF_DAMAGE:
			var self_damage_amount := float(context.unit.get("max_hp")) * damage_ratio
			context.unit.call("take_damage", self_damage_amount, context.world, "true", int(context.unit.get("instance_id")))
			return _make_event(context, context.unit, 0.0, 0.0)
		Kind.SELF_SHIELD:
			var self_shield_amount := float(context.unit.get("max_hp")) * damage_ratio
			context.unit.call("add_shield", self_shield_amount, context.world, int(context.unit.get("instance_id")))
			return _make_event(context, context.unit, 0.0, 0.0, 0.0, self_shield_amount)
		_:
			return {}


func _make_event(context: Object, event_target: Node2D, damage: float, shield_absorbed: float, healing: float = 0.0, shield_added: float = 0.0) -> Dictionary:
	if event_target == null:
		return {}
	return {
		"timestamp": float(context.world.get("time")),
		"attacker_id": int(context.unit.get("instance_id")),
		"attacker_name": String(context.unit.get("display_name")),
		"target_id": int(event_target.get("instance_id")),
		"target_name": String(event_target.get("display_name")),
		"damage": damage,
		"healing": healing,
		"shield_absorbed": shield_absorbed,
		"shield_added": shield_added,
		"distance": Vector2(context.unit.get("world_pos")).distance_to(Vector2(event_target.get("world_pos"))),
		"target_hp_after": float(event_target.get("hp")),
		"target_shield_after": float(event_target.get("shield")),
		"target_selection_reason": reason,
	}


func _merge_results(base: Dictionary, extra: Dictionary) -> Dictionary:
	if extra.is_empty():
		return base
	if base.is_empty():
		return extra
	var merged := base.duplicate()
	for key in extra.keys():
		if merged.has(key) and typeof(merged[key]) in [TYPE_INT, TYPE_FLOAT] and typeof(extra[key]) in [TYPE_INT, TYPE_FLOAT]:
			merged[key] = float(merged[key]) + float(extra[key])
		else:
			merged[key] = extra[key]
	return merged
