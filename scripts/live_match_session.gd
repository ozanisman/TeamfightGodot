extends RefCounted
class_name LiveMatchSession

const ChampionCatalog = preload("res://scripts/champion_catalog.gd")
const CombatData = preload("res://scripts/combat_data.gd")
const CombatWorldScript = preload("res://scripts/combat_world.gd")
const BattleMetricsScript = preload("res://scripts/battle_metrics.gd")
const BattleUnitScene = preload("res://scenes/battle_unit.tscn")

const TEAM_COLOR_PLAYER := Color(0.35, 0.60, 1.0)
const TEAM_COLOR_ENEMY := Color(0.92, 0.35, 0.35)
const PREP_LEFT_MIN := 0.5
const PREP_LEFT_MAX := 4.5
const PREP_Y_MIN := 0.5
const PREP_Y_MAX := 9.5

var combat_world = CombatWorldScript.new()
var combat_metrics = null
var combat_ready: bool = false
var player_heroes: Array[String] = []
var enemy_heroes: Array[String] = []
var units: Array = []


func set_world_size(size: Vector2) -> void:
	_ensure_arena_metrics(size)
	_sync_all_units()


func bootstrap_combat_registry() -> void:
	ChampionCatalog.bootstrap_combat_registry(combat_world.get_combat_registry())


func load_rosters(new_player_heroes: Array[String], new_enemy_heroes: Array[String], size: Vector2) -> void:
	player_heroes = new_player_heroes.duplicate()
	enemy_heroes = new_enemy_heroes.duplicate()
	combat_ready = false
	_ensure_arena_metrics(size)
	_rebuild_units()
	combat_world.set_units(_state_units())
	var combat_registry: Object = combat_world.get_combat_registry()
	for unit in units:
		unit.combat_state.set_combat_world(combat_world)
		unit.combat_state.set_combat_registry(combat_registry)
	_sync_all_units()


func begin_combat() -> void:
	_ensure_arena_metrics(CombatData.WORLD_SIZE_VECTOR)
	combat_ready = true
	for unit in units:
		if not is_instance_valid(unit):
			continue
		unit.combat_state.apply_combat_metrics(combat_metrics)
		unit.set_collision_enabled(true)
	combat_world.rebuild_spatial_index()


func capture_spawn_positions() -> void:
	if combat_ready and combat_metrics != null:
		combat_world.capture_spawn_positions()
		return
	for unit in units:
		if is_instance_valid(unit):
			unit.combat_state.set_spawn_position(_prep_screen_to_world(unit.global_position))


func clear_rosters() -> void:
	for unit in units:
		if is_instance_valid(unit):
			unit.queue_free()
	units.clear()
	combat_ready = false
	combat_metrics = null
	combat_world.clear()


func step(dt: float) -> Dictionary:
	if combat_world == null:
		return {}
	combat_world.step(dt)
	return combat_world.get_match_result()


func _rebuild_units() -> void:
	clear_rosters()
	_spawn_team(player_heroes, "player", true)
	_spawn_team(enemy_heroes, "enemy", false)


func _spawn_team(hero_ids: Array[String], team: String, draggable: bool) -> void:
	var visual_scale := float(combat_metrics.call("scale_length", 1.0)) if combat_metrics != null else 1.0
	for index in range(hero_ids.size()):
		var hero_id := hero_ids[index]
		var hero := ChampionCatalog.get_by_id(hero_id)
		if hero.is_empty():
			continue

		var unit: Node2D = BattleUnitScene.instantiate()
		unit.call(
			"set_identity",
			hero_id,
			String(hero.get("name", hero_id)),
			String(hero.get("role", "")),
			team,
			TEAM_COLOR_PLAYER if team == "player" else TEAM_COLOR_ENEMY,
			draggable,
			String(hero.get("passive_id", ""))
		)
		unit.call(
			"set_combat_stats",
			float(hero.get("max_hp", 1.0)),
			float(hero.get("attack_damage", 0.0)),
			float(hero.get("attack_range", 0.3)),
			float(hero.get("attack_speed", 1.0)),
			float(hero.get("move_speed", 1.0)),
			float(hero.get("armor", 0.0)),
			float(hero.get("projectile_speed", CombatData.DEFAULT_PROJECTILE_SPEED)),
			float(hero.get("magic_resist", 0.0)),
			float(hero.get("tenacity", 0.0)),
			float(hero.get("life_steal", 0.0)),
			float(hero.get("respawn_time", CombatData.RESPAWN_TIME)),
			float(hero.get("mana_per_attack", 0.0)),
			float(hero.get("max_mana", 0.0)),
			float(hero.get("ability_cd", 0.0)),
			float(hero.get("ultimate_cd", 0.0)),
			float(hero.get("projectile_radius", CombatData.DEFAULT_PROJECTILE_RADIUS))
		)
		var spawn_pos := _initial_world_position(index, team)
		unit.call("set_spawn_position", spawn_pos)
		unit.call("set_visual_scale", visual_scale)
		unit.call("set_world_position", _prep_world_to_screen(spawn_pos))
		unit.call("set_collision_enabled", false)
		units.append(unit)


func _initial_world_position(index: int, team: String) -> Vector2:
	var row := floori(index / 2)
	var col := index % 2
	var x := 1.0 + float(col) * 1.15
	var y := 2.0 + float(row) * 2.0
	var world_point := Vector2(x, y)
	if team == "enemy":
		world_point.x = 9.0 - float(col) * 1.15
		world_point.x = clampf(world_point.x, CombatData.WORLD_SIZE - PREP_LEFT_MAX, CombatData.WORLD_SIZE - PREP_LEFT_MIN)
		return Vector2(clampf(world_point.x, CombatData.WORLD_SIZE - PREP_LEFT_MAX, CombatData.WORLD_SIZE - PREP_LEFT_MIN), clampf(world_point.y, PREP_Y_MIN, PREP_Y_MAX))
	return Vector2(clampf(world_point.x, PREP_LEFT_MIN, PREP_LEFT_MAX), clampf(world_point.y, PREP_Y_MIN, PREP_Y_MAX))


func _sync_all_units() -> void:
	for unit in units:
		unit.set_focus(false, false, false)


func _state_units() -> Array:
	var result: Array = []
	for unit in units:
		if is_instance_valid(unit):
			result.append(unit.combat_state)
	return result


func _ensure_arena_metrics(size: Vector2) -> void:
	if combat_metrics != null:
		return
	combat_metrics = BattleMetricsScript.new()
	combat_metrics.setup_for_world(size, 1.0)


func _prep_world_to_screen(world_pos: Vector2) -> Vector2:
	if combat_metrics == null:
		return world_pos
	return combat_metrics.call("world_to_combat", world_pos)


func _prep_screen_to_world(screen_pos: Vector2) -> Vector2:
	if combat_metrics == null:
		return screen_pos
	return combat_metrics.call("combat_to_world", screen_pos)
