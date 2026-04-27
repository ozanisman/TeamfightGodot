extends Control

## Horizontal P1 (blue) | draws (gray) | P2 (red): percentages, then proportional bar.

const COLOR_P1 := Color(0.275, 0.51, 1.0)
const COLOR_P2 := Color(0.88, 0.31, 0.31)
const COLOR_DRAW := Color(0.55, 0.55, 0.55)
const COLOR_TRACK := Color(0.12, 0.12, 0.14, 1.0)

const FONT_SIZE_PX: int = 17
const PERCENT_BASELINE_Y: float = 18.0
const GAP_AFTER_PERCENT_LINE: float = 10.0
const BAR_HEIGHT: float = 24.0

var _p1: int = 0
var _p2: int = 0
var _draws: int = 0
var _total: int = 0


func set_balance(p1: int, p2: int, draws: int, total: int) -> void:
	_p1 = p1
	_p2 = p2
	_draws = draws
	_total = total
	queue_redraw()


func _notification(what: int) -> void:
	if what == NOTIFICATION_THEME_CHANGED:
		queue_redraw()


func _pct_label(total: int, part: int, team: String) -> String:
	if total < 1:
		return "—"
	return team + ": " + ("%.1f%%" % (100.0 * float(part) / float(total)))


func _draw() -> void:
	var bw: float = size.x
	var font: Font = ThemeDB.fallback_font
	var half: float = bw / 2.0
	var y_bar: float = PERCENT_BASELINE_Y + GAP_AFTER_PERCENT_LINE

	var rp1: float = float(_p1) / float(_total) if _total > 0 else 0.0
	var rp2: float = float(_p2) / float(_total) if _total > 0 else 0.0

	_draw_segment_label(font, 0.0, half, _pct_label(_total, _p1, "P1"), COLOR_P1)
	_draw_segment_label(font, half, half, _pct_label(_total, _p2, "P2"), COLOR_P2)

	draw_rect(Rect2(0.0, y_bar, bw, BAR_HEIGHT), COLOR_TRACK)
	if _total < 1:
		return

	var w1: float = rp1 * bw
	var w2: float = maxf(0.0, bw - w1)

	if w1 > 0.0:
		draw_rect(Rect2(0.0, y_bar, maxf(1.0, w1), BAR_HEIGHT), COLOR_P1)
	if w2 > 0.0:
		draw_rect(Rect2(w1, y_bar, maxf(1.0, w2), BAR_HEIGHT), COLOR_P2)


func _draw_segment_label(font: Font, seg_x: float, seg_w: float, text: String, col: Color) -> void:
	if seg_w < 1.0:
		return
	draw_string(
		font,
		Vector2(seg_x, PERCENT_BASELINE_Y),
		text,
		HORIZONTAL_ALIGNMENT_CENTER,
		seg_w,
		FONT_SIZE_PX,
		col
	)
