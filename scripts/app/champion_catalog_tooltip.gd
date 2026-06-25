extends Node
## Stats-dashboard style overlay: role-colored 2px border, large font, no delay, follows pointer.
## Content is [ChampionCatalog] data (not sim output stats).

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const SatelliteTooltipManagerScript := preload("res://scripts/app/satellite_tooltip_manager.gd")
const SatelliteContextScript := preload("res://scripts/app/satellite_context.gd")
const ChampionSatelliteGeneratorScript := preload("res://scripts/app/champion_satellite_generator.gd")
const UiTokensScript := preload("res://scripts/ui/ui_tokens.gd")

# Match stats_dashboard.gd (keep in sync for visual parity).
const UI_TOOLTIP_MOUSE_OFF := Vector2(16, 20)
const UI_TOOLTIP_FONT_SIZE := 22
const UI_TOOLTIP_MIN_WIDTH := 580
const UI_TOOLTIP_CONTENT_MARGIN := 10
const STAT_DIFF_EPSILON := 0.01

var _ui_parent: Control
var _tt_style: StyleBoxFlat
var _tt_panel: PanelContainer
var _tt_rich: RichTextLabel
var _active_champion: StringName = StringName()
var _active_unit_data: Dictionary = {}
var _satellite_manager: SatelliteTooltipManager
var _active_satellite_specs: Array = []


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_INHERIT


func setup(ui_layer: Control) -> void:
	_ui_parent = ui_layer
	_tt_style = StyleBoxFlat.new()
	_tt_style.bg_color = UiTokensScript.COLOR_PANEL
	_tt_style.set_border_width_all(2)
	_tt_style.border_color = UiTokensScript.COLOR_SUBTLE
	_tt_style.set_corner_radius_all(4)
	_tt_style.set_content_margin_all(UI_TOOLTIP_CONTENT_MARGIN)
	_tt_panel = PanelContainer.new()
	_tt_panel.name = "ChampionCatalogTooltip"
	_tt_panel.visible = false
	_tt_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_tt_panel.z_index = UiTokensScript.Z_CATALOG_TOOLTIP
	_tt_rich = RichTextLabel.new()
	_tt_rich.bbcode_enabled = true
	_tt_rich.scroll_active = false
	_tt_rich.fit_content = true
	_tt_rich.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_tt_rich.custom_minimum_size.x = UI_TOOLTIP_MIN_WIDTH
	_tt_panel.add_child(_tt_rich)
	_ui_parent.add_child(_tt_panel)
	
	# Initialize satellite manager
	_satellite_manager = SatelliteTooltipManagerScript.new()
	add_child(_satellite_manager)
	_satellite_manager.setup(ui_layer)
	ChampionSatelliteGeneratorScript.ensure_initialized()




func register_source(ctl: Control, champion_id: StringName, unit_data: Dictionary = {}, satellite_specs: Array = []) -> void:
	if not is_instance_valid(ctl) or not is_instance_valid(self):
		return
	ctl.set_meta(&"champ_tt_champion", champion_id)
	ctl.set_meta(&"champ_tt_unit_data", unit_data)
	ctl.set_meta(&"champ_tt_satellite_specs", satellite_specs)
	if bool(ctl.get_meta(&"champ_tt_wired", false)):
		return
	ctl.set_meta(&"champ_tt_wired", true)
	ctl.mouse_entered.connect(_on_tt_enter.bind(ctl))
	ctl.mouse_exited.connect(_on_tt_exit.bind(ctl))
	ctl.gui_input.connect(_on_tt_motion.bind(ctl))


func _on_tt_enter(ctl: Control) -> void:
	var champion_id: StringName = ctl.get_meta(&"champ_tt_champion", StringName()) as StringName
	var unit_data: Dictionary = ctl.get_meta(&"champ_tt_unit_data", {}) as Dictionary
	var satellite_specs: Array = ctl.get_meta(&"champ_tt_satellite_specs", []) as Array
	_active_champion = champion_id
	_active_unit_data = unit_data
	
	# Auto-generate satellite specs if not provided
	if satellite_specs.is_empty():
		satellite_specs = ChampionSatelliteGeneratorScript.generate_satellites(champion_id, unit_data)
	
	_active_satellite_specs = satellite_specs
	_show_for_champion(champion_id, unit_data)
	_position_panel()


func _on_tt_exit(_ctl: Control) -> void:
	_active_champion = StringName()
	_active_satellite_specs.clear()
	_hide()


func _on_tt_motion(event: InputEvent, _ctl: Control) -> void:
	if not _tt_panel.visible:
		return
	if event is InputEventMouseMotion:
		_position_panel()
		if _satellite_manager != null:
			_satellite_manager.update_origin(_tt_panel.global_position)


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


func _show_for_champion(champion_id: StringName, unit_data: Dictionary = {}) -> void:
	_tt_rich.text = _build_champion_bbcode(champion_id, unit_data)
	_tt_style.border_color = _border_color_for_champion(champion_id)
	_tt_panel.add_theme_stylebox_override("panel", _tt_style)
	_tt_panel.visible = true
	_tt_panel.reset_size()
	_position_panel()
	
	# Show satellites
	if _satellite_manager != null and not _active_satellite_specs.is_empty():
		var context = _create_satellite_context(champion_id, unit_data)
		_satellite_manager.show_satellites(_active_satellite_specs, _tt_panel.global_position, context, _tt_panel.size)


func _hide() -> void:
	if _tt_panel != null:
		_tt_panel.visible = false
	if _satellite_manager != null:
		_satellite_manager.hide_satellites()


## Update tooltip content with fresh unit data (called every tick during simulation)
func update_with_unit_data(unit_data: Dictionary) -> void:
	if not _tt_panel.visible or _active_champion.is_empty():
		return
	_active_unit_data = unit_data
	_tt_rich.text = _build_champion_bbcode(_active_champion, _active_unit_data)
	_tt_panel.reset_size()
	_position_panel()
	
	# Update satellites
	if _satellite_manager != null and not _active_satellite_specs.is_empty():
		var context = _create_satellite_context(_active_champion, unit_data)
		_satellite_manager.update_satellites(context)


## Get the currently active champion (for tooltip updates)
func get_active_champion() -> StringName:
	return _active_champion


## Create a SatelliteContext for the current tooltip state.
func _create_satellite_context(champion_id: StringName, unit_data: Dictionary):
	var main_data: Dictionary = {
		&"champion_id": champion_id,
		&"unit_data": unit_data,
		&"tooltip_position": _tt_panel.global_position if _tt_panel else Vector2.ZERO
	}
	return SatelliteContextScript.new(main_data, {}, {})


func _border_color_for_champion(champion_id: StringName) -> Color:
	var ch: Variant = ChampionCatalogScript.get_champion(champion_id)
	if ch == null:
		return UiTokensScript.COLOR_SUBTLE
	var d: Dictionary = ch.to_dict()
	var st: Dictionary = d.get("stats", {})
	var rk: String = str(st.get("role", "")).to_lower()
	return SimConstantsScript.ROLE_COLORS.get(rk, UiTokensScript.COLOR_SUBTLE) as Color


static func _escape_bbcode_plain(s: String) -> String:
	return str(s).replace("[", "[lb]").replace("]", "[rb]")


## Color keywords in text using BBCode tags.
## Escapes text first, then applies coloring to keywords.
## Case-insensitive, matches full words containing keywords.
static func _color_keywords_in_text(text: String) -> String:
	var escaped: String = _escape_bbcode_plain(text)
	var result: String = escaped
	
	for keyword in SimConstants.EFFECT_METADATA:
		var metadata: Dictionary = SimConstants.EFFECT_METADATA[keyword]
		var color: String = metadata.get("color", "#ffffff")
		# Match word boundaries with case-insensitive flag
		var regex: RegEx = RegEx.new()
		regex.compile("(?i)\\b([a-zA-Z]*%s[a-zA-Z]*)\\b" % keyword)
		result = regex.sub(result, "[color=%s]$1[/color]" % color, true)
	
	return result


## BBCode: title line in role color; rest escaped plain.
func _build_champion_bbcode(champion_id: StringName, unit_data: Dictionary = {}) -> String:
	var ch: Variant = ChampionCatalogScript.get_champion(champion_id)
	if ch == null:
		return ""
	var d: Dictionary = ch.to_dict()
	var st: Dictionary
	var base_stats: Dictionary
	
	# Always get base stats for comparison
	base_stats = ChampionCatalogScript.get_effective_stats(champion_id)
	
	# Use unit data stats if provided (in-game with modifiers), otherwise use catalog stats (draft phase)
	if not unit_data.is_empty():
		st = _build_effective_stats_from_unit_data(unit_data, champion_id)
	else:
		st = base_stats
	
	var name: String = str(st.get("name", champion_id))
	var display_name: String = _escape_bbcode_plain(name)
	var br_col: Color = _border_for_dict(st)
	var title_line: String = (
		"[color=%s]%s[/color]" % [br_col.to_html(false), display_name]
	)
	var lines: PackedStringArray = PackedStringArray()
	lines.append(title_line)
	lines.append(_escape_bbcode_plain(str(d.get("description", ""))))
	lines.append("")  # Line break before stats
	
	# Core stats (higher is better)
	var hp_str: String = _format_stat_diff_higher_better(float(base_stats.get("max_hp", 0.0)), float(st.get("max_hp", 0.0)), "HP: %.0f")
	var ad_str: String = _format_stat_diff_higher_better(float(base_stats.get("attack_damage", 0.0)), float(st.get("attack_damage", 0.0)), "AD: %.0f")
	var as_str: String = _format_stat_diff_higher_better(float(base_stats.get("attack_speed", 0.0)), float(st.get("attack_speed", 0.0)), "AS: %.2f")
	var range_str: String = _format_stat_diff_higher_better(float(base_stats.get("attack_range", 0.0)), float(st.get("attack_range", 0.0)), "Range: %.1f")
	var ms_str: String = _format_stat_diff_higher_better(float(base_stats.get("move_speed", 0.0)), float(st.get("move_speed", 0.0)), "MS: %.1f")
	lines.append("%s | %s | %s | %s | %s" % [hp_str, ad_str, as_str, range_str, ms_str])
	
	# Defensive stats (higher is better)
	var armor_str: String = _format_stat_diff_higher_better(float(base_stats.get("armor", 0.0)), float(st.get("armor", 0.0)), "Armor: %.0f%%")
	var mr_str: String = _format_stat_diff_higher_better(float(base_stats.get("magic_resist", 0.0)), float(st.get("magic_resist", 0.0)), "MR: %.0f%%")
	var tenacity_str: String = _format_stat_diff_higher_better(float(base_stats.get("tenacity", 0.0)) * 100, float(st.get("tenacity", 0.0)) * 100, "Tenacity: %.0f%%")
	var lifesteal_str: String = _format_stat_diff_higher_better(float(base_stats.get("life_steal", 0.0)) * 100, float(st.get("life_steal", 0.0)) * 100, "Lifesteal: %.0f%%")
	lines.append("%s | %s | %s | %s" % [armor_str, mr_str, tenacity_str, lifesteal_str])
	
	lines.append("")  # Line break before passive
	lines.append("%s (passive): %s" % [_escape_bbcode_plain(str(d.get("passive_name", "Passive"))), _color_keywords_in_text(str(d.get("passive_desc", "")))])
	lines.append("")  # Line break before ability
	
	# Ability cooldown (lower is better)
	var cd_str: String = _format_stat_diff_lower_better(float(base_stats.get("ability_cd", 0.0)), float(st.get("ability_cd", 0.0)), "%.1fs")
	lines.append("%s (%s): %s" % [_escape_bbcode_plain(str(d.get("ability_name", "Ability"))), cd_str, _color_keywords_in_text(str(d.get("ability_desc", "")))])
	
	lines.append("")  # Line break before ultimate
	
	# Mana cost (lower is better for cost)
	var mana_str: String = _format_stat_diff_lower_better(float(base_stats.get("mana_cost", 0.0)), float(st.get("mana_cost", 0.0)), "%.0f")
	lines.append("%s (%s mana): %s" % [_escape_bbcode_plain(str(d.get("ultimate_name", "Ultimate"))), mana_str, _color_keywords_in_text(str(d.get("ultimate_desc", "")))])
	
	return "\n".join(lines)


## Build effective stats dictionary from unit data (in-game with modifiers)
func _build_effective_stats_from_unit_data(unit_data: Dictionary, champion_id: StringName) -> Dictionary:
	var catalog_stats: Dictionary = ChampionCatalogScript.get_effective_stats(champion_id)
	var effective_stats: Dictionary = catalog_stats.duplicate(true)
	
	# Override with effective stats from unit data if available
	if unit_data.has("max_hp"):
		effective_stats["max_hp"] = float(unit_data["max_hp"])
	if unit_data.has("attack_damage"):
		effective_stats["attack_damage"] = float(unit_data["attack_damage"])
	if unit_data.has("attack_range"):
		effective_stats["attack_range"] = float(unit_data["attack_range"])
	if unit_data.has("attack_speed"):
		effective_stats["attack_speed"] = float(unit_data["attack_speed"])
	if unit_data.has("move_speed"):
		effective_stats["move_speed"] = float(unit_data["move_speed"])
	if unit_data.has("armor"):
		effective_stats["armor"] = float(unit_data["armor"])
	if unit_data.has("magic_resist"):
		effective_stats["magic_resist"] = float(unit_data["magic_resist"])
	if unit_data.has("tenacity"):
		effective_stats["tenacity"] = float(unit_data["tenacity"])
	if unit_data.has("life_steal"):
		effective_stats["life_steal"] = float(unit_data["life_steal"])
	if unit_data.has("mana_cost"):
		effective_stats["mana_cost"] = float(unit_data["mana_cost"])
	if unit_data.has("ability_cd_max"):
		effective_stats["ability_cd"] = float(unit_data["ability_cd_max"])
	
	return effective_stats


func _border_for_dict(st: Dictionary) -> Color:
	var rk: String = str(st.get("role", "")).to_lower()
	return SimConstantsScript.ROLE_COLORS.get(rk, UiTokensScript.COLOR_TEXT) as Color


## Format stat diff where higher values are better (green for increase, red for decrease)
func _format_stat_diff_higher_better(base: float, effective: float, format_string: String) -> String:
	var diff: float = effective - base
	if diff > STAT_DIFF_EPSILON:
		return "[color=%s]%s[/color]" % [UiTokensScript.COLOR_STAT_BUFF.to_html(false), format_string % effective]
	elif diff < -STAT_DIFF_EPSILON:
		return "[color=%s]%s[/color]" % [UiTokensScript.COLOR_STAT_NERF.to_html(false), format_string % effective]
	else:
		return format_string % effective


## Format stat diff where lower values are better (green for decrease, red for increase)
func _format_stat_diff_lower_better(base: float, effective: float, format_string: String) -> String:
	var diff: float = effective - base
	if diff < -STAT_DIFF_EPSILON:
		return "[color=%s]%s[/color]" % [UiTokensScript.COLOR_STAT_BUFF.to_html(false), format_string % effective]
	elif diff > STAT_DIFF_EPSILON:
		return "[color=%s]%s[/color]" % [UiTokensScript.COLOR_STAT_NERF.to_html(false), format_string % effective]
	else:
		return format_string % effective
