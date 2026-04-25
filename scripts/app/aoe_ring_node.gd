extends Node2D
## Elliptical ring + light fill, drawn in local space; parent sets global position in screen pixels.

var screen_rx: float = 24.0
var screen_ry: float = 24.0
var fill_c: Color = Color(1.0, 0.7, 0.1, 0.2)
var line_c: Color = Color(1.0, 0.75, 0.2, 0.85)
var line_w: float = 3.0
var _segs: int = 40


func setup(p_rx: float, p_ry: float, p_fill: Color, p_line: Color, p_width: float) -> void:
	screen_rx = p_rx
	screen_ry = p_ry
	fill_c = p_fill
	line_c = p_line
	line_w = p_width
	queue_redraw()


func _draw() -> void:
	var n: int = _segs
	if n < 8:
		return
	var poly := PackedVector2Array()
	for i in n:
		var t: float = TAU * float(i) / float(n)
		poly.append(Vector2(cos(t) * screen_rx, sin(t) * screen_ry))
	var fcol: Color = fill_c
	# Godot 4.6: draw_colored_polygon(points, color) — single color for the whole region.
	draw_colored_polygon(poly, fcol)
	# closed stroke
	var stroke := poly.duplicate()
	if stroke.size() > 0:
		stroke.append(stroke[0])
	draw_polyline(stroke, line_c, line_w, true)
