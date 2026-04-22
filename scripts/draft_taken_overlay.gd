extends Control
class_name DraftTakenOverlay


func set_taken(is_taken: bool) -> void:
	visible = is_taken
	queue_redraw()


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	queue_redraw()


func _draw() -> void:
	if not visible:
		return

	var rect_size := size
	if rect_size.x <= 0.0 or rect_size.y <= 0.0:
		return

	var x_color := Color(0.95, 0.28, 0.28, 0.40)
	var line_width := 5.0
	draw_line(Vector2(8.0, 8.0), Vector2(rect_size.x - 8.0, rect_size.y - 8.0), x_color, line_width, true)
	draw_line(Vector2(rect_size.x - 8.0, 8.0), Vector2(8.0, rect_size.y - 8.0), x_color, line_width, true)
