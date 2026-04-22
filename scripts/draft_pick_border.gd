extends Control
class_name DraftPickBorder

const TEAM_COLORS := {
	"player": Color(0.28, 0.58, 1.0, 0.85),
	"enemy": Color(1.0, 0.34, 0.34, 0.85),
}

var team: String = ""


func set_team(new_team: String) -> void:
	team = new_team
	visible = not team.is_empty()
	queue_redraw()


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	visible = false


func _draw() -> void:
	if not visible:
		return

	var rect_size := size
	if rect_size.x <= 0.0 or rect_size.y <= 0.0:
		return

	var border_color: Color = TEAM_COLORS.get(team, Color(0.8, 0.8, 0.8, 0.75))
	draw_rect(Rect2(Vector2(2.0, 2.0), rect_size - Vector2(4.0, 4.0)), border_color, false, 4.0)
