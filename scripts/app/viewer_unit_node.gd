extends Node2D
## Screen-space unit disk + bars; [member position] is in screen pixels (parent sets from world map).

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

const UNIT_RADIUS: float = 9.0
const HP_W: float = 30.0
const HP_H: float = 5.0
const LUNGE_SCREEN_PX: float = 8.0

signal unit_clicked(p_instance_id: int, p_team: StringName)

var instance_id: int = 0
var _team: StringName = &""
var _selected: bool = false
var _u: Dictionary = {}


func _ready() -> void:
	var hit := ColorRect.new()
	hit.size = Vector2(32, 32)
	hit.position = Vector2(-16, -16)
	hit.modulate = Color(1, 1, 1, 0.001)
	hit.mouse_filter = Control.MOUSE_FILTER_STOP
	hit.gui_input.connect(_on_hit_gui)
	add_child(hit)


func _on_hit_gui(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		unit_clicked.emit(instance_id, _team)


func setup(id: int, team: StringName) -> void:
	instance_id = id
	_team = team


func set_snapshot_data(u: Dictionary) -> void:
	_u = u
	queue_redraw()


func set_selected(on: bool) -> void:
	_selected = on
	queue_redraw()


func _lunge_offset_screen() -> Vector2:
	if _u.is_empty():
		return Vector2.ZERO
	var as_m: float = maxf(0.0001, float(_u.get("attack_speed", 1.0)))
	var acd: float = maxf(0.0, float(_u.get("attack_cooldown", 0.0)))
	var period: float = 1.0 / as_m
	var elapsed: float = period - acd
	var lunge_dur: float = 0.12
	if not (elapsed >= 0.0 and elapsed < lunge_dur and bool(_u.get("alive", true))):
		return Vector2.ZERO
	var t_id: int = int(_u.get("target_id", 0))
	if t_id <= 0:
		return Vector2.ZERO
	var w_layer: Node2D = get_parent() as Node2D
	if w_layer == null:
		return Vector2.ZERO
	var t_node: Node2D = w_layer.get_node_or_null("Unit_%d" % t_id) as Node2D
	if t_node == null or not is_instance_valid(t_node) or not t_node.visible:
		return Vector2.ZERO
	var p: float = clampf(elapsed / lunge_dur, 0.0, 1.0)
	var d: Vector2 = t_node.position - position
	var dist: float = d.length()
	if dist < 0.01:
		return Vector2.ZERO
	return (d / dist) * (LUNGE_SCREEN_PX * sin(PI * p))


func _draw() -> void:
	var wcol := Color(0.275, 0.51, 1.0, 1.0)
	if _team == &"enemy":
		wcol = Color(0.882, 0.314, 0.314, 1.0)
	var o: Vector2 = _lunge_offset_screen()
	if _u.is_empty():
		draw_circle(o, UNIT_RADIUS, wcol)
		return
	if _selected:
		var ar: float = float(_u.get("attack_range", 0.0))
		if ar > 0.0:
			var vp2: Vector2 = get_viewport_rect().size
			var s_b: float = SimConstantsScript.viewer_battle_square_side(vp2)
			var r_px: float = ar * (s_b / SimConstantsScript.WORLD_SIZE)
			draw_arc(o, r_px, 0.0, TAU, 64, Color(0.8, 0.8, 0.4, 0.55), 1.0, true)
	if float(_u.get("stun_remaining", 0.0)) > 0.0 or String(_u.get("state", "")) == "STUNNED":
		draw_circle(o, UNIT_RADIUS + 3.0, Color(0.9, 0.65, 0.2, 0.9), false, 2.0)
	draw_circle(o, UNIT_RADIUS, wcol)
	if float(_u.get("casting_remaining", 0.0)) > 0.0:
		var pr: float = 1.0 - float(_u.get("casting_remaining", 0.0)) / SimConstantsScript.CASTING_WINDUP
		pr = clampf(pr, 0.0, 1.0)
		var r_ring: float = UNIT_RADIUS + 4.0
		var ckind: String = str(_u.get("casting_kind", ""))
		var c_cast: Color = Color(1.0, 0.86, 0.4) if ckind.to_lower().contains("ult") else Color(0.47, 0.86, 0.55)
		draw_arc(o, r_ring, PI * 0.5, PI * 0.5 + TAU * pr, 32, c_cast, 3.0, true)
	var hp: float = float(_u.get("hp", 0.0))
	var max_hp: float = maxf(1.0, float(_u.get("max_hp", 1.0)))
	var ratio: float = clampf(hp / max_hp, 0.0, 1.0)
	var hp_x: float = -HP_W * 0.5
	var hp_y: float = -24.0
	draw_rect(Rect2(o + Vector2(hp_x, hp_y), Vector2(HP_W, HP_H)), Color(0.31, 0.31, 0.31, 1.0))
	draw_rect(Rect2(o + Vector2(hp_x, hp_y), Vector2(HP_W * ratio, HP_H)), Color(0.31, 0.82, 0.31, 1.0))
	if float(_u.get("shield", 0.0)) > 0.0:
		var sr: float = minf(1.0, float(_u.get("shield", 0.0)) / max_hp)
		draw_rect(
			Rect2(o + Vector2(hp_x, hp_y - 3.0), Vector2(HP_W * sr, 2.0)),
			Color(0.6, 0.78, 1.0, 0.9)
		)
	var max_mana: float = float(_u.get("max_mana", 0.0))
	if max_mana > 0.0:
		var mn: float = float(_u.get("mana", 0.0))
		var m_ratio: float = clampf(mn / maxf(1.0, max_mana), 0.0, 1.0)
		draw_rect(
			Rect2(o + Vector2(hp_x, hp_y + HP_H + 1.0), Vector2(HP_W * m_ratio, 3.0)),
			Color(0.3, 0.55, 0.9, 0.9)
		)
	var label: String = str(_u.get("archetype_id", &""))
	if label.is_empty():
		label = "?"
	if String(_u.get("state", "")) == "KITING":
		label += " [KITING]"
	elif float(_u.get("casting_remaining", 0.0)) > 0.0:
		var ckind: String = str(_u.get("casting_kind", ""))
		if ckind.to_lower().contains("ult"):
			label += " [ULTIMATE]"
		else:
			label += " [ABILITY]"
	elif float(_u.get("stun_remaining", 0.0)) > 0.0 or String(_u.get("state", "")) == "STUNNED":
		label += " [STUNNED]"
	elif int(_u.get("target_id", 0)) > 0:
		label += " [ATTACKING]"
	else:
		label += " [MOVING]"
	var font: Font = ThemeDB.fallback_font
	draw_string(font, o + Vector2(-20, -34), label, HORIZONTAL_ALIGNMENT_LEFT, 200, 11, Color(0.95, 0.95, 0.95))
