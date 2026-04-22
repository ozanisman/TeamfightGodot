extends Node2D
class_name Battlefield

const ChampionCatalog = preload("res://scripts/champion_catalog.gd")
const CombatData = preload("res://scripts/combat_data.gd")
const CombatWorldScript = preload("res://scripts/combat_world.gd")
const BattleMetricsScript = preload("res://scripts/battle_metrics.gd")
const BattleUnitScene = preload("res://scenes/battle_unit.tscn")
const FloatingTextEffect = preload("res://scripts/floating_text_effect.gd")

const PREP_LEFT_MIN := 0.5
const PREP_LEFT_MAX := 4.5
const PREP_Y_MIN := 0.5
const PREP_Y_MAX := 9.5
const PREP_SNAP_GRID := 2.0
const PROJECTILE_DRAW_SCALE := 0.25
const PROJECTILE_DRAW_MIN_RADIUS := 1.0
const TEAM_COLOR_PLAYER := Color(0.35, 0.60, 1.0)
const TEAM_COLOR_ENEMY := Color(0.92, 0.35, 0.35)

signal focus_changed(text: String)

var world_size: Vector2 = CombatData.WORLD_SIZE_VECTOR
var current_phase: int = 0
var _combat_metrics = null
var _combat_ready: bool = false
var _player_heroes: Array[String] = []
var _enemy_heroes: Array[String] = []
var _units: Array[Node2D] = []
var _arena_boundaries: Array[StaticBody2D] = []
var _selected_unit: Node2D = null
var _hovered_unit: Node2D = null
var _dragging_unit: Node2D = null
var _combat_world = CombatWorldScript.new()
var _floating_texts: Array[FloatingTextEffect] = []


func _ready() -> void:
	set_process(true)
	set_process_unhandled_input(true)
	_combat_world.combat_event_recorded.connect(_on_combat_event_recorded)
	queue_redraw()


func set_world_size(size: Vector2) -> void:
	world_size = size
	_sync_all_units()
	queue_redraw()


func set_phase(phase: int) -> void:
	current_phase = phase
	for unit in _units:
		if is_instance_valid(unit):
			unit.call("set_collision_enabled", current_phase == 2)
	if current_phase != 1:
		_clear_drag_state()
	_sync_focus_label()


func begin_combat() -> void:
	_combat_metrics = BattleMetricsScript.new()
	_combat_metrics.setup(get_viewport_rect().size)
	_combat_ready = true
	_build_arena_boundaries()
	for unit in _units:
		if not is_instance_valid(unit):
			continue
		unit.call("apply_combat_metrics", _combat_metrics)
		unit.call("set_collision_enabled", true)
	_combat_world.rebuild_spatial_index()
	queue_redraw()


func load_rosters(player_heroes: Array[String], enemy_heroes: Array[String]) -> void:
	_player_heroes = player_heroes.duplicate()
	_enemy_heroes = enemy_heroes.duplicate()
	_combat_ready = false
	_combat_metrics = null
	_clear_arena_boundaries()
	_rebuild_units()
	_combat_world.set_units(_units)
	var combat_registry: Object = _combat_world.get_combat_registry()
	for unit in _units:
		unit.call("set_combat_world", _combat_world)
		unit.call("set_combat_registry", combat_registry)
	_combat_world.capture_spawn_positions()
	_sync_all_units()
	_sync_focus_label()


func capture_spawn_positions() -> void:
	_combat_world.capture_spawn_positions()


func bootstrap_combat_registry() -> void:
	ChampionCatalog.bootstrap_combat_registry(_combat_world.get_combat_registry())


func clear_rosters() -> void:
	for unit in _units:
		unit.queue_free()
	_units.clear()
	_clear_arena_boundaries()
	_selected_unit = null
	_hovered_unit = null
	_dragging_unit = null
	_floating_texts.clear()
	_combat_ready = false
	_combat_metrics = null
	_combat_world.clear()
	_sync_focus_label()


func step_simulation(dt: float) -> Dictionary:
	if current_phase == 2:
		_combat_world.step(dt)
		_step_floating_texts(dt)
	else:
		_sync_all_units()
	_update_hover_state()
	return _combat_world.get_match_result()


func _rebuild_units() -> void:
	clear_rosters()
	_spawn_team(_player_heroes, "player", true)
	_spawn_team(_enemy_heroes, "enemy", false)


func _spawn_team(hero_ids: Array[String], team: String, draggable: bool) -> void:
	for index in range(hero_ids.size()):
		var hero_id := hero_ids[index]
		var hero := ChampionCatalog.get_by_id(hero_id)
		if hero.is_empty():
			continue

		var unit: Node2D = BattleUnitScene.instantiate()
		add_child(unit)
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
		unit.call("set_world_position", spawn_pos)
		unit.call("set_collision_enabled", current_phase == 2)
		_units.append(unit)


func _initial_world_position(index: int, team: String) -> Vector2:
	var row := floori(index / 2)
	var col := index % 2
	var x := 1.0 + float(col) * 1.15
	var y := 2.0 + float(row) * 2.0
	var world_point := Vector2(x, y)
	if team == "enemy":
		world_point.x = 9.0 - float(col) * 1.15
		world_point.x = clampf(world_point.x, PREP_LEFT_MIN, maxf(world_size.x - PREP_LEFT_MIN, PREP_LEFT_MAX))
	return Vector2(clampf(world_point.x, PREP_LEFT_MIN, PREP_LEFT_MAX), clampf(world_point.y, PREP_Y_MIN, PREP_Y_MAX))


func _process(_delta: float) -> void:
	_sync_all_units()
	_update_hover_state()


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		var mouse := (event as InputEventMouseButton).position
		if event.pressed:
			if current_phase == 1:
				_begin_drag(mouse)
			elif current_phase == 2:
				_select_unit(mouse)
		elif _dragging_unit != null:
			_dragging_unit.set("dragging", false)
			_dragging_unit = null
			_snap_selected_unit()
			_sync_focus_label()
	elif event is InputEventMouseMotion:
		if _dragging_unit != null:
			_set_unit_world_pos_from_screen(_dragging_unit, (event as InputEventMouseMotion).position)
			_sync_all_units()
			_sync_focus_label()


func _select_unit(screen_pos: Vector2) -> void:
	_selected_unit = _unit_at_screen_point(screen_pos)
	_sync_all_units()
	_sync_focus_label()


func _begin_drag(screen_pos: Vector2) -> void:
	var unit: Node2D = _unit_at_screen_point(screen_pos)
	if unit == null or not bool(unit.get("draggable")):
		_sync_focus_label()
		return

	_selected_unit = unit
	_dragging_unit = unit
	unit.set("dragging", true)
	_set_unit_world_pos_from_screen(unit, screen_pos)
	_sync_all_units()
	_sync_focus_label()


func _unit_at_screen_point(screen_pos: Vector2) -> Node2D:
	for unit in _units:
		if not bool(unit.call("is_alive")):
			continue
		if not bool(unit.get("draggable")) and current_phase == 1:
			continue
		if unit.global_position.distance_to(screen_pos) <= 26.0:
			return unit
	return null


func _set_unit_world_pos_from_screen(unit: Node2D, screen_pos: Vector2) -> void:
	var viewport_size := get_viewport_rect().size
	if viewport_size.x <= 0.0 or viewport_size.y <= 0.0:
		return
	var world := Vector2(
		clampf((screen_pos.x / viewport_size.x) * world_size.x, PREP_LEFT_MIN, PREP_LEFT_MAX),
		clampf((screen_pos.y / viewport_size.y) * world_size.y, PREP_Y_MIN, PREP_Y_MAX)
	)
	world.x = snappedf(world.x, PREP_SNAP_GRID)
	world.y = snappedf(world.y, PREP_SNAP_GRID)
	world.x = clampf(world.x, PREP_LEFT_MIN, PREP_LEFT_MAX)
	world.y = clampf(world.y, PREP_Y_MIN, PREP_Y_MAX)
	unit.call("set_world_position", world)


func _snap_selected_unit() -> void:
	if _selected_unit == null:
		return
	var selected_pos: Vector2 = _selected_unit.global_position
	selected_pos.x = snappedf(selected_pos.x, PREP_SNAP_GRID)
	selected_pos.y = snappedf(selected_pos.y, PREP_SNAP_GRID)
	_selected_unit.call("set_world_position", selected_pos)
	_sync_all_units()


func _clear_drag_state() -> void:
	if _dragging_unit != null:
		_dragging_unit.set("dragging", false)
	_dragging_unit = null
	_selected_unit = null
	_hovered_unit = null


func _update_hover_state() -> void:
	var hovered: Node2D = _unit_at_screen_point(get_global_mouse_position())
	if hovered == _hovered_unit:
		return

	_hovered_unit = hovered
	for unit in _units:
		unit.call("set_focus", unit == _selected_unit, unit == _hovered_unit, unit == _dragging_unit)
	_sync_focus_label()


func _sync_all_units() -> void:
	for unit in _units:
		unit.call("set_focus", unit == _selected_unit, unit == _hovered_unit, unit == _dragging_unit)
	queue_redraw()


func _clear_arena_boundaries() -> void:
	for body in _arena_boundaries:
		if is_instance_valid(body):
			body.queue_free()
	_arena_boundaries.clear()


func _build_arena_boundaries() -> void:
	_clear_arena_boundaries()
	if _combat_metrics == null:
		return

	var rect: Rect2 = _combat_metrics.get_rect()
	var wall_thickness := float(CombatData.ARENA_WALL_THICKNESS)
	var half_thickness := wall_thickness * 0.5
	_arena_boundaries.append(_create_arena_wall(
		Vector2(rect.position.x + rect.size.x * 0.5, rect.position.y - half_thickness),
		Vector2(rect.size.x + wall_thickness * 2.0, wall_thickness)
	))
	_arena_boundaries.append(_create_arena_wall(
		Vector2(rect.position.x + rect.size.x * 0.5, rect.position.y + rect.size.y + half_thickness),
		Vector2(rect.size.x + wall_thickness * 2.0, wall_thickness)
	))
	_arena_boundaries.append(_create_arena_wall(
		Vector2(rect.position.x - half_thickness, rect.position.y + rect.size.y * 0.5),
		Vector2(wall_thickness, rect.size.y + wall_thickness * 2.0)
	))
	_arena_boundaries.append(_create_arena_wall(
		Vector2(rect.position.x + rect.size.x + half_thickness, rect.position.y + rect.size.y * 0.5),
		Vector2(wall_thickness, rect.size.y + wall_thickness * 2.0)
	))


func _create_arena_wall(center: Vector2, size: Vector2) -> StaticBody2D:
	var wall := StaticBody2D.new()
	wall.collision_layer = 1
	wall.collision_mask = 0
	wall.position = center
	var shape_node := CollisionShape2D.new()
	var rect_shape := RectangleShape2D.new()
	rect_shape.size = size
	shape_node.shape = rect_shape
	wall.add_child(shape_node)
	add_child(wall)
	return wall


func _step_floating_texts(dt: float) -> void:
	for floating_text in _floating_texts:
		floating_text.step(dt)
	_floating_texts = _floating_texts.filter(func(effect: FloatingTextEffect) -> bool: return effect.is_alive())


func _on_combat_event_recorded(event: Dictionary) -> void:
	var world_pos: Variant = _combat_pos_for_unit_id(int(event.get("target_id", -1)))
	if world_pos == null:
		world_pos = _combat_metrics.get_rect().get_center() if _combat_ready and _combat_metrics != null else Vector2.ZERO

	var text := ""
	var color := Color.WHITE
	if float(event.get("damage", 0.0)) > 0.0:
		text = "-%.0f" % float(event.get("damage", 0.0))
		color = Color(1.0, 0.35, 0.35)
	elif float(event.get("healing", 0.0)) > 0.0:
		text = "+%.0f" % float(event.get("healing", 0.0))
		color = Color(0.35, 1.0, 0.55)
	elif float(event.get("shield_added", 0.0)) > 0.0:
		text = "+%.0f" % float(event.get("shield_added", 0.0))
		color = Color(0.35, 0.65, 1.0)
	if text.is_empty():
		return

	_floating_texts.append(FloatingTextEffect.new(text, world_pos, color, 1.0))


func _combat_pos_for_unit_id(instance_id: int) -> Variant:
	for unit in _units:
		if int(unit.get("instance_id")) == instance_id:
			return unit.global_position
	return null


func _sync_focus_label() -> void:
	var text := "Hover a unit to inspect it."
	if _dragging_unit != null:
		text = "Dragging %s. Release to place." % String(_dragging_unit.get("display_name"))
	elif _selected_unit != null:
		var state: String = _selected_unit.call("get_state_label")
		text = "Selected: %s (%s, %s)." % [String(_selected_unit.get("display_name")), String(_selected_unit.get("team")), state]
	elif _hovered_unit != null:
		text = "Hovering: %s (%s)." % [String(_hovered_unit.get("display_name")), String(_hovered_unit.get("team"))]
	focus_changed.emit(text)


func _draw() -> void:
	var size := get_viewport_rect().size
	if size.x <= 0.0 or size.y <= 0.0:
		return

	draw_rect(Rect2(Vector2.ZERO, size), Color(0.08, 0.09, 0.12), true)
	var grid_color := Color(0.22, 0.24, 0.30)
	var arena_rect := Rect2(Vector2.ZERO, size)
	var divisions := 10
	if current_phase == 2 and _combat_metrics != null:
		arena_rect = _combat_metrics.get_rect()
		draw_rect(arena_rect, Color(0.07, 0.08, 0.11), true)
	for i in range(1, divisions):
		var x := arena_rect.position.x + arena_rect.size.x * float(i) / float(divisions)
		var y := arena_rect.position.y + arena_rect.size.y * float(i) / float(divisions)
		draw_line(Vector2(x, arena_rect.position.y), Vector2(x, arena_rect.position.y + arena_rect.size.y), grid_color, 1.0)
		draw_line(Vector2(arena_rect.position.x, y), Vector2(arena_rect.position.x + arena_rect.size.x, y), grid_color, 1.0)

	if current_phase == 1:
		var left_zone := Rect2(Vector2.ZERO, Vector2(size.x * 0.45, size.y))
		var right_zone := Rect2(Vector2(size.x * 0.55, 0.0), Vector2(size.x * 0.45, size.y))
		draw_rect(left_zone, Color(0.20, 0.28, 0.42, 0.18), true)
		draw_rect(right_zone, Color(0.42, 0.20, 0.20, 0.18), true)
		draw_rect(Rect2(Vector2.ZERO, size), Color(0.45, 0.48, 0.55), false, 2.0)

	if current_phase == 2:
		for projectile in _combat_world.projectiles:
			var proj_pos: Vector2 = projectile.get("pos")
			var screen_pos := proj_pos
			var radius := maxf(PROJECTILE_DRAW_MIN_RADIUS, float(projectile.get("radius", CombatData.DEFAULT_PROJECTILE_RADIUS)))
			draw_circle(screen_pos, radius, Color(1.0, 0.87, 0.35))

		for floating_text in _floating_texts:
			var text_pos := floating_text.world_pos
			var font := ThemeDB.fallback_font
			if font:
				var font_size := 14
				var text_size := font.get_string_size(floating_text.text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size)
				draw_string(
					font,
					Vector2(text_pos.x - text_size.x * 0.5, text_pos.y),
					floating_text.text,
					HORIZONTAL_ALIGNMENT_LEFT,
					-1,
					font_size,
					floating_text.color
				)
