extends RefCounted
class_name CombatEffect

const CombatData = preload("res://scripts/combat_data.gd")
const CombatUnitState = preload("res://scripts/combat_unit_state.gd")
const CombatEffectResult = preload("res://scripts/combat_effect_result.gd")

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
	var unit: Object = context.unit
	var target: Object = context.target
	var unit_state: CombatUnitState = _unit_state_ref(unit)
	var target_state: CombatUnitState = _unit_state_ref(target)
	var combat_scale: float = unit_state.combat_scale if unit_state != null else _unit_combat_scale(unit)
	var unit_pos: Vector2 = unit_state.global_position if unit_state != null else _unit_position(unit)
	var target_pos: Vector2 = target_state.global_position if target_state != null else _unit_position(target)
	match kind:
		Kind.CONSTANT_MULTIPLIER:
			return multiplier
		Kind.TARGET_HP_THRESHOLD_MULTIPLIER:
			if target != null and is_instance_valid(target):
				var target_hp_ratio: float = _unit_hp(target) / maxf(_unit_max_hp(target), CombatData.EPSILON)
				if target_state != null:
					target_hp_ratio = target_state.hp / maxf(target_state.max_hp, CombatData.EPSILON)
				if target_hp_ratio < threshold:
					return multiplier
			return 1.0
		Kind.DISTANCE_THRESHOLD_MULTIPLIER:
			if target != null and is_instance_valid(target):
				var dist: float = unit_pos.distance_to(target_pos)
				if dist > threshold * combat_scale:
					return multiplier
			return 1.0
		Kind.SELF_HP_THRESHOLD_MULTIPLIER:
			var self_hp_ratio: float = _unit_hp(unit) / maxf(_unit_max_hp(unit), CombatData.EPSILON)
			if unit_state != null:
				self_hp_ratio = unit_state.hp / maxf(unit_state.max_hp, CombatData.EPSILON)
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
	var world: Object = context.world
	match kind:
		Kind.CONSTANT_MULTIPLIER:
			return multiplier
		Kind.DODGE:
			var roll: float = _world_random_float(world)
			if roll < dodge_chance:
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
	var unit: Object = context.unit
	var unit_state: CombatUnitState = _unit_state_ref(unit)
	match kind:
		Kind.MANA_REGEN:
			var max_mana: float = _unit_max_mana(unit)
			var current_mana: float = _unit_mana(unit)
			if unit_state != null:
				max_mana = unit_state.max_mana
				current_mana = unit_state.mana
			var amount: float = maxf(0.0, flat_amount + (max_mana * max_mana_ratio))
			_set_unit_mana(unit, minf(max_mana, current_mana + amount))
		Kind.MULTI:
			for child in children:
				child.apply_on_tick(context)


func apply_post_attack(context: Object) -> void:
	var unit: Object = context.unit
	var target: Object = context.target
	var world: Object = context.world
	var unit_id: int = _unit_instance_id(unit)
	var unit_state: CombatUnitState = _unit_state_ref(unit)
	var target_state: CombatUnitState = _unit_state_ref(target)
	var combat_scale: float = unit_state.combat_scale if unit_state != null else _unit_combat_scale(unit)
	match kind:
		Kind.DAMAGE_BASED_HEAL:
			if context.damage > 0.0:
				if unit_state != null:
					unit_state.heal(context.damage * damage_ratio, world, unit_id)
				else:
					_unit_heal(unit, context.damage * damage_ratio, world, unit_id)
		Kind.MANA_RESTORE_ON_HIT:
			var max_mana: float = _unit_max_mana(unit)
			var current_mana: float = _unit_mana(unit)
			if unit_state != null:
				max_mana = unit_state.max_mana
				current_mana = unit_state.mana
			_set_unit_mana(unit, minf(max_mana, current_mana + flat_amount))
		Kind.DRAIN_TARGET_MANA_ON_HIT:
			if target != null and is_instance_valid(target):
				if target_state != null:
					_set_unit_mana(target, maxf(0.0, target_state.mana - flat_amount))
				else:
					_set_unit_mana(target, maxf(0.0, _unit_mana(target) - flat_amount))
		Kind.POST_ATTACK_SPLASH:
			if target == null or not _unit_is_alive(target):
				return
			if threshold > 0.0 and context.damage <= _unit_attack_damage(unit) * threshold:
				return
			var combat_radius := radius * combat_scale
			if combat_radius <= 0.0 or splash_ratio <= 0.0:
				return
			var center: Vector2 = target_state.global_position if target_state != null else _unit_position(target)
			var total_aoe_dmg := 0.0
			for enemy in _world_nearby_units(world, center, combat_radius, "", unit_id):
				if enemy == target or _unit_team(enemy) == _unit_team(unit):
					continue
				var absorbed_loss: Dictionary = _unit_take_damage(enemy, context.damage * splash_ratio, world, damage_type, unit_id)
				var absorbed: float = float(absorbed_loss.get("absorbed", 0.0))
				var hp_loss: float = float(absorbed_loss.get("hp_loss", 0.0))
				var dealt: float = absorbed + hp_loss
				total_aoe_dmg += dealt
				_world_record_event(world, {
					"timestamp": _world_time(world),
					"attacker_id": unit_id,
					"attacker_name": _unit_display_name(unit),
					"target_id": _unit_instance_id(enemy),
					"target_name": _unit_display_name(enemy),
					"damage": dealt,
					"shield_absorbed": absorbed,
					"distance": center.distance_to(_unit_position(enemy)),
					"target_hp_after": _unit_hp(enemy),
					"target_shield_after": _unit_shield(enemy),
					"target_selection_reason": reason,
				})
			_add_unit_damage_dealt(unit, total_aoe_dmg)
		Kind.EVERY_N_ATTACKS_STUN:
			if every_n <= 0:
				return
			var attack_count: int = _unit_attack_count(unit)
			if unit_state != null:
				attack_count = unit_state.attack_count
			attack_count += 1
			_set_unit_attack_count(unit, attack_count)
			if attack_count >= every_n and target != null and _unit_is_alive(target):
				_unit_apply_stun(target, stun_duration, world, unit_id)
				_set_unit_attack_count(unit, 0)
		Kind.MULTI:
			for child in children:
				child.apply_post_attack(context)


func apply_post_take_damage(context: Object) -> void:
	var unit: Object = context.unit
	var unit_state: CombatUnitState = _unit_state_ref(unit)
	match kind:
		Kind.POST_DAMAGE_MANA_GAIN:
			if context.damage > 0.0:
				var max_mana: float = _unit_max_mana(unit)
				var current_mana: float = _unit_mana(unit)
				if unit_state != null:
					max_mana = unit_state.max_mana
					current_mana = unit_state.mana
				_set_unit_mana(unit, minf(max_mana, current_mana + context.damage * damage_ratio))
		Kind.MULTI:
			for child in children:
				child.apply_post_take_damage(context)


func execute_active(context: Object) -> CombatEffectResult:
	var unit: Object = context.unit
	var target: Object = context.target
	var target_ally: Object = context.target_ally
	var world: Object = context.world
	var unit_id: int = _unit_instance_id(unit)
	var unit_state: CombatUnitState = _unit_state_ref(unit)
	var target_state: CombatUnitState = _unit_state_ref(target)
	var target_ally_state: CombatUnitState = _unit_state_ref(target_ally)
	var combat_scale: float = unit_state.combat_scale if unit_state != null else _unit_combat_scale(unit)
	match kind:
		Kind.MULTI:
			var result: CombatEffectResult = CombatEffectResult.new()
			for child in children:
				result.merge(child.execute_active(context))
			return result
		Kind.DAMAGE:
			if target == null or not _unit_is_alive(target):
				return CombatEffectResult.new()
			var result_damage: CombatEffectResult = CombatEffectResult.new()
			var damage: float = _unit_attack_damage(unit) * damage_multiplier
			if unit_state != null:
				damage = unit_state.attack_damage * damage_multiplier
			var damage_result: Dictionary = _unit_take_damage(target, damage, world, damage_type, unit_id)
			var absorbed: float = float(damage_result.get("absorbed", 0.0))
			var hp_loss: float = float(damage_result.get("hp_loss", 0.0))
			var total_dmg: float = absorbed + hp_loss
			context.damage = total_dmg
			result_damage.damage = total_dmg
			result_damage.damage_type = damage_type
			result_damage.reason = reason
			result_damage.shield_absorbed = absorbed
			result_damage.emit_event = true
			result_damage.event_target = target
			if stun_duration > 0.0:
				_unit_apply_stun(target, stun_duration, world, unit_id)
			if trigger_on_hit:
				_unit_on_attack_hit(unit, target, total_dmg, world, false)
			return result_damage
		Kind.HEAL:
			var heal_target: Object = target_ally if target_ally != null else unit
			var heal_target_state: CombatUnitState = target_ally_state if target_ally_state != null else unit_state
			if heal_target == null:
				return CombatEffectResult.new()
			var heal_amount: float = flat_amount
			if damage_ratio > 0.0:
				heal_amount = _unit_max_hp(heal_target) * damage_ratio
				if heal_target_state != null:
					heal_amount = heal_target_state.max_hp * damage_ratio
			if heal_target_state != null:
				heal_target_state.heal(heal_amount, world, unit_id)
			else:
				_unit_heal(heal_target, heal_amount, world, unit_id)
			var heal_result: CombatEffectResult = CombatEffectResult.new()
			heal_result.healing = heal_amount
			heal_result.reason = reason
			heal_result.emit_event = true
			heal_result.event_target = heal_target
			return heal_result
		Kind.SHIELD:
			var shield_target: Object = target_ally if target_ally != null else unit
			var shield_target_state: CombatUnitState = target_ally_state if target_ally_state != null else unit_state
			if shield_target == null:
				return CombatEffectResult.new()
			var shield_amount: float = flat_amount
			if damage_ratio > 0.0:
				shield_amount = _unit_max_hp(shield_target) * damage_ratio
				if shield_target_state != null:
					shield_amount = shield_target_state.max_hp * damage_ratio
			if shield_target_state != null:
				shield_target_state.add_shield(shield_amount, world, unit_id)
			else:
				_unit_add_shield(shield_target, shield_amount, world, unit_id)
			var shield_result: CombatEffectResult = CombatEffectResult.new()
			shield_result.shield_added = shield_amount
			shield_result.reason = reason
			shield_result.emit_event = true
			shield_result.event_target = shield_target
			return shield_result
		Kind.STUN:
			if target == null or not _unit_is_alive(target):
				return CombatEffectResult.new()
			_unit_apply_stun(target, stun_duration, world, unit_id)
			var stun_result: CombatEffectResult = CombatEffectResult.new()
			stun_result.reason = reason
			stun_result.emit_event = true
			stun_result.event_target = target
			return stun_result
		Kind.PROJECTILE:
			var projectile_result: CombatEffectResult = CombatEffectResult.new()
			projectile_result.use_projectile = true
			projectile_result.damage = _unit_attack_damage(unit) * damage_multiplier
			projectile_result.damage_type = damage_type
			projectile_result.reason = reason if not reason.is_empty() else "Ability"
			projectile_result.projectile_speed = projectile_speed * combat_scale
			projectile_result.projectile_radius = projectile_radius * combat_scale
			projectile_result.stun_duration = stun_duration
			projectile_result.splash_radius = splash_radius * combat_scale
			projectile_result.splash_ratio = splash_ratio
			return projectile_result
		Kind.SELF_AOE_DAMAGE:
			var center: Vector2 = unit_state.global_position if unit_state != null else _unit_position(unit)
			var total_aoe_dmg := 0.0
			var primary_dmg := 0.0
			var combat_radius := radius * combat_scale
			if combat_radius <= 0.0:
				return CombatEffectResult.new()
			var damage_amount: float = _unit_attack_damage(unit) * damage_multiplier
			if unit_state != null:
				damage_amount = unit_state.attack_damage * damage_multiplier
			for enemy in _world_nearby_units(world, center, combat_radius, "", unit_id):
				if _unit_team(enemy) == _unit_team(unit):
					continue
				var absorbed_loss: Dictionary = _unit_take_damage(enemy, damage_amount, world, damage_type, unit_id)
				var absorbed: float = float(absorbed_loss.get("absorbed", 0.0))
				var hp_loss: float = float(absorbed_loss.get("hp_loss", 0.0))
				var dealt: float = absorbed + hp_loss
				total_aoe_dmg += dealt
				if target != null and _unit_instance_id(enemy) == _unit_instance_id(target):
					primary_dmg = dealt
				else:
					_world_record_event(world, {
						"timestamp": _world_time(world),
						"attacker_id": unit_id,
						"attacker_name": _unit_display_name(unit),
						"target_id": _unit_instance_id(enemy),
						"target_name": _unit_display_name(enemy),
						"damage": dealt,
						"shield_absorbed": absorbed,
						"distance": center.distance_to(_unit_position(enemy)),
						"target_hp_after": _unit_hp(enemy),
						"target_shield_after": _unit_shield(enemy),
						"target_selection_reason": reason,
					})
			_add_unit_damage_dealt(unit, maxf(0.0, total_aoe_dmg - primary_dmg))
			if total_aoe_dmg <= 0.0:
				return CombatEffectResult.new()
			var aoe_result: CombatEffectResult = CombatEffectResult.new()
			aoe_result.damage = primary_dmg
			aoe_result.total_impact = total_aoe_dmg
			aoe_result.reason = reason
			return aoe_result
		Kind.SELF_AOE_TAUNT:
			var combat_radius := radius * combat_scale
			if combat_radius <= 0.0:
				return CombatEffectResult.new()
			var taunted_count: int = 0
			var center: Vector2 = _unit_position(unit)
			for enemy in _world_nearby_units(world, center, combat_radius, "", unit_id):
				if _unit_team(enemy) == _unit_team(unit):
					continue
				_unit_apply_taunt(enemy, stun_duration, unit_id)
				taunted_count += 1
			var taunt_result: CombatEffectResult = CombatEffectResult.new()
			taunt_result.reason = reason
			taunt_result.taunted_count = taunted_count
			taunt_result.emit_event = true
			return taunt_result
		Kind.SELF_DAMAGE:
			var self_damage_amount: float = _unit_max_hp(unit) * damage_ratio
			if unit_state != null:
				self_damage_amount = unit_state.max_hp * damage_ratio
				unit_state.take_damage(self_damage_amount, world, "true", unit_id)
			else:
				_unit_take_damage(unit, self_damage_amount, world, "true", unit_id)
			var self_damage_result: CombatEffectResult = CombatEffectResult.new()
			self_damage_result.reason = reason
			self_damage_result.emit_event = true
			self_damage_result.event_target = unit
			return self_damage_result
		Kind.SELF_SHIELD:
			var self_shield_amount: float = _unit_max_hp(unit) * damage_ratio
			if unit_state != null:
				self_shield_amount = unit_state.max_hp * damage_ratio
				unit_state.add_shield(self_shield_amount, world, unit_id)
			else:
				_unit_add_shield(unit, self_shield_amount, world, unit_id)
			var self_shield_result: CombatEffectResult = CombatEffectResult.new()
			self_shield_result.shield_added = self_shield_amount
			self_shield_result.reason = reason
			self_shield_result.emit_event = true
			self_shield_result.event_target = unit
			return self_shield_result
		_:
			return CombatEffectResult.new()


func _unit_state_ref(unit: Object) -> CombatUnitState:
	if unit != null and unit.get_script() == CombatUnitState:
		return unit
	return null


func _unit_state(unit: Object) -> Object:
	if unit != null and unit.get_script() == CombatUnitState:
		return unit
	return null


func _unit_instance_id(unit: Object) -> int:
	var state := _unit_state(unit)
	if state != null:
		return state.instance_id
	return int(unit.get("instance_id"))


func _unit_display_name(unit: Object) -> String:
	var state := _unit_state(unit)
	if state != null:
		return state.display_name
	return String(unit.get("display_name"))


func _unit_team(unit: Object) -> String:
	var state := _unit_state(unit)
	if state != null:
		return state.team
	return String(unit.get("team"))


func _unit_position(unit: Object) -> Vector2:
	var state := _unit_state(unit)
	if state != null:
		return state.global_position
	return unit.global_position


func _unit_is_alive(unit: Object) -> bool:
	var state := _unit_state(unit)
	if state != null:
		return state.is_alive()
	return bool(unit.call("is_alive"))


func _unit_hp(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.hp
	return float(unit.get("hp"))


func _unit_max_hp(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.max_hp
	return float(unit.get("max_hp"))


func _unit_mana(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.mana
	return float(unit.get("mana"))


func _unit_max_mana(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.max_mana
	return float(unit.get("max_mana"))


func _unit_attack_damage(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.attack_damage
	return float(unit.get("attack_damage"))


func _unit_combat_scale(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.combat_scale
	return float(unit.get("combat_scale"))


func _unit_attack_count(unit: Object) -> int:
	var state := _unit_state(unit)
	if state != null:
		return state.attack_count
	return int(unit.get("attack_count"))


func _set_unit_mana(unit: Object, value: float) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.set_mana(value)
	elif unit != null and unit.has_method("set_mana"):
		unit.call("set_mana", value)
	else:
		unit.set("mana", value)


func _set_unit_attack_count(unit: Object, value: int) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.set_attack_count(value)
	elif unit != null and unit.has_method("set_attack_count"):
		unit.call("set_attack_count", value)
	else:
		unit.set("attack_count", value)


func _add_unit_damage_dealt(unit: Object, amount: float) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.add_damage_dealt(amount)
	elif unit != null and unit.has_method("add_damage_dealt"):
		unit.call("add_damage_dealt", amount)
	else:
		unit.set("damage_dealt", float(unit.get("damage_dealt")) + amount)


func _unit_shield(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.shield
	return float(unit.get("shield"))


func _unit_take_damage(unit: Object, amount: float, world: Object, damage_type: String, source_id: int) -> Dictionary:
	if unit == null:
		return {}
	if unit.get_script() == CombatUnitState:
		return unit.take_damage(amount, world, damage_type, source_id)
	return unit.call("take_damage", amount, world, damage_type, source_id)


func _unit_heal(unit: Object, amount: float, world: Object, source_id: int) -> void:
	if unit == null:
		return
	if unit.get_script() == CombatUnitState:
		unit.heal(amount, world, source_id)
	else:
		unit.call("heal", amount, world, source_id)


func _unit_add_shield(unit: Object, amount: float, world: Object, source_id: int) -> void:
	if unit == null:
		return
	if unit.get_script() == CombatUnitState:
		unit.add_shield(amount, world, source_id)
	else:
		unit.call("add_shield", amount, world, source_id)


func _unit_apply_stun(unit: Object, duration: float, world: Object, source_id: int) -> void:
	if unit == null:
		return
	if unit.get_script() == CombatUnitState:
		unit.apply_stun(duration, world, source_id)
	else:
		unit.call("apply_stun", duration, world, source_id)


func _unit_apply_taunt(unit: Object, duration: float, source_id: int) -> void:
	if unit == null:
		return
	if unit.get_script() == CombatUnitState:
		unit.apply_taunt(duration, source_id)
	else:
		unit.call("apply_taunt", duration, source_id)


func _unit_on_attack_hit(unit: Object, target: Object, damage: float, world: Object, is_auto: bool) -> void:
	if unit == null:
		return
	if unit.get_script() == CombatUnitState:
		unit.on_attack_hit(target, damage, world, is_auto)
	else:
		unit.call("on_attack_hit", target, damage, world, is_auto)


func _world_time(world: Object) -> float:
	if world == null:
		return 0.0
	return float(world.get("time"))


func _world_random_float(world: Object) -> float:
	if world != null and world.has_method("random_float"):
		return float(world.call("random_float"))
	return randf()


func _world_nearby_units(world: Object, center: Vector2, radius: float, team: String = "", exclude_id: int = -1) -> Array:
	if world != null and world.has_method("nearby_units"):
		return world.call("nearby_units", center, radius, team, exclude_id)
	return []


func _world_record_event(world: Object, event: Dictionary) -> void:
	if world == null:
		return
	if world.has_method("record_event"):
		world.call("record_event", event)
