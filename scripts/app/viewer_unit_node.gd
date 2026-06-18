extends Node2D
## Screen-space unit disk + bars; [member position] is in screen pixels (parent sets from world map).

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const UiTokensScript := preload("res://scripts/ui/ui_tokens.gd")

const UNIT_RADIUS: float = 9.0
const HP_W: float = 30.0
const HP_H: float = 5.0
const LUNGE_SCREEN_PX: float = 8.0
const VIEWER_BASE_MIN_AXIS: float = 720.0

func _screen_scale() -> float:
	var vp: Viewport = get_viewport()
	if vp == null:
		return 1.0
	var sz: Vector2 = vp.get_visible_rect().size
	return float(min(sz.x, sz.y)) / VIEWER_BASE_MIN_AXIS

signal unit_clicked(p_instance_id: int, p_team: StringName)

var instance_id: int = 0
var _team: StringName = &""
var _selected: bool = false
var _u: Dictionary = {}


func _ready() -> void:
	var s: float = _screen_scale()
	var hit := ColorRect.new()
	hit.size = Vector2(32, 32) * s
	hit.position = Vector2(-16, -16) * s
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


func _draw_hp_ticks(o: Vector2, hp_x: float, hp_y: float, bar_w: float, bar_h: float, max_hp: float, hp: float, shield: float) -> void:
	var total_effective: float = hp + shield
	var scale: float = bar_w / maxf(max_hp, total_effective)
	var tick_color := Color(0.3, 0.3, 0.3, 0.8)
	
	# Use total_effective as the upper bound for tick drawing when it exceeds max_hp
	var tick_upper_bound: float = maxf(max_hp, total_effective)
	
	# Draw ticks every 10 HP (top to middle)
	var tick_10_interval := 10.0
	var num_10_ticks := int(tick_upper_bound / tick_10_interval)
	for i in range(1, num_10_ticks + 1):
		var tick_hp := i * tick_10_interval
		if tick_hp > total_effective:
			break
		var tick_x := hp_x + tick_hp * scale
		var tick_height := bar_h * 0.5
		draw_line(o + Vector2(tick_x, hp_y), o + Vector2(tick_x, hp_y + tick_height), tick_color, 2.0)
	
	# Draw ticks every 100 HP (top to bottom)
	var tick_100_interval := 100.0
	var num_100_ticks := int(tick_upper_bound / tick_100_interval)
	for i in range(1, num_100_ticks + 1):
		var tick_hp := i * tick_100_interval
		if tick_hp > total_effective:
			break
		var tick_x := hp_x + tick_hp * scale
		draw_line(o + Vector2(tick_x, hp_y), o + Vector2(tick_x, hp_y + bar_h), tick_color, 2.0)


func _lunge_offset_screen() -> Vector2:
	if _u.is_empty():
		return Vector2.ZERO
	var acd: float = maxf(0.0, float(_u.get("attack_cooldown", 0.0)))
	var period: float = maxf(0.0001, float(_u.get("attack_period", 1.0)))
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
	var sc: float = _screen_scale()
	var unit_r: float = UNIT_RADIUS * sc
	var hp_w: float = HP_W * sc
	var hp_h: float = HP_H * sc
	var wcol := UiTokensScript.COLOR_UNIT_PLAYER
	if _team == &"enemy":
		wcol = UiTokensScript.COLOR_UNIT_ENEMY
	var o: Vector2 = _lunge_offset_screen()
	if _u.is_empty():
		draw_circle(o, unit_r, wcol)
		return
	if _selected:
		var ar: float = float(_u.get("attack_range", 0.0))
		if ar > 0.0:
			var vp2: Vector2 = get_viewport_rect().size
			var s_b: float = SimConstantsScript.viewer_battle_square_side(vp2)
			var r_px: float = ar * (s_b / SimConstantsScript.WORLD_SIZE)
			draw_arc(o, r_px, 0.0, TAU, 64, Color(0.8, 0.8, 0.4, 0.55), 1.0, true)
	if String(_u.get("state", "")) == SimConstants.UNIT_STATE_STUNNED:
		draw_circle(o, unit_r + 3.0 * sc, UiTokensScript.COLOR_UNIT_STUN_RING, false, 2.0 * sc)
	# Apply stealth opacity
	var draw_color := wcol
	if float(_u.get("stealth_remaining", 0.0)) > 0.0:
		draw_color.a = 0.4
	draw_circle(o, unit_r, draw_color)
	if float(_u.get("casting_remaining", 0.0)) > 0.0:
		var pr: float = 1.0 - float(_u.get("casting_remaining", 0.0)) / SimConstantsScript.CASTING_WINDUP
		pr = clampf(pr, 0.0, 1.0)
		var r_ring: float = unit_r + 4.0 * sc
		var ckind: String = str(_u.get("casting_kind", ""))
		var c_cast: Color = UiTokensScript.COLOR_CASTING_ULT if ckind.to_lower().contains("ult") else UiTokensScript.COLOR_CASTING_ABILITY
		draw_arc(o, r_ring, PI * 0.5, PI * 0.5 + TAU * pr, 32, c_cast, 3.0 * sc, true)
	var hp: float = float(_u.get("hp", 0.0))
	var max_hp: float = maxf(1.0, float(_u.get("max_hp", 1.0)))
	var shield: float = float(_u.get("shield", 0.0))
	var hp_x: float = -hp_w * 0.5
	var hp_y: float = -24.0 * sc
	draw_rect(Rect2(o + Vector2(hp_x, hp_y), Vector2(hp_w, hp_h)), UiTokensScript.COLOR_HP_BAR_BG)
	
	# Calculate HP and shield ratios for display
	var total_effective: float = hp + shield
	var hp_ratio: float
	var shield_ratio: float
	
	if total_effective > max_hp:
		# When HP + Shield > Max HP, show ratio of HP to total effective health
		hp_ratio = clampf(hp / total_effective, 0.0, 1.0)
		shield_ratio = clampf(shield / total_effective, 0.0, 1.0)
	else:
		# When HP + Shield <= Max HP, show ratios relative to Max HP
		hp_ratio = clampf(hp / max_hp, 0.0, 1.0)
		shield_ratio = clampf(shield / max_hp, 0.0, 1.0)
	
	# Draw HP bar (green)
	draw_rect(Rect2(o + Vector2(hp_x, hp_y), Vector2(hp_w * hp_ratio, hp_h)), UiTokensScript.COLOR_HP_BAR_FILL)
	
	# Draw tick marks (commented out - may re-enable later)
	# Draw before shield so they're visible on shield portion
	# _draw_hp_ticks(o, hp_x, hp_y, hp_w, hp_h, max_hp, hp, shield)
	
	# Draw shield bar (white) layered on top, to the right of HP
	if shield > 0.0:
		var shield_x: float = hp_x + hp_w * hp_ratio
		draw_rect(Rect2(o + Vector2(shield_x, hp_y), Vector2(hp_w * shield_ratio, hp_h)), UiTokensScript.COLOR_SHIELD_FILL)
	var mana_cost: float = float(_u.get("mana_cost", 0.0))
	if mana_cost > 0.0:
		var mn: float = float(_u.get("mana", 0.0))
		var m_ratio: float = clampf(mn / maxf(1.0, mana_cost), 0.0, 1.0)
		draw_rect(
			Rect2(o + Vector2(hp_x, hp_y + hp_h + 1.0 * sc), Vector2(hp_w * m_ratio, 3.0 * sc)),
			UiTokensScript.COLOR_MANA_BAR_FILL
		)
	var label: String = str(_u.get("unit_id", &""))
	if label.is_empty():
		label = "?"
	if String(_u.get("state", "")) == SimConstants.UNIT_STATE_DEAD:
		label += " [%s]" % SimConstants.UNIT_STATE_DEAD
	elif String(_u.get("state", "")) == SimConstants.UNIT_STATE_STUNNED:
		label += " [%s]" % SimConstants.UNIT_STATE_STUNNED
	elif float(_u.get("taunt_remaining", 0.0)) > 0.0:
		label += " [TAUNTED]"
	elif String(_u.get("state", "")) == SimConstants.UNIT_STATE_KITING:
		label += " [%s]" % SimConstants.UNIT_STATE_KITING
	elif float(_u.get("casting_remaining", 0.0)) > 0.0:
		var ckind: String = str(_u.get("casting_kind", ""))
		if ckind.to_lower().contains("ult"):
			label += " [ULTIMATE]"
		else:
			label += " [ABILITY]"
	elif bool(_u.get("is_channeling", false)):
		var ckind: String = str(_u.get("channel_action_kind", ""))
		if ckind.to_lower().contains("ult"):
			label += " [CHANNELING ULT]"
		else:
			label += " [CHANNELING]"
	elif int(_u.get("target_id", 0)) > 0 and bool(_u.get("in_range", false)):
		label += " [ATTACKING]"
	elif int(_u.get("target_id", 0)) > 0:
		label += " [MOVING]"
	else:
		label += " [WAITING]"
	var font: Font = ThemeDB.fallback_font
	draw_string(font, o + Vector2(-20 * sc, -34 * sc), label, HORIZONTAL_ALIGNMENT_LEFT, 200 * sc, int(11 * sc), UiTokensScript.COLOR_UNIT_LABEL)
