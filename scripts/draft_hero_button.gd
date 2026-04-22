extends Button
class_name DraftHeroButton

const ROLE_COLORS := {
	"tank": Color(0.22, 0.50, 0.86),
	"fighter": Color(0.92, 0.48, 0.14),
	"assassin": Color(0.72, 0.24, 0.92),
	"marksman": Color(0.16, 0.74, 0.60),
	"mage": Color(0.18, 0.68, 0.96),
	"support": Color(0.82, 0.60, 0.22),
}

var hero_id: String = ""
var hero_data: Dictionary = {}
var pick_owner: String = ""
var base_button_style: StyleBoxFlat
var bold_button_font: Font
var regular_button_font: Font


func setup(new_hero_id: String, new_hero_data: Dictionary) -> void:
	hero_id = new_hero_id
	hero_data = new_hero_data.duplicate(true)
	text = String(hero_data.get("name", hero_id))
	var role := String(hero_data.get("role", ""))
	if base_button_style == null:
		var normal_style := get_theme_stylebox("normal")
		if normal_style is StyleBoxFlat:
			base_button_style = (normal_style as StyleBoxFlat).duplicate() as StyleBoxFlat
	var role_color: Color = ROLE_COLORS.get(role, Color(0.4, 0.4, 0.4)).darkened(0.14)
	var button_style := base_button_style.duplicate() as StyleBoxFlat if base_button_style != null else StyleBoxFlat.new()
	button_style.bg_color = role_color
	button_style.border_color = role_color.lightened(0.12)
	add_theme_stylebox_override("normal", button_style)
	add_theme_stylebox_override("hover", button_style.duplicate() as StyleBoxFlat)
	add_theme_stylebox_override("pressed", button_style.duplicate() as StyleBoxFlat)
	add_theme_stylebox_override("disabled", button_style.duplicate() as StyleBoxFlat)
	add_theme_stylebox_override("focus", button_style.duplicate() as StyleBoxFlat)
	_ensure_fonts()
	_update_font_state()


func set_pick_owner(owner: String) -> void:
	pick_owner = owner
	_update_font_state()
	queue_redraw()


func _draw() -> void:
	var rect := Rect2(Vector2.ZERO, size)
	if rect.size.x <= 0.0 or rect.size.y <= 0.0:
		return

	if pick_owner == "player" or pick_owner == "enemy":
		var team_color := Color(0.28, 0.58, 1.0, 0.90) if pick_owner == "player" else Color(1.0, 0.34, 0.34, 0.90)
		draw_rect(rect.grow(0.0), Color(0, 0, 0, 0.95), false, 10.0)
		draw_rect(rect.grow(-4.0), team_color, false, 6.0)
	elif pick_owner == "banned":
		var x_color := Color(0.95, 0.28, 0.28, 0.50)
		draw_line(Vector2(8.0, 8.0), Vector2(rect.size.x - 8.0, rect.size.y - 8.0), x_color, 5.0, true)
		draw_line(Vector2(rect.size.x - 8.0, 8.0), Vector2(8.0, rect.size.y - 8.0), x_color, 5.0, true)


func _ensure_fonts() -> void:
	if regular_button_font == null:
		regular_button_font = ThemeDB.fallback_font
	if bold_button_font == null and ThemeDB.fallback_font != null:
		var font_variation := FontVariation.new()
		font_variation.base_font = ThemeDB.fallback_font
		font_variation.variation_embolden = 0.35
		bold_button_font = font_variation


func _update_font_state() -> void:
	if pick_owner.is_empty():
		add_theme_font_override("font", bold_button_font if bold_button_font != null else ThemeDB.fallback_font)
	else:
		add_theme_font_override("font", regular_button_font if regular_button_font != null else ThemeDB.fallback_font)
