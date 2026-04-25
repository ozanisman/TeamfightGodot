extends Node
## Stats-dashboard style overlay: role-colored 2px border, large font, no delay, follows pointer.
## Content is [ChampionCatalog] data (not sim output stats).

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")

# Match stats_dashboard.gd (keep in sync for visual parity).
const UI_TOOLTIP_MOUSE_OFF := Vector2(16, 20)
const UI_TOOLTIP_FONT_SIZE := 22
const UI_TOOLTIP_MIN_WIDTH := 520
const UI_TOOLTIP_CONTENT_MARGIN := 10
const COLOR_PANEL := Color(0.11, 0.11, 0.149, 1.0)
const COLOR_TEXT := Color(0.9, 0.9, 0.9, 1.0)
const COLOR_SUBTLE := Color(0.71, 0.71, 0.75, 1.0)

const ROLE_COLORS: Dictionary = {
	"tank": Color8(204, 51, 51),
	"fighter": Color8(210, 105, 30),
	"assassin": Color8(153, 50, 204),
	"marksman": Color8(34, 139, 34),
	"mage": Color8(76, 153, 204),
	"support": Color8(218, 165, 32),
}

var _ui_parent: Control
var _tt_style: StyleBoxFlat
var _tt_panel: PanelContainer
var _tt_rich: RichTextLabel
var _active_hero: StringName = StringName()


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_INHERIT


func setup(ui_layer: Control) -> void:
	_ui_parent = ui_layer
	_tt_style = StyleBoxFlat.new()
	_tt_style.bg_color = COLOR_PANEL
	_tt_style.set_border_width_all(2)
	_tt_style.border_color = COLOR_SUBTLE
	_tt_style.set_corner_radius_all(4)
	_tt_style.set_content_margin_all(UI_TOOLTIP_CONTENT_MARGIN)
	_tt_panel = PanelContainer.new()
	_tt_panel.name = "ChampionCatalogTooltip"
	_tt_panel.visible = false
	_tt_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_tt_panel.z_index = 200
	_tt_panel.add_theme_stylebox_override("panel", _tt_style)
	_tt_rich = RichTextLabel.new()
	_tt_rich.bbcode_enabled = true
	_tt_rich.scroll_active = false
	_tt_rich.fit_content = true
	_tt_rich.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_tt_rich.add_theme_color_override("default_color", COLOR_TEXT)
	_tt_rich.add_theme_font_size_override("normal_font_size", UI_TOOLTIP_FONT_SIZE)
	_tt_rich.add_theme_font_size_override("bold_font_size", UI_TOOLTIP_FONT_SIZE)
	_tt_rich.add_theme_font_size_override("italics_font_size", UI_TOOLTIP_FONT_SIZE)
	_tt_rich.add_theme_font_size_override("bold_italics_font_size", UI_TOOLTIP_FONT_SIZE)
	_tt_rich.add_theme_font_size_override("mono_font_size", UI_TOOLTIP_FONT_SIZE)
	_tt_rich.custom_minimum_size.x = UI_TOOLTIP_MIN_WIDTH
	_tt_panel.add_child(_tt_rich)
	_ui_parent.add_child(_tt_panel)


func register_source(ctl: Control, hero_id: StringName) -> void:
	if not is_instance_valid(ctl) or not is_instance_valid(self):
		return
	ctl.set_meta(&"champ_tt_hero", hero_id)
	if bool(ctl.get_meta(&"champ_tt_wired", false)):
		return
	ctl.set_meta(&"champ_tt_wired", true)
	ctl.mouse_entered.connect(_on_tt_enter.bind(ctl))
	ctl.mouse_exited.connect(_on_tt_exit.bind(ctl))
	ctl.gui_input.connect(_on_tt_motion.bind(ctl))


func _on_tt_enter(ctl: Control) -> void:
	var hero_id: StringName = ctl.get_meta(&"champ_tt_hero", StringName()) as StringName
	_active_hero = hero_id
	_show_for_hero(hero_id)
	_position_panel()


func _on_tt_exit(_ctl: Control) -> void:
	_active_hero = StringName()
	_hide()


func _on_tt_motion(event: InputEvent, _ctl: Control) -> void:
	if not _tt_panel.visible:
		return
	if event is InputEventMouseMotion:
		_position_panel()


func _position_panel() -> void:
	if _tt_panel == null or not _tt_panel.visible or get_viewport() == null:
		return
	if _ui_parent == null or not is_instance_valid(_ui_parent):
		return
	var vp: Rect2 = get_viewport().get_visible_rect()
	var sz: Vector2 = _tt_panel.get_combined_minimum_size()
	var g: Vector2 = _ui_parent.get_global_mouse_position() + UI_TOOLTIP_MOUSE_OFF
	g.x = clampf(g.x, vp.position.x + 4.0, vp.end.x - sz.x - 4.0)
	g.y = clampf(g.y, vp.position.y + 4.0, vp.end.y - sz.y - 4.0)
	_tt_panel.global_position = g


func _show_for_hero(hero_id: StringName) -> void:
	_tt_rich.text = _build_champion_bbcode(hero_id)
	_tt_style.border_color = _border_color_for_hero(hero_id)
	_tt_panel.visible = true
	_tt_panel.reset_size()
	_position_panel()


func _hide() -> void:
	if _tt_panel != null:
		_tt_panel.visible = false


func _border_color_for_hero(hero_id: StringName) -> Color:
	var ch: Variant = ChampionCatalogScript.get_champion(hero_id)
	if ch == null:
		return COLOR_SUBTLE
	var d: Dictionary = ch.to_dict()
	var st: Dictionary = d.get("stats", {})
	var rk: String = str(st.get("role", "")).to_lower()
	return ROLE_COLORS.get(rk, COLOR_SUBTLE) as Color


static func _escape_bbcode_plain(s: String) -> String:
	return str(s).replace("[", "[lb]").replace("]", "[rb]")


## BBCode: title line in role color; rest escaped plain.
func _build_champion_bbcode(hero_id: StringName) -> String:
	var ch: Variant = ChampionCatalogScript.get_champion(hero_id)
	if ch == null:
		return ""
	var d: Dictionary = ch.to_dict()
	var st: Dictionary = d.get("stats", {})
	var name: String = str(st.get("name", hero_id))
	var role: String = str(st.get("role", "")).to_upper()
	var display_name: String = _escape_bbcode_plain(name)
	var role_s: String = _escape_bbcode_plain(role)
	var br_col: Color = _border_for_dict(st)
	var title_line: String = (
		"[color=%s]%s[/color]  (%s)" % [br_col.to_html(false), display_name, role_s]
	)
	var lines: PackedStringArray = PackedStringArray()
	lines.append(title_line)
	lines.append(_escape_bbcode_plain(str(d.get("description", ""))))
	lines.append(
		_escape_bbcode_plain(
			"HP %.0f | AD %.1f | AS %.2f | Range %.1f"
			% [
				float(st.get("max_hp", 0.0)),
				float(st.get("attack_damage", 0.0)),
				float(st.get("attack_speed", 0.0)),
				float(st.get("attack_range", 0.0)),
			]
		)
	)
	lines.append(_escape_bbcode_plain("Passive: %s" % str(d.get("passive_desc", ""))))
	lines.append(
		_escape_bbcode_plain("Ability (%ss): %s" % [str(st.get("ability_cd", 0.0)), str(d.get("ability_desc", ""))])
	)
	lines.append(
		_escape_bbcode_plain(
			"Ultimate (%ss): %s" % [str(st.get("ultimate_cd", 0.0)), str(d.get("ultimate_desc", ""))]
		)
	)
	return "\n".join(lines)


func _border_for_dict(st: Dictionary) -> Color:
	var rk: String = str(st.get("role", "")).to_lower()
	return ROLE_COLORS.get(rk, COLOR_TEXT) as Color
