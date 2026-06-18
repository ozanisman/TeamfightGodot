extends Node2D
## Grid + P1/P2 zone tint; preparation shows both zones, combat can hide zones for clarity.

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const UiTokensScript := preload("res://scripts/ui/ui_tokens.gd")

## Match outer edge of square battle map (world space), not the thin grid.
const BATTLE_OUTLINE_WIDTH_PX: float = 2.5
const BATTLE_INNER_INSET_PX: float = 0.5
const BATTLE_OUTLINE_COLOR := Color(0.42, 0.46, 0.6, 1.0)
const BATTLE_OUTLINE_INNER_GLOW := Color(0.55, 0.6, 0.72, 0.5)

var show_preparation_zones: bool = false
var _grid_divisions: int = SimConstantsScript.VIEWER_WORLD_GRID_DIVISIONS
var _world_size: float = SimConstantsScript.WORLD_SIZE
var _zone_p1_w_ratio: float = 0.45
var _zone_p2_w_ratio: float = 0.45
var _zone_p2_x_ratio: float = 0.55


func _ready() -> void:
	# Slightly under units (0); avoid very negative z — global sort with parent Control can hide grid.
	z_index = UiTokensScript.Z_BACKGROUND
	set_process(false)


func _draw() -> void:
	var vp: Vector2 = get_viewport_rect().size if is_inside_tree() else Vector2(800, 600)
	var s: float = SimConstantsScript.viewer_battle_square_side(vp)
	var w: float = s
	var h: float = s
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
	# Framing border: inner soft line, then main outline (on top of grid).
	var b: Rect2 = Rect2(0, 0, w, h)
	draw_rect(
		Rect2(b.position + Vector2.ONE * BATTLE_INNER_INSET_PX, b.size - Vector2.ONE * (BATTLE_INNER_INSET_PX * 2.0)),
		BATTLE_OUTLINE_INNER_GLOW,
		false,
		1.0,
		true
	)
	draw_rect(b, BATTLE_OUTLINE_COLOR, false, BATTLE_OUTLINE_WIDTH_PX, true)


func refresh() -> void:
	queue_redraw()
