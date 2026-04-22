extends Node2D
class_name Battlefield

const CombatData = preload("res://scripts/combat_data.gd")
const BattleMetricsScript = preload("res://scripts/battle_metrics.gd")
const LiveMatchSessionScript = preload("res://scripts/live_match_session.gd")
const FloatingTextEffect = preload("res://scripts/floating_text_effect.gd")

const PREP_LEFT_MIN := 0.5
const PREP_LEFT_MAX := 4.5
const PREP_Y_MIN := 0.5
const PREP_Y_MAX := 9.5
const PREP_SNAP_GRID := 2.0
const PROJECTILE_DRAW_SCALE := 0.25
const PROJECTILE_DRAW_MIN_RADIUS := 1.0
signal focus_changed(text: String)

var world_size: Vector2 = CombatData.WORLD_SIZE_VECTOR
var current_phase: int = 0
var _arena_boundaries: Array[StaticBody2D] = []
var _selected_unit: Node2D = null
var _hovered_unit: Node2D = null
var _dragging_unit: Node2D = null
var _session: Object = LiveMatchSessionScript.new()
var _floating_texts: Array[FloatingTextEffect] = []


func _ready() -> void:
	set_process(true)
	set_process_unhandled_input(true)
	_ensure_arena_metrics()
	_session.combat_world.combat_event_recorded.connect(_on_combat_event_recorded)
	queue_redraw()


func set_world_size(size: Vector2) -> void:
	world_size = size
	_session.set_world_size(size)
	_sync_all_units()
	queue_redraw()


func set_phase(phase: int) -> void:
	current_phase = phase
	for unit in _session.units:
		if is_instance_valid(unit):
			unit.call("set_collision_enabled", current_phase == 2)
	if current_phase != 1:
		_clear_drag_state()
	_sync_focus_label()


func begin_combat() -> void:
	_ensure_arena_metrics()
	_session.begin_combat()
	_build_arena_boundaries()
	for unit in _session.units:
		if not is_instance_valid(unit):
			continue
		unit.call("apply_combat_metrics", _session.combat_metrics)
		unit.call("set_collision_enabled", true)
	_session.combat_world.rebuild_spatial_index()
	queue_redraw()


func load_rosters(player_heroes: Array[String], enemy_heroes: Array[String]) -> void:
	_session.load_rosters(player_heroes, enemy_heroes, world_size)
	_clear_arena_boundaries()
	_sync_all_units()
	_sync_focus_label()


func capture_spawn_positions() -> void:
	if _session.combat_ready and _session.combat_metrics != null:
		_session.capture_spawn_positions()
		return
	for unit in _session.units:
		if is_instance_valid(unit):
			unit.call("set_spawn_position", _prep_screen_to_world(unit.global_position))


func bootstrap_combat_registry() -> void:
	_session.bootstrap_combat_registry()


func clear_rosters() -> void:
	_session.clear_rosters()
	_clear_arena_boundaries()
	_selected_unit = null
	_hovered_unit = null
	_dragging_unit = null
	_floating_texts.clear()
	_sync_focus_label()


func step_simulation(dt: float) -> Dictionary:
	if current_phase == 2:
		var result := _session.step(dt)
		_step_floating_texts(dt)
		return result
	else:
		_sync_all_units()
		_update_hover_state()
	return _session.combat_world.get_match_result()


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
	for unit in _session.units:
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
	var prep_pos := _clamp_prep_screen_position(screen_pos)
	var spawn_pos := _prep_screen_to_world(prep_pos)
	spawn_pos.x = snappedf(clampf(spawn_pos.x, PREP_LEFT_MIN, PREP_LEFT_MAX), PREP_SNAP_GRID)
	spawn_pos.y = snappedf(clampf(spawn_pos.y, PREP_Y_MIN, PREP_Y_MAX), PREP_SNAP_GRID)
	unit.call("set_spawn_position", spawn_pos)
	unit.call("set_world_position", _prep_world_to_screen(spawn_pos))


func _snap_selected_unit() -> void:
	if _selected_unit == null:
		return
	_set_unit_world_pos_from_screen(_selected_unit, _selected_unit.global_position)
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
	for unit in _session.units:
		unit.call("set_focus", unit == _selected_unit, unit == _hovered_unit, unit == _dragging_unit)
	_sync_focus_label()


func _sync_all_units() -> void:
	for unit in _session.units:
		unit.call("set_focus", unit == _selected_unit, unit == _hovered_unit, unit == _dragging_unit)
	queue_redraw()


func _clear_arena_boundaries() -> void:
	for body in _arena_boundaries:
		if is_instance_valid(body):
			body.queue_free()
	_arena_boundaries.clear()


func _build_arena_boundaries() -> void:
	_clear_arena_boundaries()
	if _session.combat_metrics == null:
		return

	var rect: Rect2 = _session.combat_metrics.get_rect()
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
		world_pos = _session.combat_metrics.get_rect().get_center() if _session.combat_ready and _session.combat_metrics != null else Vector2.ZERO

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
	for unit in _session.units:
		if int(unit.get("instance_id")) == instance_id:
			return unit.global_position
	return null


func _prep_world_to_screen(world_pos: Vector2) -> Vector2:
	_ensure_arena_metrics()
	if _session.combat_metrics == null:
		return world_pos
	return _session.combat_metrics.call("world_to_combat", world_pos)


func _prep_screen_to_world(screen_pos: Vector2) -> Vector2:
	_ensure_arena_metrics()
	if _session.combat_metrics == null:
		return screen_pos
	return _session.combat_metrics.call("combat_to_world", screen_pos)


func _clamp_prep_screen_position(screen_pos: Vector2) -> Vector2:
	if _session.combat_metrics != null:
		return _session.combat_metrics.clamp_position(screen_pos)
	var viewport_size := get_viewport_rect().size
	return Vector2(
		clampf(screen_pos.x, 0.0, viewport_size.x),
		clampf(screen_pos.y, 0.0, viewport_size.y)
	)


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
	_ensure_arena_metrics()
	var size := get_viewport_rect().size
	if size.x <= 0.0 or size.y <= 0.0:
		return

	draw_rect(Rect2(Vector2.ZERO, size), Color(0.08, 0.09, 0.12), true)
	var grid_color := Color(0.22, 0.24, 0.30)
	var arena_rect := Rect2(Vector2.ZERO, size)
	var divisions := 10
	if _session.combat_metrics != null:
		arena_rect = _session.combat_metrics.get_rect()
		draw_rect(arena_rect, Color(0.07, 0.08, 0.11), true)
	for i in range(1, divisions):
		var x := arena_rect.position.x + arena_rect.size.x * float(i) / float(divisions)
		var y := arena_rect.position.y + arena_rect.size.y * float(i) / float(divisions)
		draw_line(Vector2(x, arena_rect.position.y), Vector2(x, arena_rect.position.y + arena_rect.size.y), grid_color, 1.0)
		draw_line(Vector2(arena_rect.position.x, y), Vector2(arena_rect.position.x + arena_rect.size.x, y), grid_color, 1.0)

	if current_phase == 1:
		var left_zone := Rect2(arena_rect.position, Vector2(arena_rect.size.x * 0.45, arena_rect.size.y))
		var right_zone := Rect2(
			Vector2(arena_rect.position.x + arena_rect.size.x * 0.55, arena_rect.position.y),
			Vector2(arena_rect.size.x * 0.45, arena_rect.size.y)
		)
		draw_rect(left_zone, Color(0.20, 0.28, 0.42, 0.18), true)
		draw_rect(right_zone, Color(0.42, 0.20, 0.20, 0.18), true)
		draw_rect(arena_rect, Color(0.45, 0.48, 0.55), false, 2.0)

	if current_phase == 2:
		for projectile in _session.combat_world.projectiles:
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


func _ensure_arena_metrics() -> void:
	var viewport_size := get_viewport_rect().size
	if viewport_size.x <= 0.0 or viewport_size.y <= 0.0:
		return
	if _session.combat_metrics != null:
		var rect: Rect2 = _session.combat_metrics.get_rect()
		var expected_side := minf(viewport_size.x, viewport_size.y)
		var expected_origin := Vector2(
			(viewport_size.x - expected_side) * 0.5,
			(viewport_size.y - expected_side) * 0.5
		)
		if is_equal_approx(rect.size.x, expected_side) and is_equal_approx(rect.size.y, expected_side) and rect.position.distance_to(expected_origin) <= CombatData.EPSILON:
			return
	_session.combat_metrics = BattleMetricsScript.new()
	_session.combat_metrics.setup_for_viewport(viewport_size)
