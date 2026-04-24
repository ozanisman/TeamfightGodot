extends Control

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const StatsDashboardLoaderScript := preload("res://scripts/tools/stats_dashboard_loader.gd")
const StatsDoughnutScript := preload("res://scripts/app/stats_doughnut.gd")
const StatsBarControlScript := preload("res://scripts/app/stats_bar_control.gd")

const COLOR_BG := Color(0.078, 0.078, 0.102, 1.0)
const COLOR_PANEL := Color(0.11, 0.11, 0.149, 1.0)
const COLOR_TEXT := Color(0.9, 0.9, 0.9, 1.0)
const COLOR_HIGHLIGHT := Color(0.47, 0.86, 0.55, 1.0)
const COLOR_SUBTLE := Color(0.71, 0.71, 0.75, 1.0)
const COLOR_RED := Color(0.88, 0.31, 0.31, 1.0)
const COLOR_GREEN := Color(0.47, 0.86, 0.55, 1.0)
const COLOR_BLUE := Color(0.275, 0.51, 1.0)
const COLOR_SECTION_BG := Color(0.133, 0.133, 0.18, 1.0)

const CI_SUPPORTED: Dictionary = {
	&"winRate": true,
	&"damage_dealt": true,
	&"damage_received": true,
	&"healing_done": true,
	&"damage_mitigated": true,
}

const ROLE_COLORS: Dictionary = {
	"tank": Color8(70, 130, 180),
	"fighter": Color8(210, 105, 30),
	"assassin": Color8(153, 50, 204),
	"marksman": Color8(34, 139, 34),
	"mage": Color8(0, 191, 255),
	"support": Color8(218, 165, 32),
}

const ROLE_SORT_ORDER: Dictionary = {
	"tank": 0,
	"fighter": 1,
	"assassin": 2,
	"support": 3,
	"mage": 4,
	"marksman": 5,
}

const METRICS: Array[Dictionary] = [
	{"label": "Win Rate", "key": &"winRate"},
	{"label": "Damage Dealt", "key": &"damage_dealt"},
	{"label": "Damage Taken", "key": &"damage_received"},
	{"label": "Mitigated by Resists", "key": &"damage_mitigated"},
	{"label": "Healing Done", "key": &"healing_done"},
	{"label": "Shielding Done", "key": &"shielding_done"},
	{"label": "KDA Ratio", "key": &"kda"},
	{"label": "CC (Stuns)", "key": &"stuns"},
	{"label": "Synergies", "key": &"synergy"},
]

const SORT_MODES: Array[Dictionary] = [
	{"label": "Metric Value", "key": &"value"},
	{"label": "Games Played", "key": &"games"},
	{"label": "Win Rate", "key": &"winRate"},
	{"label": "Role", "key": &"role"},
	{"label": "Alphabetical", "key": &"name"},
]

const VIEW_MODES: Array[Dictionary] = [
	{"label": "Champions", "key": &"champions"},
	{"label": "Roles", "key": &"roles"},
]

var _loader = StatsDashboardLoaderScript.new()
var _unit_roles: Dictionary = {}
var _stats_path: String = ""

var _current_size: int = 1
var _current_metric: StringName = &"winRate"
var _view_mode: StringName = &"champions"
var _current_sort: StringName = &"value"
var _absolute_colors: bool = true
var _use_ci: bool = true
var _search_text: String = ""
var _active_role_filters: Dictionary = {}
var _chart_uses_ci: bool = false

var _doughnut: Control
var _metric_option: OptionButton
var _view_option: OptionButton
var _sort_option: OptionButton
var _ci_button: Button
var _color_button: Button
var _search: LineEdit
var _role_grid: GridContainer
var _chart_vbox: VBoxContainer
var _title_label: Label
var _error_label: Label
var _team_button_group: ButtonGroup

@onready var _root_vb: VBoxContainer = $RootVBox


func _dim_color(col: Color, amount: float = 60.0 / 255.0) -> Color:
	return Color(
		maxf(0.0, col.r - amount),
		maxf(0.0, col.g - amount),
		maxf(0.0, col.b - amount),
		col.a
	)


func _stretch_control_to_parent_full_rect(c: Control) -> void:
	c.anchor_left = 0.0
	c.anchor_top = 0.0
	c.anchor_right = 1.0
	c.anchor_bottom = 1.0
	c.offset_left = 0.0
	c.offset_top = 0.0
	c.offset_right = 0.0
	c.offset_bottom = 0.0
	c.grow_horizontal = Control.GROW_DIRECTION_BOTH
	c.grow_vertical = Control.GROW_DIRECTION_BOTH


func _ready() -> void:
	_team_button_group = ButtonGroup.new()
	_build_unit_roles()
	if not _try_load_stats():
		_build_fatal_error_ui()
		return
	_build_ui()
	_wire_controls()
	_refresh_all()


func _build_unit_roles() -> void:
	var catalog: Dictionary = ChampionCatalogScript.build_catalog()
	for archetype in catalog.keys():
		var spec: Variant = catalog[archetype]
		_unit_roles[String(archetype)] = String(spec.stats.role)


func _try_load_stats() -> bool:
	var primary: String = "res://stats_output"
	if _loader.load_from_dir(primary) == OK:
		_stats_path = primary
		return true
	if _loader.load_from_dir("res://fixtures/stats_dashboard") == OK:
		_stats_path = "res://fixtures/stats_dashboard"
		return true
	push_error("StatsDashboard: no CSV bundle (need res://stats_output or fixtures)")
	return false


func _build_fatal_error_ui() -> void:
	var bg := ColorRect.new()
	_stretch_control_to_parent_full_rect(bg)
	bg.color = COLOR_BG
	add_child(bg)
	var lb := Label.new()
	_stretch_control_to_parent_full_rect(lb)
	lb.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	lb.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	lb.text = "Stats CSV bundle missing.\nAdd combat_stats.csv, hero_combinations.csv, role_stats.csv, summary_stats.csv\nunder res://stats_output (or run batch generator)."
	lb.add_theme_color_override("font_color", COLOR_TEXT)
	add_child(lb)


func _build_ui() -> void:
	var top := HBoxContainer.new()
	top.custom_minimum_size.y = 40
	var quit := Button.new()
	quit.text = "Quit"
	quit.size_flags_horizontal = Control.SIZE_SHRINK_END
	quit.pressed.connect(func(): get_tree().quit())
	top.add_child(Label.new()) # spacer
	top.add_child(quit)
	_root_vb.add_child(top)

	var split := HSplitContainer.new()
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	split.split_offset = 280
	_root_vb.add_child(split)

	var left_scroll := ScrollContainer.new()
	left_scroll.custom_minimum_size.x = 280
	left_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var left_vb := VBoxContainer.new()
	left_vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	left_scroll.add_child(left_vb)
	split.add_child(left_scroll)

	_error_label = Label.new()
	_error_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	left_vb.add_child(_error_label)

	_doughnut = StatsDoughnutScript.new()
	_doughnut.custom_minimum_size = Vector2(220, 140)
	left_vb.add_child(_doughnut)

	left_vb.add_child(_section_label("VIEW MODE"))
	_view_option = OptionButton.new()
	for vm in VIEW_MODES:
		_view_option.add_item(vm["label"])
	left_vb.add_child(_view_option)

	left_vb.add_child(_section_label("METRICS"))
	_metric_option = OptionButton.new()
	for m in METRICS:
		_metric_option.add_item(m["label"])
	left_vb.add_child(_metric_option)

	left_vb.add_child(_section_label("TEAM SIZE"))
	var team_row := HBoxContainer.new()
	var sizes: Array = Array(SimConstantsScript.SIMULATION_TEAM_SIZES)
	for sz in sizes:
		var b := Button.new()
		b.text = str(int(sz))
		b.toggle_mode = true
		b.button_group = _team_button_group
		b.pressed.connect(_on_team_size_pressed.bind(int(sz)))
		team_row.add_child(b)
		if int(sz) == _current_size:
			b.button_pressed = true
	left_vb.add_child(team_row)

	left_vb.add_child(_section_label("DATA MODE"))
	_ci_button = Button.new()
	left_vb.add_child(_ci_button)

	left_vb.add_child(_section_label("SORT BY"))
	_sort_option = OptionButton.new()
	for sm in SORT_MODES:
		_sort_option.add_item(sm["label"])
	left_vb.add_child(_sort_option)

	left_vb.add_child(_section_label("COLOR SCALE"))
	_color_button = Button.new()
	left_vb.add_child(_color_button)

	left_vb.add_child(_section_label("FILTER"))
	_search = LineEdit.new()
	_search.placeholder_text = "Search hero..."
	left_vb.add_child(_search)

	left_vb.add_child(_section_label("ROLES"))
	_role_grid = GridContainer.new()
	_role_grid.columns = 2
	for role_key in ROLE_COLORS.keys():
		var tb := Button.new()
		tb.toggle_mode = true
		tb.text = str(role_key).to_upper()
		var rk := str(role_key)
		tb.toggled.connect(func(on: bool): _on_role_toggled(rk, on))
		_role_grid.add_child(tb)
	left_vb.add_child(_role_grid)

	var right_vb := VBoxContainer.new()
	right_vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	right_vb.size_flags_stretch_ratio = 1.0
	split.add_child(right_vb)

	_title_label = Label.new()
	_title_label.add_theme_color_override("font_color", COLOR_TEXT)
	right_vb.add_child(_title_label)

	var header := HBoxContainer.new()
	var h_games := Label.new()
	h_games.text = "Games"
	h_games.custom_minimum_size.x = 72
	h_games.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	var h_val := Label.new()
	h_val.text = "Value"
	h_val.custom_minimum_size.x = 72
	h_val.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	header.add_child(Control.new()) # name + bar spacer
	header.add_child(h_games)
	header.add_child(h_val)
	right_vb.add_child(header)

	var chart_scroll := ScrollContainer.new()
	chart_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_chart_vbox = VBoxContainer.new()
	_chart_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	chart_scroll.add_child(_chart_vbox)
	right_vb.add_child(chart_scroll)


func _section_label(text: String) -> Label:
	var lb := Label.new()
	lb.text = text
	lb.add_theme_color_override("font_color", COLOR_HIGHLIGHT)
	return lb


func _wire_controls() -> void:
	_metric_option.item_selected.connect(func(_i): _sync_metric_from_ui(); _refresh_all())
	_view_option.item_selected.connect(func(_i): _sync_view_from_ui(); _refresh_all())
	_sort_option.item_selected.connect(func(_i): _sync_sort_from_ui(); _refresh_all())
	_search.text_changed.connect(func(t): _search_text = t; _refresh_chart())
	_search.text_submitted.connect(func(_t): _refresh_chart())
	_ci_button.pressed.connect(func():
		if _loader.ci_stats.is_empty():
			return
		_use_ci = not _use_ci
		_refresh_all()
	)
	_color_button.pressed.connect(func():
		_absolute_colors = not _absolute_colors
		_refresh_chart()
	)


func _sync_metric_from_ui() -> void:
	var i: int = _metric_option.selected
	if i >= 0 and i < METRICS.size():
		_current_metric = METRICS[i]["key"]


func _sync_view_from_ui() -> void:
	var i: int = _view_option.selected
	if i >= 0 and i < VIEW_MODES.size():
		_view_mode = VIEW_MODES[i]["key"]


func _sync_sort_from_ui() -> void:
	var i: int = _sort_option.selected
	if i >= 0 and i < SORT_MODES.size():
		_current_sort = SORT_MODES[i]["key"]


func _on_team_size_pressed(sz: int) -> void:
	_current_size = sz
	_refresh_all()


func _on_role_toggled(role: String, on: bool) -> void:
	if on:
		_active_role_filters[role] = true
	else:
		_active_role_filters.erase(role)
	_refresh_chart()


func _refresh_all() -> void:
	_sync_metric_from_ui()
	_sync_view_from_ui()
	_sync_sort_from_ui()
	_update_sidebar_buttons()
	_refresh_chart()


func _update_sidebar_buttons() -> void:
	var has_ci: bool = not _loader.ci_stats.is_empty()
	_ci_button.disabled = not has_ci
	_ci_button.text = "Mode: CI" if _use_ci else "Mode: Regular"
	_color_button.text = "Mode: Absolute" if _absolute_colors else "Mode: Relative"
	if not _stats_path.is_empty():
		_error_label.text = "Data: %s" % _stats_path
		_error_label.add_theme_color_override("font_color", COLOR_SUBTLE)


func _ci_mode_active() -> bool:
	if not _use_ci:
		return false
	if _view_mode != &"champions":
		return false
	if not CI_SUPPORTED.has(_current_metric):
		return false
	if not _loader.ci_stats.has(_current_size):
		return false
	return not _loader.ci_stats[_current_size].is_empty()


func _display_name(key: String, synergy: bool) -> String:
	if synergy:
		var parts: PackedStringArray = key.split("\u001e")
		var s := ""
		for i in parts.size():
			if i > 0:
				s += " + "
			s += parts[i]
		return s
	return key.capitalize()


func _refresh_chart() -> void:
	for c in _chart_vbox.get_children():
		c.queue_free()

	var is_synergy: bool = _current_metric == &"synergy"
	var use_ci: bool = _ci_mode_active()
	_chart_uses_ci = use_ci

	if is_synergy and _current_size == 1:
		var lb := Label.new()
		lb.text = "Synergies N/A for 1v1"
		lb.add_theme_color_override("font_color", COLOR_TEXT)
		_chart_vbox.add_child(lb)
		_title_label.text = ""
		return

	var data_context: Dictionary = {}
	if use_ci:
		data_context = _loader.ci_stats.get(_current_size, {})
	elif is_synergy:
		data_context = _loader.all_stats.get(_current_size, {}).get("combinations", {})
	elif _view_mode == &"roles":
		data_context = _loader.all_stats.get(_current_size, {}).get("roles", {})
	else:
		data_context = _loader.all_stats.get(_current_size, {}).get("units", {})

	if data_context.is_empty():
		var lb2 := Label.new()
		lb2.text = "No Data for this team size"
		lb2.add_theme_color_override("font_color", COLOR_TEXT)
		_chart_vbox.add_child(lb2)
		_title_label.text = ""
		return

	var display_data: Dictionary = data_context.duplicate(true)
	if not _search_text.is_empty():
		var q: String = _search_text.to_lower()
		var filtered: Dictionary = {}
		for k in display_data.keys():
			if is_synergy:
				var parts: PackedStringArray = str(k).split("\u001e")
				var match_any := false
				for p in parts:
					if q in str(p).to_lower():
						match_any = true
						break
				if match_any:
					filtered[k] = display_data[k]
			else:
				if q in str(k).to_lower():
					filtered[k] = display_data[k]
		display_data = filtered

	if not _active_role_filters.is_empty() and not is_synergy and _view_mode == &"champions":
		var filtered2: Dictionary = {}
		for hero in display_data.keys():
			var role: String = str(_unit_roles.get(String(hero), ""))
			if _active_role_filters.has(role):
				filtered2[hero] = display_data[hero]
		display_data = filtered2

	var keys: Array = display_data.keys()
	if keys.is_empty():
		return

	keys.sort_custom(
		func(a, b): return _sort_less(String(a), String(b), display_data, is_synergy, use_ci)
	)

	var vals: Array[float] = []
	for k in keys:
		var u_data: Dictionary = display_data[k]
		var v: float = (
			StatsDashboardLoaderScript.get_display_val_ci(u_data, _current_metric)
			if use_ci
			else StatsDashboardLoaderScript.get_display_val(u_data, _current_metric)
		)
		vals.append(v)

	var max_val: float = vals[0]
	var min_val: float = vals[0]
	for v in vals:
		max_val = maxf(max_val, v)
		min_val = minf(min_val, v)
	if max_val == 0.0:
		max_val = 1.0
	var bar_scale_max: float = 1.0 if (_current_metric == &"winRate" or is_synergy) else max_val
	var val_range: float = max_val - min_val

	var metric_label: String = ""
	for m in METRICS:
		if m["key"] == _current_metric:
			metric_label = m["label"]
			break
	_title_label.text = "%s Averages - %dv%d%s" % [
		metric_label,
		_current_size,
		_current_size,
		" (CI)" if use_ci else "",
	]

	_update_doughnut()

	var label_width := 520.0 if is_synergy else 200.0
	var row_idx := 0
	for k in keys:
		var key_s: String = str(k)
		var u_data: Dictionary = display_data[k]
		var val: float = (
			StatsDashboardLoaderScript.get_display_val_ci(u_data, _current_metric)
			if use_ci
			else StatsDashboardLoaderScript.get_display_val(u_data, _current_metric)
		)
		var row := PanelContainer.new()
		var sb_row := StyleBoxFlat.new()
		sb_row.bg_color = COLOR_SECTION_BG if row_idx % 2 == 0 else Color(0, 0, 0, 0)
		sb_row.set_content_margin_all(4)
		row.add_theme_stylebox_override("panel", sb_row)
		row.tooltip_text = _build_tooltip(key_s, u_data, use_ci, is_synergy)

		var hb := HBoxContainer.new()
		row.add_child(hb)

		var name_w := Control.new()
		name_w.custom_minimum_size.x = label_width
		var name_lb := Label.new()
		name_lb.text = _display_name(key_s, is_synergy)
		name_lb.autowrap_mode = TextServer.AUTOWRAP_OFF
		if not is_synergy and _view_mode == &"champions":
			var role: String = str(_unit_roles.get(key_s, ""))
			var rc: Color = ROLE_COLORS.get(role, COLOR_TEXT) as Color
			name_lb.add_theme_color_override("font_color", rc)
			var marker := ColorRect.new()
			marker.custom_minimum_size = Vector2(8, 8)
			marker.color = rc
			var name_h := HBoxContainer.new()
			name_h.add_child(marker)
			name_h.add_child(name_lb)
			name_w.add_child(name_h)
		else:
			name_lb.add_theme_color_override("font_color", COLOR_TEXT)
			name_w.add_child(name_lb)
		hb.add_child(name_w)

		var bar_holder := StatsBarControlScript.new()
		bar_holder.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		bar_holder.custom_minimum_size = Vector2(120, 28)
		var fill_ratio: float = clampf(val / bar_scale_max, 0.0, 1.0)
		var ratio_color: float
		if _absolute_colors and (_current_metric == &"winRate" or is_synergy):
			ratio_color = val
		else:
			ratio_color = (val - min_val) / val_range if val_range > 0.0 else (0.5 if _absolute_colors else 0.0)
		var bar_col := _blend_bar_color(ratio_color)
		var ci_lo_r: float = -1.0
		var ci_hi_r: float = -1.0
		if use_ci and CI_SUPPORTED.has(_current_metric):
			var ci_bounds: Variant = u_data.get("ci", {}).get(_current_metric, null)
			if ci_bounds is Array and (ci_bounds as Array).size() >= 2:
				var lo: float = maxf(0.0, float(ci_bounds[0]))
				var hi: float = maxf(lo, float(ci_bounds[1]))
				ci_lo_r = clampf(lo / bar_scale_max, 0.0, 1.0)
				ci_hi_r = clampf(hi / bar_scale_max, 0.0, 1.0)
		bar_holder.set_visual(fill_ratio, bar_col, ci_lo_r, ci_hi_r)
		hb.add_child(bar_holder)

		var games_lb := Label.new()
		games_lb.custom_minimum_size.x = 72
		games_lb.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		var count: int = (
			maxi(1, int(u_data.get("samples", 0)))
			if use_ci
			else StatsDashboardLoaderScript.get_count(u_data)
		)
		games_lb.text = str(count)
		games_lb.add_theme_color_override("font_color", COLOR_SUBTLE)
		hb.add_child(games_lb)

		var val_lb := Label.new()
		val_lb.custom_minimum_size.x = 72
		val_lb.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		if _current_metric == &"winRate" or is_synergy:
			val_lb.text = "%.1f%%" % (val * 100.0)
		elif _current_metric == &"kda":
			val_lb.text = "%.2f" % val
		else:
			val_lb.text = "%.0f" % val
		val_lb.add_theme_color_override("font_color", COLOR_TEXT)
		hb.add_child(val_lb)

		_chart_vbox.add_child(row)
		row_idx += 1


func _blend_bar_color(ratio: float) -> Color:
	var r: float = clampf(ratio, 0.0, 1.0)
	if r <= 0.5:
		var sub: float = r * 2.0
		return COLOR_RED.lerp(COLOR_GREEN, sub)
	var sub2: float = (r - 0.5) * 2.0
	return COLOR_GREEN.lerp(COLOR_BLUE, sub2)


func _sort_less(
	a: String,
	b: String,
	display_data: Dictionary,
	is_synergy: bool,
	use_ci: bool
) -> bool:
	var rev: bool = _current_sort != &"name"
	if _current_sort == &"name":
		var na: String = _display_name(a, is_synergy).to_lower()
		var nb: String = _display_name(b, is_synergy).to_lower()
		return na < nb
	if _current_sort == &"role":
		if is_synergy:
			var sa: String = _display_name(a, is_synergy).to_lower()
			var sb: String = _display_name(b, is_synergy).to_lower()
			return sa > sb if rev else sa < sb
		if _view_mode == &"roles":
			var oa: int = int(ROLE_SORT_ORDER.get(a, 99))
			var ob: int = int(ROLE_SORT_ORDER.get(b, 99))
			if oa != ob:
				return oa > ob if rev else oa < ob
			return a.to_lower() > b.to_lower() if rev else a.to_lower() < b.to_lower()
		var ra: String = str(_unit_roles.get(a, ""))
		var rb: String = str(_unit_roles.get(b, ""))
		var oa2: int = int(ROLE_SORT_ORDER.get(ra, 99))
		var ob2: int = int(ROLE_SORT_ORDER.get(rb, 99))
		if oa2 != ob2:
			return oa2 > ob2 if rev else oa2 < ob2
		var na2: String = a.to_lower()
		var nb2: String = b.to_lower()
		return na2 > nb2 if rev else na2 < nb2
	var ua: Dictionary = display_data[a]
	var ub: Dictionary = display_data[b]
	if _current_sort == &"games":
		var ga: float = float(
			maxi(1, int(ua.get("samples", 0))) if use_ci else StatsDashboardLoaderScript.get_count(ua)
		)
		var gb: float = float(
			maxi(1, int(ub.get("samples", 0))) if use_ci else StatsDashboardLoaderScript.get_count(ub)
		)
		if ga != gb:
			return ga > gb if rev else ga < gb
		return a.to_lower() > b.to_lower() if rev else a.to_lower() < b.to_lower()
	if _current_sort == &"winRate":
		var wa: float = float(ua.get("winRate", 0.0))
		var wb: float = float(ub.get("winRate", 0.0))
		if wa != wb:
			return wa > wb if rev else wa < wb
		return a.to_lower() > b.to_lower() if rev else a.to_lower() < b.to_lower()
	var va: float = (
		StatsDashboardLoaderScript.get_display_val_ci(ua, _current_metric)
		if use_ci
		else StatsDashboardLoaderScript.get_display_val(ua, _current_metric)
	)
	var vb: float = (
		StatsDashboardLoaderScript.get_display_val_ci(ub, _current_metric)
		if use_ci
		else StatsDashboardLoaderScript.get_display_val(ub, _current_metric)
	)
	if va != vb:
		return va > vb if rev else va < vb
	return a.to_lower() > b.to_lower() if rev else a.to_lower() < b.to_lower()


func _update_doughnut() -> void:
	var s: Dictionary = _loader.summary_stats.get(_current_size, {})
	_doughnut.set_balance(
		int(s.get("p1", 0)),
		int(s.get("p2", 0)),
		int(s.get("draws", 0)),
		int(s.get("total", 0))
	)


func _build_tooltip(key: String, u_data: Dictionary, use_ci: bool, is_synergy: bool) -> String:
	var display_name: String = _display_name(key, is_synergy)
	var count: int = maxi(1, int(u_data.get("samples", 0))) if use_ci else StatsDashboardLoaderScript.get_count(u_data)
	var lines: PackedStringArray = PackedStringArray()
	if use_ci:
		var ci: Dictionary = u_data.get("ci", {})
		var wr: Array = ci.get("winRate", [u_data.get("winRate", 0.0), u_data.get("winRate", 0.0)])
		var dd: Array = ci.get("damage_dealt", [u_data.get("damage_dealt", 0.0), u_data.get("damage_dealt", 0.0)])
		var dr: Array = ci.get("damage_received", [u_data.get("damage_received", 0.0), u_data.get("damage_received", 0.0)])
		var dm: Array = ci.get("damage_mitigated", [u_data.get("damage_mitigated", 0.0), u_data.get("damage_mitigated", 0.0)])
		var hd: Array = ci.get("healing_done", [u_data.get("healing_done", 0.0), u_data.get("healing_done", 0.0)])
		lines.append("HERO: %s" % display_name)
		lines.append("Samples: %d" % int(count))
		lines.append("Win Rate: %.1f%% [%.1f%%, %.1f%%]" % [
			float(u_data.get("winRate", 0.0)) * 100.0,
			float(wr[0]) * 100.0,
			float(wr[1]) * 100.0,
		])
		lines.append("Avg Dmg: %.0f [%.0f, %.0f]" % [
			float(u_data.get("damage_dealt", 0.0)),
			float(dd[0]),
			float(dd[1]),
		])
		lines.append("Avg Rec: %.0f [%.0f, %.0f]" % [
			float(u_data.get("damage_received", 0.0)),
			float(dr[0]),
			float(dr[1]),
		])
		lines.append("Avg Mitigated: %.0f [%.0f, %.0f]" % [
			float(u_data.get("damage_mitigated", 0.0)),
			float(dm[0]),
			float(dm[1]),
		])
		lines.append("Avg Heal: %.0f [%.0f, %.0f]" % [
			float(u_data.get("healing_done", 0.0)),
			float(hd[0]),
			float(hd[1]),
		])
	else:
		lines.append("HERO: %s" % display_name)
		lines.append("Games Played: %d" % int(count))
		lines.append(
			"Record: %d-%d-%d (W-L-D)"
			% [int(u_data.get("wins", 0)), int(u_data.get("losses", 0)), int(u_data.get("draws", 0))]
		)
		var kills: float = float(u_data.get("kills", 0.0))
		var deaths: float = float(u_data.get("deaths", 0.0))
		var assists: float = float(u_data.get("assists", 0.0))
		var cf: float = float(count)
		lines.append(
			"K/D/A: %.1f/%.1f/%.1f (%.2f)"
			% [kills / cf, deaths / cf, assists / cf, float(u_data.get("kda", 0.0))]
		)
		lines.append(
			"Win Rate: %.1f%%" % (float(u_data.get("winRate", 0.0)) * 100.0)
		)
		lines.append(
			"Avg Dmg: %.0f (Rec: %.0f)"
			% [
				float(u_data.get("damage_dealt", 0.0)) / cf,
				float(u_data.get("damage_received", 0.0)) / cf,
			]
		)
		lines.append("Avg Mitigated: %.0f" % (float(u_data.get("damage_mitigated", 0.0)) / cf))
		lines.append(
			"Avg Heal: %.0f | Shield: %.0f"
			% [
				float(u_data.get("healing_done", 0.0)) / cf,
				float(u_data.get("shielding_done", 0.0)) / cf,
			]
		)
		lines.append("Stuns: %.1f" % (float(u_data.get("stuns", 0.0)) / cf))
		if u_data.has("breakdown"):
			var b: Dictionary = u_data["breakdown"]
			lines.append(
				"Atks: %.0f | Abs: %.0f | Ults: %.0f"
				% [
					float(b.get("auto", 0.0)) / cf,
					float(b.get("ability", 0.0)) / cf,
					float(b.get("ultimate", 0.0)) / cf,
				]
			)
	return "\n".join(lines)
