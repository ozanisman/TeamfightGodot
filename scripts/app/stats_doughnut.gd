extends Control

const COLOR_PLAYER := Color(0.275, 0.51, 1.0)
const COLOR_ENEMY := Color(0.88, 0.31, 0.31)
const COLOR_DRAW := Color(0.55, 0.55, 0.55)
const TAU := 6.283185307179586

var _p1_ratio: float = 0.0
var _p2_ratio: float = 0.0
var _draw_ratio: float = 0.0


func set_balance(p1: int, p2: int, draws: int, total: int) -> void:
	if total <= 0:
		_p1_ratio = 0.0
		_p2_ratio = 0.0
		_draw_ratio = 0.0
	else:
		_p1_ratio = float(p1) / float(total)
		_p2_ratio = float(p2) / float(total)
		_draw_ratio = float(draws) / float(total)
	queue_redraw()


func _draw() -> void:
	var c: Vector2 = size * 0.5
	var rad: float = mini(size.x, size.y) * 0.35
	var start: float = -PI * 0.5
	if _p1_ratio > 0.0:
		var end1: float = start + _p1_ratio * TAU
		draw_arc(c, rad, start, end1, 48, COLOR_PLAYER, 12.0, true)
		start = end1
	if _p2_ratio > 0.0:
		var end2: float = start + _p2_ratio * TAU
		draw_arc(c, rad, start, end2, 48, COLOR_ENEMY, 12.0, true)
		start = end2
	if _draw_ratio > 0.0:
		var end3: float = start + _draw_ratio * TAU
		draw_arc(c, rad, start, end3, 48, COLOR_DRAW, 12.0, true)
