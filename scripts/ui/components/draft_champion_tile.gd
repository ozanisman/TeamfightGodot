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
	add_theme_font_size_override("font_size", Tokens.DRAFT_CHAMPION_FONT_SIZE_PX)
	add_theme_color_override("font_color", Color.WHITE)
	add_theme_color_override("font_disabled_color", Color.WHITE)
	add_theme_color_override("font_outline_color", Color.BLACK)
	add_theme_constant_override("outline_size", 1)
	text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	modulate = Color.WHITE


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

	if _is_banned:
		var gray := _role_color.get_luminance()
		var dimmed_color := Color(gray, gray, gray, _role_color.a * 0.5)
		var border_color := Color(gray, gray, gray, 0.50)
		_apply_style(Styles.bordered(dimmed_color, border_color, 6))
		add_theme_color_override("font_color", Color(0.5, 0.5, 0.5))
		add_theme_color_override("font_disabled_color", Color(0.5, 0.5, 0.5))
		disabled = true
	elif _is_taken:
		var gray := _role_color.get_luminance()
		var pastel_bg := Color(gray, gray, gray, _role_color.a * 0.7)
		var border_color := Color(gray, gray, gray, 0.8)
		if _team_owner == &"player":
			border_color = Tokens.COLOR_PLAYER
		elif _team_owner == &"enemy":
			border_color = Tokens.COLOR_ENEMY
		_apply_style(Styles.bordered(pastel_bg, border_color, 6))
		add_theme_color_override("font_color", Color.WHITE)
		add_theme_color_override("font_disabled_color", Color.WHITE)
		disabled = true
	else:
		var pastel_bg := _get_fill_color(_role_color)
		var style := Styles.panel(pastel_bg, _role_color, 6)
		style.set_corner_radius_all(4)
		_apply_style(style)
		add_theme_color_override("font_color", Color.WHITE)
		add_theme_color_override("font_disabled_color", Color.WHITE)
		disabled = false
	queue_redraw()


func _apply_square_size(tile_size: int) -> void:
	var rect := Vector2(tile_size, tile_size * 3 / 4)
	custom_minimum_size = rect
	size = rect
	size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
	size_flags_vertical = Control.SIZE_SHRINK_BEGIN


func _get_fill_color(role_color: Color) -> Color:
	var h := role_color.h

	# Preserve more color identity.
	var s := clampf(role_color.s * 0.90, 0.52, 0.82)
	var v := clampf(role_color.v * 0.88, 0.56, 0.82)

	# Hue compensation:
	# yellows/golds get capped harder because they visually flare in dark mode.
	var yellow_bias := -0.06 * cos(TAU * (h - 0.16))
	v = clampf(v + yellow_bias, 0.52, 0.84)

	var fill := Color.from_hsv(h, s, v, role_color.a)

	# Instead of blending heavily into the background, blend lightly.
	# This keeps the color cleaner and less muddy.
	var bg := Color("#171821")
	fill = bg.lerp(fill, 0.90)

	# Soft luminance ceiling: prevents overexposed yellow/cyan cards.
	var max_lum := 0.30
	var lum := fill.get_luminance()

	if lum > max_lum:
		var excess := lum - max_lum
		fill = fill.darkened(clampf(excess * 1.4, 0.02, 0.16))

	return fill

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
	var line_color := Tokens.COLOR_SECTION_BORDER
	if _team_owner == &"player":
		line_color = Tokens.COLOR_PLAYER
	elif _team_owner == &"enemy":
		line_color = Tokens.COLOR_ENEMY
	line_color.a = 0.75
	var w := 5.0

	# Top-left to bottom-right
	draw_colored_polygon([
		Vector2(0, 0),
		Vector2(w, 0),
		Vector2(size.x, size.y - w),
		Vector2(size.x, size.y),
		Vector2(size.x - w, size.y),
		Vector2(0, w),
	], line_color)

	# Top-right to bottom-left
	draw_colored_polygon([
		Vector2(size.x, 0),
		Vector2(size.x, w),
		Vector2(w, size.y),
		Vector2(0, size.y),
		Vector2(0, size.y - w),
		Vector2(size.x - w, 0),
	], line_color)
