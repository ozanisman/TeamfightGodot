extends Control

## Vertical guides matching Python/stats_gui.py draw_chart (50%, 100%, bar origin).

const COLOR_GUIDE := Color(45.0 / 255.0, 45.0 / 255.0, 55.0 / 255.0)
const COLOR_BAR_ORIGIN := Color(60.0 / 255.0, 60.0 / 255.0, 70.0 / 255.0)

var label_width: float = 180.0
## Matches chart row panel horizontal content margin (stats_dashboard UI_ROW_MARGIN).
var row_h_inset: float = 0.0
## Metric value column (right of bar), then games, then kda.
var metric_val_col_w: float = 120.0
var games_col_w: float = 128.0
var kda_col_w: float = 56.0
var col_sep: float = 8.0
var show_percent_guides: bool = false
var show_bar_origin_line: bool = false


func set_layout(
	p_label_w: float,
	p_metric_val_w: float,
	p_games_w: float,
	p_kda_w: float,
	p_sep: float,
	p_row_h_inset: float = 0.0
) -> void:
	label_width = p_label_w
	metric_val_col_w = p_metric_val_w
	games_col_w = p_games_w
	kda_col_w = p_kda_w
	col_sep = p_sep
	row_h_inset = p_row_h_inset
	queue_redraw()


func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED:
		queue_redraw()


func _draw() -> void:
	var w: float = size.x
	var h: float = size.y
	if w < 2.0 or h < 2.0:
		return
	var bar_left: float = row_h_inset + label_width + col_sep
	var after_bar: float = (
		col_sep + metric_val_col_w + col_sep + games_col_w + col_sep + kda_col_w
	)
	var bar_right: float = w - row_h_inset - after_bar
	if bar_right <= bar_left + 2.0:
		return
	if show_bar_origin_line:
		draw_line(Vector2(bar_left, 0.0), Vector2(bar_left, h), COLOR_BAR_ORIGIN, 2.0)
	if show_percent_guides:
		draw_line(Vector2(bar_right, 0.0), Vector2(bar_right, h), COLOR_GUIDE, 1.0)
