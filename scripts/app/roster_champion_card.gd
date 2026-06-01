class_name RosterChampionCard
extends PanelContainer
## One square tile in the side roster: name, K/D/A, HP and mana progress bars.

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")

const _NAME_NEUTRAL := Color(0.82, 0.84, 0.88, 1.0)

const COLOR_HP_BG := Color(0.18, 0.18, 0.2, 1.0)
const COLOR_HP_FILL := Color(0.31, 0.78, 0.31, 1.0)
const COLOR_MN_BG := Color(0.12, 0.14, 0.2, 1.0)
const COLOR_MN_FILL := Color(0.28, 0.48, 0.9, 1.0)
const MIN_SQUARE_PX: int = 40

## Bound snapshot id for in-place [method apply_unit_data] when roster order is stable.
var instance_id: int = 0

var _name_label: Label
var _kda_label: Label
var _respawn_label: Label
var _behavior_label: Label
var _ability_label: Label
var _ultimate_label: Label
var _hp_shield_bar: Control
var _mana_bar: ProgressBar
var _align_right: bool
var _team_is_player: bool
var _square_px: int = 0
var _inner: VBoxContainer
var _ud_cache: Dictionary = {}
## [param square_px] initial min edge length; [method set_square_size] reflows from column.
func setup(
	ud: Dictionary, p_align_right: bool, p_team_is_player: bool, square_px: int
) -> void:
	_align_right = p_align_right
	_team_is_player = p_team_is_player
	if square_px < 1:
		square_px = 96
	_square_px = square_px

	var panel_style := StyleBoxFlat.new()
	panel_style.bg_color = Color(0.11, 0.12, 0.16, 0.95)
	if p_team_is_player:
		panel_style.border_color = Color(0.35, 0.5, 0.95, 0.9)
	else:
		panel_style.border_color = Color(0.9, 0.38, 0.35, 0.9)
	panel_style.set_border_width_all(1)
	panel_style.set_corner_radius_all(3)
	add_theme_stylebox_override("panel", panel_style)
	custom_minimum_size = Vector2(square_px, square_px)
	size = custom_minimum_size

	# Single child of Panel: stack overlay (PanelContainer = one child only).
	var stack := Control.new()
	stack.set_anchors_preset(Control.PRESET_FULL_RECT)
	stack.offset_left = 0.0
	stack.offset_top = 0.0
	stack.offset_right = 0.0
	stack.offset_bottom = 0.0
	stack.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(stack)

	var margin := MarginContainer.new()
	margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	margin.add_theme_constant_override("margin_left", 6)
	margin.add_theme_constant_override("margin_top", 5)
	margin.add_theme_constant_override("margin_right", 6)
	margin.add_theme_constant_override("margin_bottom", 5)
	stack.add_child(margin)

	_inner = VBoxContainer.new()
	_inner.set_anchors_preset(Control.PRESET_FULL_RECT)
	_inner.add_theme_constant_override("separation", 2)
	# Tuned in [method apply_font_and_bar_scales] from [member _square_px].
	_inner.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_inner.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	margin.add_child(_inner)
	margin.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_inner.mouse_filter = Control.MOUSE_FILTER_IGNORE

	_name_label = Label.new()
	_name_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_name_label.max_lines_visible = 2
	_name_label.autowrap_mode = TextServer.AUTOWRAP_OFF
	_name_label.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	_name_label.set_h_size_flags(Control.SIZE_EXPAND_FILL)
	_inner.add_child(_name_label)

	_kda_label = Label.new()
	_kda_label.set_h_size_flags(Control.SIZE_EXPAND_FILL)
	_kda_label.add_theme_color_override("font_color", Color(0.78, 0.78, 0.8, 1.0))
	_inner.add_child(_kda_label)

	_behavior_label = Label.new()
	_behavior_label.visible = false
	_behavior_label.set_h_size_flags(Control.SIZE_EXPAND_FILL)
	_behavior_label.add_theme_color_override("font_color", Color(0.6, 0.8, 0.95, 1.0))
	_inner.add_child(_behavior_label)

	_ability_label = Label.new()
	_ability_label.visible = false
	_ability_label.set_h_size_flags(Control.SIZE_EXPAND_FILL)
	_inner.add_child(_ability_label)

	_ultimate_label = Label.new()
	_ultimate_label.visible = false
	_ultimate_label.set_h_size_flags(Control.SIZE_EXPAND_FILL)
	_inner.add_child(_ultimate_label)

	_respawn_label = Label.new()
	_respawn_label.visible = false
	_respawn_label.set_h_size_flags(Control.SIZE_EXPAND_FILL)
	_respawn_label.add_theme_color_override("font_color", Color(0.9, 0.72, 0.45, 1.0))
	_inner.add_child(_respawn_label)

	var spacer := Control.new()
	spacer.size_flags_vertical = Control.SIZE_EXPAND_FILL
	spacer.custom_minimum_size = Vector2(0, 0)
	_inner.add_child(spacer)

	_hp_shield_bar = _make_hp_shield_bar()
	_inner.add_child(_hp_shield_bar)
	_mana_bar = _make_bar(COLOR_MN_BG, COLOR_MN_FILL)
	_mana_bar.visible = false
	_inner.add_child(_mana_bar)
	_name_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_kda_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_respawn_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_ability_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_ultimate_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_hp_shield_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_mana_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	spacer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var _tt_catch := Control.new()
	_tt_catch.name = "RosterChampionTooltipCatcher"
	_tt_catch.set_anchors_preset(Control.PRESET_FULL_RECT)
	_tt_catch.offset_left = 0.0
	_tt_catch.offset_top = 0.0
	_tt_catch.offset_right = 0.0
	_tt_catch.offset_bottom = 0.0
	_tt_catch.mouse_filter = Control.MOUSE_FILTER_STOP
	_tt_catch.z_index = 20
	stack.add_child(_tt_catch)
	apply_unit_data(ud, square_px, true)
	if p_align_right:
		_name_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		_kda_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		_behavior_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		_ability_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		_ultimate_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		_respawn_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT


static func _role_color_for_archetype(archetype_key: String) -> Color:
	var arch: StringName = StringName(archetype_key)
	if String(arch).is_empty():
		return _NAME_NEUTRAL
	var ch: Variant = ChampionCatalogScript.get_champion(arch)
	if ch == null:
		return _NAME_NEUTRAL
	var d: Dictionary = ch.to_dict()
	var st: Dictionary = d.get("stats", {})
	var rk: String = str(st.get("role", "")).to_lower()
	return SimConstantsScript.ROLE_COLORS.get(rk, _NAME_NEUTRAL) as Color


func _make_bar(c_bg: Color, c_fill: Color) -> ProgressBar:
	var p := ProgressBar.new()
	p.min_value = 0.0
	p.max_value = 100.0
	p.value = 0.0
	p.show_percentage = false
	p.custom_minimum_size = Vector2(0, 6.0)
	p.size_flags_vertical = Control.SIZE_SHRINK_END
	p.set_h_size_flags(Control.SIZE_EXPAND_FILL)
	var st_bg := StyleBoxFlat.new()
	st_bg.bg_color = c_bg
	st_bg.set_content_margin_all(0)
	p.add_theme_stylebox_override("background", st_bg)
	var st_fill := StyleBoxFlat.new()
	st_fill.bg_color = c_fill
	p.add_theme_stylebox_override("fill", st_fill)
	return p


func _make_hp_shield_bar() -> Control:
	var c := Control.new()
	c.custom_minimum_size = Vector2(0, 6.0)
	c.size_flags_vertical = Control.SIZE_SHRINK_END
	c.set_h_size_flags(Control.SIZE_EXPAND_FILL)
	var st_bg := StyleBoxFlat.new()
	st_bg.bg_color = COLOR_HP_BG
	st_bg.set_content_margin_all(0)
	c.add_theme_stylebox_override("panel", st_bg)
	c.set_script(preload("res://scripts/app/hp_shield_bar.gd"))
	return c


func set_square_size(px: int) -> void:
	if px < MIN_SQUARE_PX:
		px = MIN_SQUARE_PX
	_square_px = px
	custom_minimum_size = Vector2(px, px)
	apply_font_and_bar_scales()


## Refresh labels and bar values from a snapshot [Dictionary].
func apply_unit_data(ud: Dictionary, square_px: int = 0, p_do_font: bool = false) -> void:
	_ud_cache = ud.duplicate(true)
	instance_id = int(ud.get("instance_id", 0))
	if square_px > 0:
		_square_px = maxi(square_px, MIN_SQUARE_PX)
	if p_do_font:
		apply_font_and_bar_scales()
	var nm: String = str(ud.get("archetype_id", "?"))
	_name_label.text = nm.capitalize() if not nm.begins_with("?") else nm
	var k: int = int(ud.get("kills", 0))
	var d: int = int(ud.get("deaths", 0))
	var a: int = int(ud.get("assists", 0))
	var st: String = str(ud.get("state", "ALIVE"))
	var kda_s: String = "%d / %d / %d" % [k, d, a]
	_kda_label.text = kda_s
	
	# Set behavior label based on unit state
	var behavior_text := ""
	if st == "DEAD":
		behavior_text = "Behavior: [DEAD]"
	elif float(ud.get("stun_remaining", 0.0)) > 0.0 or st == "STUNNED":
		behavior_text = "Behavior: [STUNNED]"
	elif float(ud.get("taunt_remaining", 0.0)) > 0.0:
		behavior_text = "Behavior: [TAUNTED]"
	elif st == "KITING":
		behavior_text = "Behavior: [KITING]"
	elif float(ud.get("casting_remaining", 0.0)) > 0.0:
		var ckind: String = str(ud.get("casting_kind", ""))
		if ckind.to_lower().contains("ult"):
			behavior_text = "Behavior: [ULTIMATE]"
		else:
			behavior_text = "Behavior: [ABILITY]"
	elif int(ud.get("target_id", 0)) > 0 and bool(ud.get("in_range", false)):
		behavior_text = "Behavior: [ATTACKING]"
	elif int(ud.get("target_id", 0)) > 0:
		behavior_text = "Behavior: [MOVING]"
	else:
		behavior_text = "Behavior: [WAITING]"
	
	if not behavior_text.is_empty():
		_behavior_label.text = behavior_text
		_behavior_label.visible = true
	else:
		_behavior_label.visible = false
	
	# Set ability label based on ability cooldown, casting, or channeling
	var ability_cd: float = float(ud.get("abi", 0.0))
	var casting_rem: float = float(ud.get("casting_remaining", 0.0))
	var casting_kind: String = str(ud.get("casting_kind", ""))
	var is_channeling: bool = bool(ud.get("is_channeling", false))
	var channel_rem: float = float(ud.get("channel_remaining_duration", 0.0))
	var channel_kind: String = str(ud.get("channel_action_kind", ""))
	var ability_text := ""
	if is_channeling and (channel_kind.to_lower().contains("ability") or channel_kind == "ability"):
		ability_text = "Ability: [CHANNELING %.1fs]" % channel_rem
		_ability_label.add_theme_color_override("font_color", Color(0.9, 0.8, 0.4, 1.0))
	elif casting_rem > 0.0 and (casting_kind.to_lower().contains("ability") or casting_kind == "ability"):
		ability_text = "Ability: [CASTING %.1fs]" % casting_rem
		_ability_label.add_theme_color_override("font_color", Color(0.9, 0.8, 0.4, 1.0))
	elif ability_cd <= 0.0:
		ability_text = "Ability: [READY]"
		_ability_label.add_theme_color_override("font_color", Color(0.4, 0.9, 0.4, 1.0))
	else:
		ability_text = "Ability: %.1fs" % ability_cd
		_ability_label.add_theme_color_override("font_color", Color(0.78, 0.78, 0.8, 1.0))
	
	if not ability_text.is_empty():
		_ability_label.text = ability_text
		_ability_label.visible = true
	else:
		_ability_label.visible = false
	
	# Set ultimate label based on mana, casting, or channeling
	var mana_cost: float = float(ud.get("mana_cost", 0.0))
	var mn: float = float(ud.get("mana", 0.0))
	var ultimate_text := ""
	if is_channeling and (channel_kind.to_lower().contains("ult") or channel_kind == "ultimate"):
		ultimate_text = "Ultimate: [CHANNELING %.1fs]" % channel_rem
		_ultimate_label.add_theme_color_override("font_color", Color(0.9, 0.8, 0.4, 1.0))
	elif casting_rem > 0.0 and casting_kind.to_lower().contains("ult"):
		ultimate_text = "Ultimate: [CASTING %.1fs]" % casting_rem
		_ultimate_label.add_theme_color_override("font_color", Color(0.9, 0.8, 0.4, 1.0))
	elif mana_cost > SimConstantsScript.EPSILON * 2.0:
		ultimate_text = "Ultimate: [%d/%d]" % [int(mn), int(mana_cost)]
		_ultimate_label.add_theme_color_override("font_color", Color(0.78, 0.78, 0.8, 1.0))
	
	if not ultimate_text.is_empty():
		_ultimate_label.text = ultimate_text
		_ultimate_label.visible = true
	else:
		_ultimate_label.visible = false
	
	var name_c: Color = _role_color_for_archetype(nm)
	if st == "DEAD" or not bool(ud.get("alive", true)):
		var t_rem: float = maxf(0.0, float(ud.get("respawn_timer", 0.0)))
		_respawn_label.visible = true
		_respawn_label.text = "Respawn: %.1fs" % t_rem
		_name_label.add_theme_color_override("font_color", name_c.darkened(0.45))
		_kda_label.add_theme_color_override("font_color", Color(0.55, 0.5, 0.5, 1.0))
		modulate = Color(0.75, 0.75, 0.8, 0.9)
	else:
		_respawn_label.visible = false
		_name_label.add_theme_color_override("font_color", name_c)
		_kda_label.add_theme_color_override("font_color", Color(0.78, 0.78, 0.8, 1.0))
		modulate = Color.WHITE
	var max_hp: float = maxf(1.0, float(ud.get("max_hp", 1.0)))
	var hp: float = float(ud.get("hp", 0.0))
	var shield: float = float(ud.get("shield", 0.0))
	if _hp_shield_bar != null and _hp_shield_bar.has_method("set_data"):
		_hp_shield_bar.set_data(max_hp, hp, shield)
	if mana_cost > SimConstantsScript.EPSILON * 2.0:
		_mana_bar.visible = true
		_mana_bar.max_value = mana_cost
		_mana_bar.value = clampf(mn, 0.0, mana_cost)
	else:
		_mana_bar.visible = false


func apply_font_and_bar_scales() -> void:
	if _name_label == null:
		return
	var s: float = float(_square_px)
	# Scales with tile; wider clamps so large side columns use readable type.
	var name_fs: int = int(clampf(s * 0.128, 10.0, 19.0))
	var kda_fs: int = int(clampf(s * 0.102, 9.0, 15.0))
	var behavior_fs: int = int(clampf(s * 0.094, 8.0, 14.0))
	var ability_fs: int = int(clampf(s * 0.094, 8.0, 14.0))
	var ultimate_fs: int = int(clampf(s * 0.094, 8.0, 14.0))
	var respawn_fs: int = int(clampf(s * 0.094, 8.0, 14.0))
	var bar_h: float = clampf(s * 0.082, 4.0, 12.0)
	var sep: int = int(clampf(s * 0.034, 2.0, 6.0))
	_inner.add_theme_constant_override("separation", sep)
	_name_label.add_theme_font_size_override("font_size", name_fs)
	_kda_label.add_theme_font_size_override("font_size", kda_fs)
	_behavior_label.add_theme_font_size_override("font_size", behavior_fs)
	_ability_label.add_theme_font_size_override("font_size", ability_fs)
	_ultimate_label.add_theme_font_size_override("font_size", ultimate_fs)
	_respawn_label.add_theme_font_size_override("font_size", respawn_fs)
	_hp_shield_bar.custom_minimum_size = Vector2(0, bar_h)
	_mana_bar.custom_minimum_size = Vector2(0, bar_h)
	if _align_right and _kda_label != null:
		_name_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		_kda_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		_behavior_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		_ability_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		_ultimate_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		_respawn_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
