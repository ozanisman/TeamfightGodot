extends Control

const COLOR_SCALE_MID_TICK := Color(55.0 / 255.0, 55.0 / 255.0, 68.0 / 255.0)
const COLOR_DRAW_SEGMENT := Color(0.5, 0.5, 0.52, 1.0)
const MID_TICK_WIDTH_PX := 3.0
const MID_TICK_HALF_HEIGHT_PX := 7.0

## When negative: one segment up to fill_ratio. When non-negative: stacked win (fill_ratio) then draw (this ratio).
var _draw_segment_ratio: float = -1.0
var _fill_ratio: float = 0.0
var _bar_color: Color = Color.WHITE
var _ci_low_ratio: float = -1.0
var _ci_high_ratio: float = -1.0
var _show_scale_mid_tick: bool = false


func set_visual(
	fill_ratio: float,
	bar_color: Color,
	ci_low_ratio: float = -1.0,
	ci_high_ratio: float = -1.0,
	show_scale_mid_tick: bool = false,
	draw_segment_ratio: float = -1.0
) -> void:
	_fill_ratio = fill_ratio
	_bar_color = bar_color
	_ci_low_ratio = ci_low_ratio
	_ci_high_ratio = ci_high_ratio
	_show_scale_mid_tick = show_scale_mid_tick
	_draw_segment_ratio = draw_segment_ratio
	queue_redraw()


func _draw() -> void:
	var h: float = maxf(1.0, size.y - 6.0)
	var y: float = 3.0
	var outline := Color(0.18, 0.18, 0.22, 1.0)
	var track := Color(0.12, 0.12, 0.15, 1.0)
	draw_rect(Rect2(0.0, y, size.x, h), track)
	if _draw_segment_ratio >= 0.0:
		var win_w: float = size.x * clampf(_fill_ratio, 0.0, 1.0)
		var draw_w: float = size.x * clampf(_draw_segment_ratio, 0.0, 1.0)
		draw_w = minf(draw_w, maxf(0.0, size.x - win_w))
		if win_w > 0.0:
			draw_rect(Rect2(0.0, y, win_w, h), _bar_color)
		if draw_w > 0.0:
			draw_rect(Rect2(win_w, y, draw_w, h), COLOR_DRAW_SEGMENT)
	else:
		var w: float = maxf(0.0, size.x * clampf(_fill_ratio, 0.0, 1.0))
		if w > 0.0:
			draw_rect(Rect2(0.0, y, w, h), _bar_color)
	draw_rect(Rect2(0.0, y, size.x, h), outline, false, 1.0)
	if _show_scale_mid_tick:
		var ym: float = y + h * 0.5
		var tick_top: float = ym - MID_TICK_HALF_HEIGHT_PX
		var tick_h: float = MID_TICK_HALF_HEIGHT_PX * 2.0
		var tick_left: float = roundf(size.x * 0.5 - MID_TICK_WIDTH_PX * 0.5)
		draw_rect(Rect2(tick_left, tick_top, MID_TICK_WIDTH_PX, tick_h), COLOR_SCALE_MID_TICK)
	if _ci_low_ratio >= 0.0 and _ci_high_ratio >= 0.0:
		var x0: float = size.x * clampf(_ci_low_ratio, 0.0, 1.0)
		var x1: float = size.x * clampf(_ci_high_ratio, 0.0, 1.0)
		var ym: float = y + h * 0.5
		draw_line(Vector2(x0, ym), Vector2(x1, ym), Color(0.9, 0.9, 0.9), 1.0)
		draw_line(Vector2(x0, ym - 3.0), Vector2(x0, ym + 3.0), Color(0.9, 0.9, 0.9), 1.0)
		draw_line(Vector2(x1, ym - 3.0), Vector2(x1, ym + 3.0), Color(0.9, 0.9, 0.9), 1.0)
