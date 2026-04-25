extends Node2D
## Grid + P1/P2 zone tint; preparation shows both zones, combat can hide zones for clarity.

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

var show_preparation_zones: bool = false
var _grid_divisions: int = SimConstantsScript.VIEWER_WORLD_GRID_DIVISIONS
var _world_size: float = SimConstantsScript.WORLD_SIZE
var _zone_p1_w_ratio: float = 0.45
var _zone_p2_w_ratio: float = 0.45
var _zone_p2_x_ratio: float = 0.55


func _ready() -> void:
	z_index = -100
	set_process(false)


func _draw() -> void:
	var r: Rect2 = get_viewport_rect() if is_inside_tree() else Rect2(0, 0, 800, 600)
	var w: float = r.size.x
	var h: float = r.size.y
	if w < 1.0 or h < 1.0:
		return
	if show_preparation_zones:
		var p1_w: float = w * _zone_p1_w_ratio
		var p2_w: float = w * _zone_p2_w_ratio
		var p2_x: float = w * _zone_p2_x_ratio
		draw_rect(Rect2(0, 0, p1_w, h), Color(0.157, 0.196, 0.314, 0.5))
		draw_rect(Rect2(p2_x, 0, p2_w, h), Color(0.314, 0.157, 0.157, 0.5))
	for i in range(1, _grid_divisions):
		var x: float = w * (float(i) / float(_grid_divisions))
		var y: float = h * (float(i) / float(_grid_divisions))
		draw_line(Vector2(x, 0), Vector2(x, h), Color(0.157, 0.157, 0.204, 0.6), 1.0)
		draw_line(Vector2(0, y), Vector2(w, y), Color(0.157, 0.157, 0.204, 0.6), 1.0)


func refresh() -> void:
	queue_redraw()
