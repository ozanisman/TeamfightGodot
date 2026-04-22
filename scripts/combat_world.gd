extends RefCounted
class_name CombatWorld

const CombatData = preload("res://scripts/combat_data.gd")
const CombatRegistryScript = preload("res://scripts/combat_registry.gd")
const TargetingSystemScript = preload("res://scripts/targeting_system.gd")

signal combat_event_recorded(event: Dictionary)

var tick_rate: float = CombatData.DEFAULT_WORLD_TICK_RATE
var time: float = 0.0
var tick: int = 0
var units: Array[Node2D] = []
var projectiles: Array[Dictionary] = []
var targeting_system = TargetingSystemScript.new()
var combat_registry = CombatRegistryScript.new()
var player_kills: int = 0
var enemy_kills: int = 0
var record_events: bool = true
var _events_current_tick: Array[Dictionary] = []
var _next_instance_id: int = 1


func clear() -> void:
	units.clear()
	projectiles.clear()
	_events_current_tick.clear()
	time = 0.0
	tick = 0
	player_kills = 0
	enemy_kills = 0
	_next_instance_id = 1


func set_units(new_units: Array[Node2D]) -> void:
	units = new_units.duplicate()
	for unit in units:
		if is_instance_valid(unit):
			unit.set("instance_id", _next_instance_id)
			_next_instance_id += 1


func get_targeting_system() -> Object:
	return targeting_system


func get_combat_registry() -> Object:
	return combat_registry


func capture_spawn_positions() -> void:
	for unit in units:
		if is_instance_valid(unit):
			unit.call("set_spawn_position", unit.global_position)


func get_unit_by_id(instance_id: int) -> Node2D:
	for unit in units:
		if is_instance_valid(unit) and int(unit.get("instance_id")) == instance_id:
			return unit
	return null


func get_alive_units(team: String = "") -> Array[Node2D]:
	var result: Array[Node2D] = []
	for unit in units:
		if not is_instance_valid(unit) or not bool(unit.call("is_alive")):
			continue
		if team != "" and String(unit.get("team")) != team:
			continue
		result.append(unit)
	return result


func get_allies_for(unit: Node2D) -> Array[Node2D]:
	return get_alive_units(String(unit.get("team")))


func get_enemies_for(unit: Node2D) -> Array[Node2D]:
	var result: Array[Node2D] = []
	for other in units:
		if not is_instance_valid(other) or not bool(other.call("is_alive")):
			continue
		if other == unit:
			continue
		if String(other.get("team")) == String(unit.get("team")):
			continue
		result.append(other)
	return result


func nearby_units(pos: Vector2, radius: float, team: String = "", exclude_id: int = -1) -> Array[Node2D]:
	if radius <= 0.0:
		return []
	var radius_sq := radius * radius
	var result: Array[Node2D] = []
	for unit in units:
		if not is_instance_valid(unit) or not bool(unit.call("is_alive")):
			continue
		if exclude_id >= 0 and int(unit.get("instance_id")) == exclude_id:
			continue
		if team != "" and String(unit.get("team")) != team:
			continue
		if unit.global_position.distance_squared_to(pos) <= radius_sq:
			result.append(unit)
	return result


func notify_unit_moved(_unit: Node2D, _old_pos: Vector2) -> void:
	pass


func adjust_target_pressure(old_target: Node2D, new_target: Node2D) -> void:
	if old_target == new_target:
		return
	if old_target != null and is_instance_valid(old_target):
		old_target.set("incoming_target_count", max(0, int(old_target.get("incoming_target_count")) - 1))
	if new_target != null and is_instance_valid(new_target) and bool(new_target.call("is_alive")):
		new_target.set("incoming_target_count", int(new_target.get("incoming_target_count")) + 1)


func refresh_target_pressure(source_units: Array[Node2D] = []) -> void:
	var alive_units := source_units if not source_units.is_empty() else get_alive_units()
	for unit in alive_units:
		unit.set("incoming_target_count", 0)
	for unit in alive_units:
		var target: Node2D = unit.get("current_target")
		if target != null and is_instance_valid(target) and bool(target.call("is_alive")):
			target.set("incoming_target_count", int(target.get("incoming_target_count")) + 1)


func spawn_projectile(
	attacker: Node2D,
	target: Node2D,
	damage: float,
	damage_type: String,
	reason: String,
	speed: float = CombatData.DEFAULT_PROJECTILE_SPEED,
	radius: float = CombatData.DEFAULT_PROJECTILE_RADIUS,
	stun_duration: float = CombatData.DEFAULT_PROJECTILE_STUN_DURATION,
	is_ability: bool = false,
	is_ultimate: bool = false,
	splash_radius: float = 0.0,
	splash_ratio: float = 0.0
) -> void:
	projectiles.append({
		"attacker_id": int(attacker.get("instance_id")),
		"target_id": int(target.get("instance_id")),
		"team": String(attacker.get("team")),
		"pos": attacker.global_position,
		"speed": speed,
		"radius": radius,
		"damage": damage,
		"damage_type": damage_type,
		"reason": reason,
		"stun_duration": stun_duration,
		"is_ability": is_ability,
		"is_ultimate": is_ultimate,
		"splash_radius": splash_radius,
		"splash_ratio": splash_ratio,
	})


func record_event(event: Dictionary) -> void:
	if not record_events:
		return
	_events_current_tick.append(event)
	combat_event_recorded.emit(event)


func step(dt: float) -> void:
	tick += 1
	time = float(tick) * tick_rate
	_events_current_tick.clear()

	var alive_at_start: Dictionary = {}
	for unit in units:
		if is_instance_valid(unit):
			alive_at_start[int(unit.get("instance_id"))] = bool(unit.call("is_alive"))

	var projectiles_to_remove: Array[int] = []
	for i in range(projectiles.size()):
		var projectile: Dictionary = projectiles[i]
		var target := get_unit_by_id(int(projectile.get("target_id", -1)))
		if target == null or not bool(target.call("is_alive")):
			projectiles_to_remove.append(i)
			continue

		var target_pos: Vector2 = target.global_position
		var proj_pos: Vector2 = projectile.get("pos")
		var direction: Vector2 = target_pos - proj_pos
		var dist: float = direction.length()
		var move_dist: float = float(projectile.get("speed", CombatData.DEFAULT_PROJECTILE_SPEED)) * dt
		if dist <= move_dist + float(projectile.get("radius", CombatData.DEFAULT_PROJECTILE_RADIUS)):
			_resolve_projectile_hit(projectile, target)
			projectiles_to_remove.append(i)
		elif dist > CombatData.EPSILON:
			projectile["pos"] = proj_pos + direction * (move_dist / dist)
			projectiles[i] = projectile

	for i in range(projectiles_to_remove.size() - 1, -1, -1):
		projectiles.remove_at(projectiles_to_remove[i])

	refresh_target_pressure(get_alive_units())

	for unit in units:
		if is_instance_valid(unit):
			unit.call("update", dt, self)

	refresh_target_pressure(get_alive_units())

	for unit in units:
		if not is_instance_valid(unit):
			continue
		var uid := int(unit.get("instance_id"))
		if bool(alive_at_start.get(uid, false)) and not bool(unit.call("is_alive")):
			unit.set("deaths", int(unit.get("deaths")) + 1)
			if String(unit.get("team")) == "player":
				enemy_kills += 1
			else:
				player_kills += 1

			var killer_id := -1
			var last_time := -INF
			var recent_damage_sources: Dictionary = unit.get("recent_damage_sources")
			for attacker_id in recent_damage_sources.keys():
				var timestamp := float(recent_damage_sources[attacker_id])
				if timestamp > last_time:
					last_time = timestamp
					killer_id = int(attacker_id)

			var assistants: Dictionary = {}
			if killer_id >= 0:
				var killer := get_unit_by_id(killer_id)
				if killer != null:
					killer.set("kills", int(killer.get("kills")) + 1)
					var killer_benefactors: Dictionary = killer.get("recent_benefactors")
					for benefactor_id in killer_benefactors.keys():
						var timestamp := float(killer_benefactors[benefactor_id])
						if time - timestamp <= CombatData.ASSIST_WINDOW:
							assistants[int(benefactor_id)] = true
				for attacker_id in recent_damage_sources.keys():
					var timestamp := float(recent_damage_sources[attacker_id])
					if attacker_id != killer_id and time - timestamp <= CombatData.ASSIST_WINDOW:
						assistants[int(attacker_id)] = true

			for ally in units:
				if is_instance_valid(ally) and assistants.has(int(ally.get("instance_id"))):
					ally.set("assists", int(ally.get("assists")) + 1)


func winner() -> String:
	if time < CombatData.MATCH_DURATION:
		return ""
	if player_kills > enemy_kills:
		return "player"
	if enemy_kills > player_kills:
		return "enemy"
	return "draw"


func get_match_result() -> Dictionary:
	var result := winner()
	if result == "":
		return {}
	return {
		"winner": result,
		"player_kills": player_kills,
		"enemy_kills": enemy_kills,
		"time": time,
	}


func _resolve_projectile_hit(projectile: Dictionary, target: Node2D) -> void:
	var attacker := get_unit_by_id(int(projectile.get("attacker_id", -1)))
	if attacker == null:
		return

	var absorbed_loss: Dictionary = target.call("take_damage", float(projectile.get("damage", 0.0)), self, String(projectile.get("damage_type", "true")), int(projectile.get("attacker_id", -1)))
	var absorbed: float = float(absorbed_loss.get("absorbed", 0.0))
	var hp_loss: float = float(absorbed_loss.get("hp_loss", 0.0))
	var total_dmg := absorbed + hp_loss

	if bool(projectile.get("is_ultimate", false)):
		attacker.set("damage_dealt_ultimate", float(attacker.get("damage_dealt_ultimate")) + total_dmg)
	elif bool(projectile.get("is_ability", false)):
		attacker.set("damage_dealt_ability", float(attacker.get("damage_dealt_ability")) + total_dmg)
	else:
		attacker.set("damage_dealt_auto", float(attacker.get("damage_dealt_auto")) + total_dmg)
	attacker.set("damage_dealt", float(attacker.get("damage_dealt")) + total_dmg)
	attacker.call("on_attack_hit", target, total_dmg, self, not bool(projectile.get("is_ability", false) or projectile.get("is_ultimate", false)))

	if float(projectile.get("stun_duration", 0.0)) > 0.0:
		target.call("apply_stun", float(projectile.get("stun_duration", 0.0)), self, int(projectile.get("attacker_id", -1)))

	record_event({
		"timestamp": time,
		"attacker_id": int(projectile.get("attacker_id", -1)),
		"attacker_name": String(attacker.get("display_name")),
		"target_id": int(projectile.get("target_id", -1)),
		"target_name": String(target.get("display_name")),
		"damage": total_dmg,
		"shield_absorbed": absorbed,
		"distance": attacker.global_position.distance_to(target.global_position),
		"target_hp_after": float(target.get("hp")),
		"target_shield_after": float(target.get("shield")),
		"target_selection_reason": String(projectile.get("reason", "Auto Attack")),
	})

	var splash_radius := float(projectile.get("splash_radius", 0.0))
	var splash_ratio := float(projectile.get("splash_ratio", 0.0))
	if splash_radius > 0.0 and splash_ratio > 0.0:
		var splash_damage := total_dmg * splash_ratio
		for splash_target in nearby_units(target.global_position, splash_radius, "", int(target.get("instance_id"))):
			if String(splash_target.get("team")) == String(attacker.get("team")):
				continue
			var splash_result: Dictionary = splash_target.call("take_damage", splash_damage, self, String(projectile.get("damage_type", "true")), int(projectile.get("attacker_id", -1)))
			var splash_absorbed := float(splash_result.get("absorbed", 0.0))
			var splash_hp_loss := float(splash_result.get("hp_loss", 0.0))
			var splash_total := splash_absorbed + splash_hp_loss
			record_event({
				"timestamp": time,
				"attacker_id": int(projectile.get("attacker_id", -1)),
				"attacker_name": String(attacker.get("display_name")),
				"target_id": int(splash_target.get("instance_id")),
				"target_name": String(splash_target.get("display_name")),
				"damage": splash_total,
				"shield_absorbed": splash_absorbed,
				"distance": attacker.global_position.distance_to(splash_target.global_position),
				"target_hp_after": float(splash_target.get("hp")),
				"target_shield_after": float(splash_target.get("shield")),
				"target_selection_reason": String(projectile.get("reason", "Auto Attack")) + " splash",
			})
