extends RefCounted
class_name CombatWorld

const CombatData = preload("res://scripts/combat_data.gd")
const CombatRegistryScript = preload("res://scripts/combat_registry.gd")
const CombatProjectileStateScript = preload("res://scripts/combat_projectile_state.gd")
const CombatUnitStateScript = preload("res://scripts/combat_unit_state.gd")
const TargetingSystemScript = preload("res://scripts/targeting_system.gd")

signal combat_event_recorded(event: Dictionary)

var tick_rate: float = CombatData.DEFAULT_WORLD_TICK_RATE
var time: float = 0.0
var tick: int = 0
var units: Array = []
var projectiles: Array[CombatProjectileState] = []
var targeting_system = TargetingSystemScript.new()
var combat_registry = CombatRegistryScript.new()
var player_kills: int = 0
var enemy_kills: int = 0
var record_events: bool = true
var simulate_motion: bool = false
var rng: RandomNumberGenerator = RandomNumberGenerator.new()
var _events_current_tick: Array[Dictionary] = []
var _next_instance_id: int = 1
var _spatial_cells: Dictionary = {}
var _spatial_cell_by_unit_id: Dictionary = {}


func _init() -> void:
	rng.randomize()


func clear() -> void:
	for unit in units:
		if is_instance_valid(unit):
			_unit_set_combat_world(unit, null)
			_unit_set_combat_registry(unit, null)
			_unit_set_current_target(unit, null)
			_unit_set_current_ally_target(unit, null)
	units.clear()
	projectiles.clear()
	_events_current_tick.clear()
	_spatial_cells.clear()
	_spatial_cell_by_unit_id.clear()
	time = 0.0
	tick = 0
	player_kills = 0
	enemy_kills = 0
	_next_instance_id = 1


func set_seed(seed: int) -> void:
	rng.seed = seed


func random_float() -> float:
	return rng.randf()


func set_units(new_units: Array) -> void:
	units = []
	units.append_array(new_units)
	_spatial_cells.clear()
	_spatial_cell_by_unit_id.clear()
	for unit in units:
		if is_instance_valid(unit):
			_unit_set_instance_id(unit, _next_instance_id)
			_next_instance_id += 1
	rebuild_spatial_index()


func get_targeting_system() -> Object:
	return targeting_system


func get_combat_registry() -> Object:
	return combat_registry


func capture_spawn_positions() -> void:
	for unit in units:
		if is_instance_valid(unit):
			_unit_set_spawn_position(unit, _unit_position(unit))


func get_unit_by_id(instance_id: int) -> Object:
	for unit in units:
		if is_instance_valid(unit) and _unit_instance_id(unit) == instance_id:
			return unit
	return null


func get_alive_units(team: String = "") -> Array[Object]:
	var result: Array[Object] = []
	for unit in units:
		if not is_instance_valid(unit) or not _unit_is_alive(unit):
			continue
		if team != "" and _unit_team(unit) != team:
			continue
		result.append(unit)
	return result


func get_allies_for(unit: Object) -> Array[Object]:
	return get_alive_units(_unit_team(unit))


func get_enemies_for(unit: Object) -> Array[Object]:
	var result: Array[Object] = []
	var unit_team := _unit_team(unit)
	for other in get_alive_units():
		if _unit_team(other) == unit_team:
			continue
		result.append(other)
	return result


func nearby_units(pos: Vector2, radius: float, team: String = "", exclude_id: int = -1) -> Array[Object]:
	return get_units_in_radius(pos, radius, team, exclude_id)


func get_units_in_radius(pos: Vector2, radius: float, team: String = "", exclude_id: int = -1) -> Array[Object]:
	if radius <= 0.0:
		return []
	var radius_sq := radius * radius
	var min_cell := _cell_coords_for_pos(pos - Vector2(radius, radius))
	var max_cell := _cell_coords_for_pos(pos + Vector2(radius, radius))
	var result: Array[Object] = []
	for cell_x in range(min_cell.x, max_cell.x + 1):
		for cell_y in range(min_cell.y, max_cell.y + 1):
			var cell_key := _cell_key(cell_x, cell_y)
			var cell_units: Array = _spatial_cells.get(cell_key, [])
			for unit in cell_units:
				if not is_instance_valid(unit) or not _unit_is_alive(unit):
					continue
				if exclude_id >= 0 and _unit_instance_id(unit) == exclude_id:
					continue
				if team != "" and _unit_team(unit) != team:
					continue
				if _unit_position(unit).distance_squared_to(pos) <= radius_sq:
					result.append(unit)
	return result


func get_nearby_allies_for(unit: Object, radius: float = CombatData.SPATIAL_GRID_CELL_SIZE * CombatData.TARGET_BROAD_PHASE_CELL_RADIUS) -> Array[Object]:
	return get_units_in_radius(_unit_position(unit), radius, _unit_team(unit))


func get_nearby_enemies_for(unit: Object, radius: float = CombatData.SPATIAL_GRID_CELL_SIZE * CombatData.TARGET_BROAD_PHASE_CELL_RADIUS) -> Array[Object]:
	var result: Array[Object] = []
	for enemy in get_units_in_radius(_unit_position(unit), radius, "", _unit_instance_id(unit)):
		if _unit_team(enemy) != _unit_team(unit):
			result.append(enemy)
	return result


func rebuild_spatial_index() -> void:
	_spatial_cells.clear()
	_spatial_cell_by_unit_id.clear()
	for unit in units:
		if is_instance_valid(unit):
			_add_unit_to_spatial_cell(unit, _cell_key_for_position(_unit_position(unit)))


func notify_unit_moved(unit: Object, _old_pos: Vector2) -> void:
	if unit == null or not is_instance_valid(unit) or not _unit_is_alive(unit):
		return
	_update_unit_spatial_cell(unit)


func adjust_target_pressure(old_target: Object, new_target: Object) -> void:
	if old_target == new_target:
		return
	if old_target != null and is_instance_valid(old_target):
		var old_target_count: int = _unit_incoming_target_count(old_target)
		_unit_set_incoming_target_count(old_target, max(0, old_target_count - 1))
	if new_target != null and is_instance_valid(new_target) and _unit_is_alive(new_target):
		var new_target_count: int = _unit_incoming_target_count(new_target)
		_unit_set_incoming_target_count(new_target, new_target_count + 1)


func _update_unit_spatial_cell(unit: Object) -> void:
	var unit_id := _unit_instance_id(unit)
	var new_key := _cell_key_for_position(_unit_position(unit))
	var old_key := String(_spatial_cell_by_unit_id.get(unit_id, ""))
	if old_key == new_key:
		return
	if old_key != "":
		_remove_unit_from_spatial_cell(unit, old_key)
	_add_unit_to_spatial_cell(unit, new_key)


func _add_unit_to_spatial_cell(unit: Object, cell_key: String) -> void:
	if not _spatial_cells.has(cell_key):
		_spatial_cells[cell_key] = []
	var cell_units: Array = _spatial_cells[cell_key]
	cell_units.append(unit)
	_spatial_cells[cell_key] = cell_units
	_spatial_cell_by_unit_id[_unit_instance_id(unit)] = cell_key


func _remove_unit_from_spatial_cell(unit: Object, cell_key: String) -> void:
	if not _spatial_cells.has(cell_key):
		return
	var cell_units: Array = _spatial_cells[cell_key]
	var unit_id := _unit_instance_id(unit)
	for i in range(cell_units.size() - 1, -1, -1):
		if not is_instance_valid(cell_units[i]) or _unit_instance_id(cell_units[i]) == unit_id:
			cell_units.remove_at(i)
	if cell_units.is_empty():
		_spatial_cells.erase(cell_key)
	else:
		_spatial_cells[cell_key] = cell_units
	_spatial_cell_by_unit_id.erase(unit_id)


func _cell_key_for_position(pos: Vector2) -> String:
	var cell := _cell_coords_for_pos(pos)
	return _cell_key(cell.x, cell.y)


func _cell_coords_for_pos(pos: Vector2) -> Vector2i:
	var cell_size := maxf(CombatData.SPATIAL_GRID_CELL_SIZE, CombatData.EPSILON)
	return Vector2i(floori(pos.x / cell_size), floori(pos.y / cell_size))


func _cell_key(cell_x: int, cell_y: int) -> String:
	return "%d:%d" % [cell_x, cell_y]


func refresh_target_pressure(source_units: Array = []) -> void:
	var alive_units := source_units if not source_units.is_empty() else get_alive_units()
	for unit in alive_units:
		_unit_set_incoming_target_count(unit, 0)
	for unit in alive_units:
		var target: Object = _unit_current_target(unit)
		if target != null and is_instance_valid(target) and _unit_is_alive(target):
			_unit_set_incoming_target_count(target, _unit_incoming_target_count(target) + 1)


func spawn_projectile(
	attacker: Object,
	target: Object,
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
	var projectile: CombatProjectileState = CombatProjectileStateScript.new()
	projectile.attacker_id = _unit_instance_id(attacker)
	projectile.target_id = _unit_instance_id(target)
	projectile.team = _unit_team(attacker)
	projectile.pos = _unit_position(attacker)
	projectile.speed = speed
	projectile.radius = radius
	projectile.damage = damage
	projectile.damage_type = damage_type
	projectile.reason = reason
	projectile.stun_duration = stun_duration
	projectile.is_ability = is_ability
	projectile.is_ultimate = is_ultimate
	projectile.splash_radius = splash_radius
	projectile.splash_ratio = splash_ratio
	projectile.source_attack_range = _unit_attack_range(attacker)
	projectile.source_projectile_radius = _unit_projectile_radius(attacker)
	projectiles.append(projectile)


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
			alive_at_start[_unit_instance_id(unit)] = _unit_is_alive(unit)

	var projectiles_to_remove: Array[int] = []
	for i in range(projectiles.size()):
		var projectile: CombatProjectileState = projectiles[i]
		var target := get_unit_by_id(projectile.target_id)
		if target == null or not _unit_is_alive(target):
			projectiles_to_remove.append(i)
			continue

		var target_pos: Vector2 = _unit_position(target)
		var proj_pos: Vector2 = projectile.pos
		var direction: Vector2 = target_pos - proj_pos
		var dist: float = direction.length()
		var move_dist: float = projectile.speed * dt
		if dist <= move_dist + projectile.radius:
			_resolve_projectile_hit(projectile, target)
			projectiles_to_remove.append(i)
		elif dist > CombatData.EPSILON:
			projectile.pos = proj_pos + direction * (move_dist / dist)
			projectiles[i] = projectile

	for i in range(projectiles_to_remove.size() - 1, -1, -1):
		projectiles.remove_at(projectiles_to_remove[i])

	refresh_target_pressure(get_alive_units())

	for unit in units:
		if is_instance_valid(unit):
			_unit_update(unit, dt, self)
			if simulate_motion:
				_unit_step_motion(unit, dt)

	refresh_target_pressure(get_alive_units())

	for unit in units:
		if not is_instance_valid(unit):
			continue
		var uid := _unit_instance_id(unit)
		if bool(alive_at_start.get(uid, false)) and not _unit_is_alive(unit):
			_unit_set_deaths(unit, _unit_deaths(unit) + 1)
			if _unit_team(unit) == "player":
				enemy_kills += 1
			else:
				player_kills += 1

			var killer_id := -1
			var last_time := -INF
			var recent_damage_sources: Dictionary = _unit_recent_damage_sources(unit)
			for attacker_id in recent_damage_sources.keys():
				var timestamp := float(recent_damage_sources[attacker_id])
				if timestamp > last_time:
					last_time = timestamp
					killer_id = int(attacker_id)

			var assistants: Dictionary = {}
			if killer_id >= 0:
				var killer := get_unit_by_id(killer_id)
				if killer != null:
					_unit_set_kills(killer, _unit_kills(killer) + 1)
					var killer_benefactors: Dictionary = _unit_recent_benefactors(killer)
					for benefactor_id in killer_benefactors.keys():
						var timestamp := float(killer_benefactors[benefactor_id])
						if time - timestamp <= CombatData.ASSIST_WINDOW:
							assistants[int(benefactor_id)] = true
				for attacker_id in recent_damage_sources.keys():
					var timestamp := float(recent_damage_sources[attacker_id])
					if attacker_id != killer_id and time - timestamp <= CombatData.ASSIST_WINDOW:
						assistants[int(attacker_id)] = true

			for ally in units:
				if is_instance_valid(ally) and assistants.has(_unit_instance_id(ally)):
					_unit_set_assists(ally, _unit_assists(ally) + 1)


func winner() -> String:
	var player_alive := get_alive_units("player").size()
	var enemy_alive := get_alive_units("enemy").size()
	if player_alive <= 0 and enemy_alive <= 0:
		if time < CombatData.MATCH_DURATION:
			return "draw"
	elif player_alive <= 0:
		return "enemy"
	elif enemy_alive <= 0:
		return "player"
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


func _resolve_projectile_hit(projectile: CombatProjectileState, target: Object) -> void:
	var attacker := get_unit_by_id(projectile.attacker_id)
	if attacker == null:
		return

	var absorbed_loss: Dictionary = _unit_take_damage(target, projectile.damage, self, projectile.damage_type, projectile.attacker_id)
	var absorbed: float = float(absorbed_loss.get("absorbed", 0.0))
	var hp_loss: float = float(absorbed_loss.get("hp_loss", 0.0))
	var total_dmg := absorbed + hp_loss

	if projectile.is_ultimate:
		_unit_set_damage_dealt_ultimate(attacker, _unit_damage_dealt_ultimate(attacker) + total_dmg)
	elif projectile.is_ability:
		_unit_set_damage_dealt_ability(attacker, _unit_damage_dealt_ability(attacker) + total_dmg)
	else:
		_unit_set_damage_dealt_auto(attacker, _unit_damage_dealt_auto(attacker) + total_dmg)
	_unit_set_damage_dealt(attacker, _unit_damage_dealt(attacker) + total_dmg)
	_unit_on_attack_hit(attacker, target, total_dmg, self, not (projectile.is_ability or projectile.is_ultimate))

	if projectile.stun_duration > 0.0:
		_unit_apply_stun(target, projectile.stun_duration, self, projectile.attacker_id)

	record_event({
		"timestamp": time,
		"attacker_id": projectile.attacker_id,
		"attacker_name": _unit_display_name(attacker),
		"target_id": projectile.target_id,
		"target_name": _unit_display_name(target),
		"damage": total_dmg,
		"shield_absorbed": absorbed,
		"distance": _unit_position(attacker).distance_to(_unit_position(target)),
		"target_hp_after": _unit_hp(target),
		"target_shield_after": _unit_shield(target),
		"target_selection_reason": projectile.reason,
	})

	var splash_radius := projectile.splash_radius
	var splash_ratio := projectile.splash_ratio
	if splash_radius > 0.0 and splash_ratio > 0.0:
		var splash_damage := total_dmg * splash_ratio
		for splash_target in nearby_units(_unit_position(target), splash_radius, "", _unit_instance_id(target)):
			if _unit_team(splash_target) == _unit_team(attacker):
				continue
			var splash_result: Dictionary = _unit_take_damage(splash_target, splash_damage, self, projectile.damage_type, projectile.attacker_id)
			var splash_absorbed := float(splash_result.get("absorbed", 0.0))
			var splash_hp_loss := float(splash_result.get("hp_loss", 0.0))
			var splash_total := splash_absorbed + splash_hp_loss
			record_event({
				"timestamp": time,
				"attacker_id": projectile.attacker_id,
				"attacker_name": _unit_display_name(attacker),
				"target_id": _unit_instance_id(splash_target),
				"target_name": _unit_display_name(splash_target),
				"damage": splash_total,
				"shield_absorbed": splash_absorbed,
				"distance": _unit_position(attacker).distance_to(_unit_position(splash_target)),
				"target_hp_after": _unit_hp(splash_target),
				"target_shield_after": _unit_shield(splash_target),
				"target_selection_reason": projectile.reason + " splash",
			})


func _unit_state(unit: Object) -> Object:
	if unit != null and unit.get_script() == CombatUnitStateScript:
		return unit
	return null


func _unit_instance_id(unit: Object) -> int:
	var state := _unit_state(unit)
	if state != null:
		return state.instance_id
	return int(unit.get("instance_id"))


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


func _unit_current_target(unit: Object) -> Object:
	var state := _unit_state(unit)
	if state != null:
		return state.current_target
	return unit.get("current_target")


func _unit_current_ally_target(unit: Object) -> Object:
	var state := _unit_state(unit)
	if state != null:
		return state.current_ally_target
	return unit.get("current_ally_target")


func _unit_attack_range(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.attack_range
	return float(unit.get("attack_range"))


func _unit_projectile_radius(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.projectile_radius
	return float(unit.get("projectile_radius"))


func _unit_is_alive(unit: Object) -> bool:
	var state := _unit_state(unit)
	if state != null:
		return state.alive and state.hp > 0.0
	return bool(unit.call("is_alive"))


func _unit_incoming_target_count(unit: Object) -> int:
	var state := _unit_state(unit)
	if state != null:
		return state.incoming_target_count
	return int(unit.get("incoming_target_count"))


func _unit_set_incoming_target_count(unit: Object, value: int) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.incoming_target_count = value
	else:
		unit.set("incoming_target_count", value)


func _unit_set_instance_id(unit: Object, value: int) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.instance_id = value
	else:
		unit.set("instance_id", value)


func _unit_set_spawn_position(unit: Object, pos: Vector2) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.spawn_pos = pos
	else:
		unit.call("set_spawn_position", pos)


func _unit_set_current_target(unit: Object, target: Object) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.current_target = target
	else:
		unit.set("current_target", target)


func _unit_set_current_ally_target(unit: Object, target: Object) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.current_ally_target = target
	else:
		unit.set("current_ally_target", target)


func _unit_set_combat_world(unit: Object, world: Object) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.set_combat_world(world)
	elif unit.has_method("set_combat_world"):
		unit.call("set_combat_world", world)


func _unit_set_combat_registry(unit: Object, registry: Object) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.set_combat_registry(registry)
	elif unit.has_method("set_combat_registry"):
		unit.call("set_combat_registry", registry)


func _unit_update(unit: Object, dt: float, world: Object) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.update(dt, world)
	else:
		unit.call("update", dt, world)


func _unit_step_motion(unit: Object, dt: float) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.step_motion(dt)
	elif unit.has_method("step_motion"):
		unit.call("step_motion", dt)


func _unit_take_damage(unit: Object, amount: float, world: Object, damage_type: String, source_id: int) -> Dictionary:
	var state := _unit_state(unit)
	if state != null:
		return state.take_damage(amount, world, damage_type, source_id)
	return unit.call("take_damage", amount, world, damage_type, source_id)


func _unit_apply_stun(unit: Object, duration: float, world: Object, source_id: int) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.apply_stun(duration, world, source_id)
	else:
		unit.call("apply_stun", duration, world, source_id)


func _unit_on_attack_hit(unit: Object, target: Object, damage: float, world: Object, is_auto: bool) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.on_attack_hit(target, damage, world, is_auto)
	else:
		unit.call("on_attack_hit", target, damage, world, is_auto)


func _unit_set_damage_dealt(unit: Object, value: float) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.damage_dealt = value
	else:
		unit.set("damage_dealt", value)


func _unit_damage_dealt(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.damage_dealt
	return float(unit.get("damage_dealt"))


func _unit_set_damage_dealt_auto(unit: Object, value: float) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.damage_dealt_auto = value
	else:
		unit.set("damage_dealt_auto", value)


func _unit_damage_dealt_auto(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.damage_dealt_auto
	return float(unit.get("damage_dealt_auto"))


func _unit_set_damage_dealt_ability(unit: Object, value: float) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.damage_dealt_ability = value
	else:
		unit.set("damage_dealt_ability", value)


func _unit_damage_dealt_ability(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.damage_dealt_ability
	return float(unit.get("damage_dealt_ability"))


func _unit_set_damage_dealt_ultimate(unit: Object, value: float) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.damage_dealt_ultimate = value
	else:
		unit.set("damage_dealt_ultimate", value)


func _unit_damage_dealt_ultimate(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.damage_dealt_ultimate
	return float(unit.get("damage_dealt_ultimate"))


func _unit_hp(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.hp
	return float(unit.get("hp"))


func _unit_shield(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.shield
	return float(unit.get("shield"))


func _unit_display_name(unit: Object) -> String:
	var state := _unit_state(unit)
	if state != null:
		return state.display_name
	return String(unit.get("display_name"))


func _unit_recent_damage_sources(unit: Object) -> Dictionary:
	var state := _unit_state(unit)
	if state != null:
		return state.recent_damage_sources
	return unit.get("recent_damage_sources")


func _unit_recent_benefactors(unit: Object) -> Dictionary:
	var state := _unit_state(unit)
	if state != null:
		return state.recent_benefactors
	return unit.get("recent_benefactors")


func _unit_set_kills(unit: Object, value: int) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.kills = value
	else:
		unit.set("kills", value)


func _unit_kills(unit: Object) -> int:
	var state := _unit_state(unit)
	if state != null:
		return state.kills
	return int(unit.get("kills"))


func _unit_set_deaths(unit: Object, value: int) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.deaths = value
	else:
		unit.set("deaths", value)


func _unit_deaths(unit: Object) -> int:
	var state := _unit_state(unit)
	if state != null:
		return state.deaths
	return int(unit.get("deaths"))


func _unit_set_assists(unit: Object, value: int) -> void:
	var state := _unit_state(unit)
	if state != null:
		state.assists = value
	else:
		unit.set("assists", value)


func _unit_assists(unit: Object) -> int:
	var state := _unit_state(unit)
	if state != null:
		return state.assists
	return int(unit.get("assists"))
