class_name DraftChampionTile
extends Button

signal champion_pressed(champion_id: StringName)

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const Tokens := preload("res://scripts/ui/ui_tokens.gd")
const Styles := preload("res://scripts/ui/ui_styles.gd")

var champion_id: StringName
var _role_color: Color = Tokens.COLOR_BUTTON
var _is_taken: bool = false
var _is_banned: bool = false
var _team_owner: StringName = &""


func _ready() -> void:
	if not pressed.is_connected(_on_pressed):
		pressed.connect(_on_pressed)


func setup(
	p_champion_id: StringName,
	role_color: Color,
	is_taken: bool,
	is_banned: bool,
	team_owner: StringName,
	tile_size: int
) -> void:
	champion_id = p_champion_id
	_role_color = role_color
	_apply_square_size(tile_size)
	refresh_state(is_taken, is_banned, team_owner)


func refresh_state(is_taken: bool, is_banned: bool, team_owner: StringName) -> void:
	_is_taken = is_taken
	_is_banned = is_banned
	_team_owner = team_owner
	text = _champion_display_name(champion_id)
	add_theme_font_size_override("font_size", Tokens.DRAFT_CHAMPION_FONT_SIZE_PX)
	add_theme_color_override("font_color", Tokens.COLOR_TEXT)
	text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	modulate = Color.WHITE

	if _is_taken:
		var dimmed_color := Color(_role_color.r * 0.5, _role_color.g * 0.5, _role_color.b * 0.5, _role_color.a)
		var border_color := Tokens.COLOR_SECTION_BORDER
		if _team_owner == &"player":
			border_color = Tokens.COLOR_PLAYER
		elif _team_owner == &"enemy":
			border_color = Tokens.COLOR_ENEMY
		_apply_style(Styles.bordered(dimmed_color, border_color, 3))
		disabled = true
	else:
		_apply_style(Styles.solid(_role_color))
		disabled = false
	queue_redraw()


func _apply_square_size(tile_size: int) -> void:
	var square := Vector2(tile_size, tile_size)
	custom_minimum_size = square
	size = square
	size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
	size_flags_vertical = Control.SIZE_SHRINK_BEGIN


func _apply_style(style: StyleBoxFlat) -> void:
	add_theme_stylebox_override("normal", style)
	add_theme_stylebox_override("hover", style)
	add_theme_stylebox_override("pressed", style)
	add_theme_stylebox_override("disabled", style)


func _champion_display_name(p_champion_id: StringName) -> String:
	var champion: Variant = ChampionCatalogScript.get_champion(p_champion_id)
	if champion == null:
		return String(p_champion_id)
	var champion_dict: Dictionary = champion.to_dict()
	var stats_dict: Dictionary = champion_dict.get("stats", {})
	return String(stats_dict.get("name", String(p_champion_id)))


func _on_pressed() -> void:
	champion_pressed.emit(champion_id)


func _draw() -> void:
	if not _is_banned:
		return
	var line_color := Color(0.5, 0.5, 0.5)
	var line_width := 4.0
	var inset := 4.0
	draw_line(Vector2(inset, inset), Vector2(size.x - inset, size.y - inset), line_color, line_width)
	draw_line(Vector2(size.x - inset, inset), Vector2(inset, size.y - inset), line_color, line_width)
