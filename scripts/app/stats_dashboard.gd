extends Control

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const StatsDashboardLoaderScript := preload("res://scripts/tools/stats_dashboard_loader.gd")
const StatsBalanceBarScript := preload("res://scripts/app/stats_balance_bar.gd")
const StatsBarControlScript := preload("res://scripts/app/stats_bar_control.gd")
const StatsChartAxisGuidesScript := preload("res://scripts/app/stats_chart_axis_guides.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const SimulationBatchWorkerScript := preload("res://scripts/simulation/simulation_batch_worker.gd")
const StatsSimulationCsvGeneratorScript := preload("res://scripts/tools/stats_simulation_csv_generator.gd")
const MatchupDataLoaderScript := preload("res://scripts/tools/matchup_data_loader.gd")

const COLOR_BG := Color(0.078, 0.078, 0.102, 1.0)
const COLOR_PANEL := Color(0.11, 0.11, 0.149, 1.0)
const COLOR_TEXT := Color(0.9, 0.9, 0.9, 1.0)
const COLOR_HIGHLIGHT := Color(0.47, 0.86, 0.55, 1.0)
const COLOR_SUBTLE := Color(0.71, 0.71, 0.75, 1.0)
const COLOR_RED := Color(0.88, 0.31, 0.31, 1.0)
const COLOR_GREEN := Color(0.47, 0.86, 0.55, 1.0)
const COLOR_BLUE := Color(0.275, 0.51, 1.0)
const COLOR_SECTION_BG := Color(0.133, 0.133, 0.18, 1.0)

## Larger UI; theme default_font_size drives most controls.
const UI_FONT_BODY := 20
const UI_FONT_SECTION := 22
const UI_FONT_TAB := 21
const UI_MIN_CONTROL_H := 46
const UI_ROW_MARGIN := 3
const UI_NAME_H_SEP := 2
const UI_ROLE_MARKER_S := 10
const UI_TOOLTIP_MOUSE_OFF := Vector2(16, 20)
const UI_HEADER_SUB_FONT := 16
## Chart hover tooltip (custom overlay, not Theme tooltip).
const UI_TOOLTIP_FONT_SIZE := 22
const UI_TOOLTIP_MIN_WIDTH := 520
const UI_TOOLTIP_CONTENT_MARGIN := 10
const DASHBOARD_MAX_FPS := 60
## Left pane ~stats_gui.py sidebar: narrow controls column, chart gets the rest.
const UI_BALANCE_BAR_MIN := Vector2(236, 56)
const UI_SIDEBAR_MIN_W := 320
const UI_TOP_BAR_H := 52
const UI_WINDOW_MIN := Vector2(1920, 1080)
## When true, export always opens the confirm dialog first (for UI testing only).
const DEBUG_FORCE_REGEN_CONFIRM_POPUP := false

const CI_SUPPORTED: Dictionary = {
	&"winRate": true,
	&"damage_dealt": true,
	&"damage_received": true,
	&"healing_done": true,
	&"damage_mitigated": true,
}

const ROLE_COLORS: Dictionary = {
	"tank": Color8(204, 51, 51),
	"fighter": Color8(210, 105, 30),
	"assassin": Color8(153, 50, 204),
	"marksman": Color8(34, 139, 34),
	"mage": Color8(76, 153, 204),
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
	{"label": "KDA Ratio", "key": &"kda"},
	{"label": "Alphabetical", "key": &"name"},
	
]

const VIEW_MODES: Array[Dictionary] = [
	{"label": "Champions", "key": &"champions"},
	{"label": "Roles", "key": &"roles"},
]

@export_group("Chart table")
@export var chart_col_sep: int = 6
@export var chart_row_bar_h: int = 32
@export var chart_bar_holder_min_w: int = 58
@export var chart_scroll_min_bar_region: int = 136
@export var chart_scroll_width_slack: int = 48
@export var chart_label_w: int = 152
@export var chart_label_w_synergy: int = 464
@export var chart_games_col_w: int = 72
@export var chart_kda_col_w: int = 120
@export var chart_val_col_w: int = 96

var _loader = StatsDashboardLoaderScript.new()
var _unit_roles: Dictionary = {}
var _stats_path: String = ""

var _current_size: int = 5
var _current_metric: StringName = &"winRate"
var _view_mode: StringName = &"champions"
var _current_sort: StringName = &"value"
var _absolute_colors: bool = true
var _use_ci: bool = true
var _search_text: String = ""
var _active_role_filters: Dictionary = {}
var _chart_uses_ci: bool = false

var _balance_bar: Control
var _metric_option: OptionButton
var _view_option: OptionButton
var _sort_option: OptionButton
var _ci_button: Button
var _color_button: Button
var _search: LineEdit
var _role_grid: GridContainer
var _chart_vbox: VBoxContainer
var _title_label: Label
var _chart_hdr_row: HBoxContainer
var _hdr_name_stack: VBoxContainer
var _hdr_bar_stack: VBoxContainer
var _hdr_games_stack: VBoxContainer
var _hdr_val_stack: VBoxContainer
var _hdr_name_pri: Label
var _hdr_name_sub: Label
var _hdr_bar_pri: Label
var _hdr_bar_sub: Label
var _hdr_games_pri: Label
var _hdr_games_sub: Label
var _hdr_val_pri: Label
var _hdr_val_sub: Label
var _hdr_kda_stack: VBoxContainer
var _hdr_kda_pri: Label
var _hdr_bar_titles_vb: VBoxContainer
var _hdr_bar_pct_axis: HBoxContainer
var _hdr_pct_spacer_left: Control
var _hdr_pct_spacer_mid: Control
var _hdr_pct_l50: Label
var _hdr_pct_l100: Label
var _chart_host: Control
var _axis_guides: Control
var _tt_panel: PanelContainer
var _tt_label: RichTextLabel
var _right_panel: VBoxContainer
var _tt_style: StyleBoxFlat
var _error_label: Label
var _refresh_btn: Button
var _team_button_group: ButtonGroup
var _team_size_buttons: Dictionary = {}
var _regen_checks: Dictionary = {}
var _regen_sample_edit: LineEdit
var _regen_worker_edit: LineEdit
var _regen_button: Button
var _regen_progress: ProgressBar
var _regen_status: Label
var _export_popup: Window
var _export_popup_content: VBoxContainer
var _matchup_loader: RefCounted
var _matchup_vb: VBoxContainer
var _matchup_right_panel: VBoxContainer
var _current_champion: String = ""
var _current_view_mode: int = 2  # 0=vs, 1=with, 2=both
var _current_sort_mode: int = 0  # 0=winrate_desc, 1=winrate_asc, 2=wins, 3=losses, 4=alpha
var _native_required_notice: Label
var _regen_native_confirm: ConfirmationDialog
var _regen_stashed_export_params: Variant = null
var _regen_thread: Thread
var _regen_runner: RefCounted
var _regen_poll_timer: Timer
var _regen_total_matches: int = 0
var _regen_wall_start_msec: int = 0
var _regen_target_dir: String = ""
var _main_split: HSplitContainer
var _saved_max_fps_before_cap: int = 0
var _fps_cap_applied: bool = false

@onready var _root_vb: VBoxContainer = $RootVBox


func _native_backend_ready() -> bool:
	return ClassDB.class_exists("TeamfightSimulationCore") and ClassDB.can_instantiate("TeamfightSimulationCore")


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
	_apply_dashboard_fps_cap()
	_apply_dashboard_window_settings()
	_team_button_group = ButtonGroup.new()
	_build_unit_roles()
	_setup_regen_timer()  # Initialize timer before potential early return
	if not _try_load_stats():
		_build_ui()  # Load normal UI with red border on generate button
		_wire_controls()  # Wire controls even without data
		_setup_regen_native_confirm_dialog()
		return
	_clamp_current_size_to_loaded()
	_build_ui()
	_wire_controls()
	_setup_regen_native_confirm_dialog()
	_refresh_all()


func _apply_dashboard_fps_cap() -> void:
	if _fps_cap_applied:
		return
	_saved_max_fps_before_cap = Engine.max_fps
	Engine.max_fps = DASHBOARD_MAX_FPS
	_fps_cap_applied = true


func _exit_tree() -> void:
	if _fps_cap_applied:
		Engine.max_fps = _saved_max_fps_before_cap
		_fps_cap_applied = false


func _apply_dashboard_window_settings() -> void:
	custom_minimum_size = UI_WINDOW_MIN
	var win: Window = get_viewport().get_window()
	if win == null:
		return
	win.min_size = UI_WINDOW_MIN
	win.mode = Window.MODE_MAXIMIZED


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
	push_error("StatsDashboard: no CSV bundle (need res://stats_output)")
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
	lb.add_theme_font_size_override("font_size", UI_FONT_SECTION)
	add_child(lb)


func _build_ui() -> void:
	var theme := Theme.new()
	theme.default_font_size = UI_FONT_BODY
	theme.set_font_size("font_size", "TabBar", UI_FONT_TAB)
	_root_vb.theme = theme
	
	# When no data available, highlight Generate Stats button in bright red
	if _regen_button != null and _stats_path.is_empty():
		_regen_button.add_theme_color_override("font_color", Color.RED)
		_regen_button.add_theme_stylebox_override("normal", StyleBoxFlat.new())
		var red_border_style := _regen_button.get_theme_stylebox("normal") as StyleBoxFlat
		red_border_style.border_color = Color.RED
		red_border_style.border_width_left = 2
		red_border_style.border_width_right = 2
		red_border_style.border_width_top = 2
		red_border_style.border_width_bottom = 2
		_regen_button.add_theme_stylebox_override("normal", red_border_style)
	
	# When no data available, highlight Generate Stats button in bright red
	if _regen_button != null and _stats_path.is_empty():
		_regen_button.add_theme_color_override("font_color", Color.RED)
		_regen_button.add_theme_stylebox_override("normal", StyleBoxFlat.new())
		var red_border_style := _regen_button.get_theme_stylebox("normal") as StyleBoxFlat
		red_border_style.border_color = Color.RED
		red_border_style.border_width_left = 2
		red_border_style.border_width_right = 2
		red_border_style.border_width_top = 2
		red_border_style.border_width_bottom = 2
		_regen_button.add_theme_stylebox_override("normal", red_border_style)

	var top := HBoxContainer.new()
	top.custom_minimum_size.y = UI_TOP_BAR_H
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top.add_child(spacer)
	var quit := Button.new()
	quit.text = "Back to Menu"
	quit.custom_minimum_size = Vector2(120, UI_MIN_CONTROL_H)
	quit.size_flags_horizontal = Control.SIZE_SHRINK_END
	quit.pressed.connect(_on_back_to_menu)
	top.add_child(quit)
	_root_vb.add_child(top)

	var split := HSplitContainer.new()
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	split.split_offset = UI_SIDEBAR_MIN_W
	_root_vb.add_child(split)

	var tabs := TabContainer.new()
	tabs.custom_minimum_size.x = UI_SIDEBAR_MIN_W
	# Do not EXPAND horizontally: lets HSplitContainer honor a narrow split_offset.
	tabs.size_flags_horizontal = Control.SIZE_FILL
	tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
	split.add_child(tabs)

	var filter_scroll := ScrollContainer.new()
	filter_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	filter_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	filter_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	var filter_vb := VBoxContainer.new()
	filter_vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	filter_vb.add_theme_constant_override("separation", 10)
	filter_scroll.add_child(filter_vb)
	tabs.add_child(filter_scroll)

	var regen_scroll := ScrollContainer.new()
	regen_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	regen_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	regen_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	regen_scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	var regen_vb := VBoxContainer.new()
	regen_vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	regen_vb.size_flags_vertical = Control.SIZE_EXPAND_FILL
	regen_vb.add_theme_constant_override("separation", 10)
	regen_scroll.add_child(regen_vb)
	tabs.add_child(regen_scroll)
	tabs.set_tab_title(0, "Filters")
	tabs.set_tab_title(1, "Matchups")

	# Add tab change listener
	tabs.tab_changed.connect(_on_tab_changed)

	# Add New Data button to top bar (after spacer, before Quit)
	var new_data_button := Button.new()
	new_data_button.text = "Generate Data"
	new_data_button.custom_minimum_size = Vector2(120, UI_MIN_CONTROL_H)
	new_data_button.size_flags_horizontal = Control.SIZE_SHRINK_END
	new_data_button.pressed.connect(_on_new_data_pressed)
	top.add_child(new_data_button)
	# Move after spacer (index 0) and before Quit (last child)
	top.move_child(new_data_button, 1)

	_error_label = Label.new()
	_error_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	filter_vb.add_child(_error_label)

	_balance_bar = StatsBalanceBarScript.new()
	_balance_bar.custom_minimum_size = UI_BALANCE_BAR_MIN
	_balance_bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	filter_vb.add_child(_balance_bar)

	filter_vb.add_child(_section_label("VIEW MODE"))
	_view_option = OptionButton.new()
	_view_option.fit_to_longest_item = false
	_view_option.custom_minimum_size.y = UI_MIN_CONTROL_H
	for vm in VIEW_MODES:
		_view_option.add_item(vm["label"])
	filter_vb.add_child(_view_option)

	filter_vb.add_child(_section_label("METRICS"))
	_metric_option = OptionButton.new()
	_metric_option.fit_to_longest_item = false
	_metric_option.custom_minimum_size.y = UI_MIN_CONTROL_H
	for m in METRICS:
		_metric_option.add_item(m["label"])
	filter_vb.add_child(_metric_option)

	filter_vb.add_child(_section_label("TEAM SIZE"))
	var team_row := HBoxContainer.new()
	team_row.add_theme_constant_override("separation", 3)
	var sizes: Array = Array(SimConstantsScript.SIMULATION_TEAM_SIZES)
	_team_size_buttons.clear()
	for sz in sizes:
		var b := Button.new()
		b.text = str(int(sz))
		b.toggle_mode = true
		b.custom_minimum_size = Vector2(46, UI_MIN_CONTROL_H)
		b.button_group = _team_button_group
		b.pressed.connect(_on_team_size_pressed.bind(int(sz)))
		team_row.add_child(b)
		_team_size_buttons[int(sz)] = b
		if int(sz) == _current_size:
			b.button_pressed = true
	filter_vb.add_child(team_row)

	filter_vb.add_child(_section_label("DATA MODE"))
	_ci_button = Button.new()
	_ci_button.custom_minimum_size.y = UI_MIN_CONTROL_H
	filter_vb.add_child(_ci_button)

	filter_vb.add_child(_section_label("SORT BY"))
	_sort_option = OptionButton.new()
	_sort_option.fit_to_longest_item = false
	_sort_option.custom_minimum_size.y = UI_MIN_CONTROL_H
	for sm in SORT_MODES:
		_sort_option.add_item(sm["label"])
	filter_vb.add_child(_sort_option)

	filter_vb.add_child(_section_label("COLOR SCALE"))
	_color_button = Button.new()
	_color_button.custom_minimum_size.y = UI_MIN_CONTROL_H
	filter_vb.add_child(_color_button)

	filter_vb.add_child(_section_label("FILTER"))
	_search = LineEdit.new()
	_search.custom_minimum_size.y = UI_MIN_CONTROL_H
	_search.placeholder_text = "Search hero..."
	filter_vb.add_child(_search)

	filter_vb.add_child(_section_label("ROLES"))
	_role_grid = GridContainer.new()
	_role_grid.columns = 2
	_role_grid.add_theme_constant_override("h_separation", 8)
	_role_grid.add_theme_constant_override("v_separation", 8)
	for role_key in ROLE_COLORS.keys():
		var tb := Button.new()
		tb.toggle_mode = true
		tb.custom_minimum_size.y = UI_MIN_CONTROL_H
		tb.text = str(role_key).to_upper()
		var rk := str(role_key)
		tb.toggled.connect(func(on: bool): _on_role_toggled(rk, on))
		_role_grid.add_child(tb)
	filter_vb.add_child(_role_grid)

	# Export UI moved to popup - Matchups tab now free for matchup visualization
	regen_vb.add_child(_section_label("MATCHUP ANALYSIS"))
	_matchup_vb = VBoxContainer.new()
	_matchup_vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_matchup_vb.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_matchup_vb.add_theme_constant_override("separation", 16)
	regen_vb.add_child(_matchup_vb)
	
	# Initialize matchup loader
	_matchup_loader = MatchupDataLoaderScript.new()
	_build_matchup_ui()

	var right_vb := VBoxContainer.new()
	right_vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	right_vb.size_flags_vertical = Control.SIZE_EXPAND_FILL
	right_vb.size_flags_stretch_ratio = 1.0
	split.add_child(right_vb)
	_right_panel = right_vb  # Store reference for tab switching

	_title_label = Label.new()
	_title_label.add_theme_color_override("font_color", COLOR_TEXT)
	_title_label.add_theme_font_size_override("font_size", UI_FONT_SECTION + 2)
	right_vb.add_child(_title_label)

	_chart_hdr_row = HBoxContainer.new()
	_chart_hdr_row.add_theme_constant_override("separation", chart_col_sep)
	_hdr_name_stack = VBoxContainer.new()
	_hdr_name_stack.custom_minimum_size.x = chart_label_w
	_hdr_name_pri = Label.new()
	_hdr_name_pri.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	_hdr_name_sub = Label.new()
	_hdr_name_stack.add_child(_hdr_name_pri)
	_hdr_name_stack.add_child(_hdr_name_sub)
	_style_header_sub_label(_hdr_name_sub)

	_hdr_bar_stack = VBoxContainer.new()
	_hdr_bar_stack.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_hdr_bar_titles_vb = VBoxContainer.new()
	_hdr_bar_titles_vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_hdr_bar_pri = Label.new()
	_hdr_bar_pri.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_hdr_bar_sub = Label.new()
	_hdr_bar_sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_hdr_bar_titles_vb.add_child(_hdr_bar_pri)
	_hdr_bar_titles_vb.add_child(_hdr_bar_sub)
	_style_header_sub_label(_hdr_bar_sub)
	_hdr_bar_pct_axis = HBoxContainer.new()
	_hdr_bar_pct_axis.add_theme_constant_override("separation", 0)
	_hdr_bar_pct_axis.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_hdr_bar_pct_axis.visible = false
	_hdr_pct_spacer_left = Control.new()
	_hdr_pct_spacer_left.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_hdr_pct_spacer_left.size_flags_stretch_ratio = 1.0
	_hdr_pct_l50 = Label.new()
	_hdr_pct_l50.text = "50%"
	_hdr_pct_l50.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_hdr_pct_l50.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_hdr_pct_l50.add_theme_color_override("font_color", COLOR_SUBTLE)
	_hdr_pct_l50.add_theme_font_size_override("font_size", UI_HEADER_SUB_FONT)
	_hdr_pct_spacer_mid = Control.new()
	_hdr_pct_spacer_mid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_hdr_pct_spacer_mid.size_flags_stretch_ratio = 1.0
	_hdr_pct_l100 = Label.new()
	_hdr_pct_l100.text = "100%"
	_hdr_pct_l100.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_hdr_pct_l100.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_hdr_pct_l100.add_theme_color_override("font_color", COLOR_SUBTLE)
	_hdr_pct_l100.add_theme_font_size_override("font_size", UI_HEADER_SUB_FONT)
	_hdr_bar_pct_axis.add_child(_hdr_pct_spacer_left)
	_hdr_bar_pct_axis.add_child(_hdr_pct_l50)
	_hdr_bar_pct_axis.add_child(_hdr_pct_spacer_mid)
	_hdr_bar_pct_axis.add_child(_hdr_pct_l100)
	_hdr_bar_stack.add_child(_hdr_bar_titles_vb)
	_hdr_bar_stack.add_child(_hdr_bar_pct_axis)

	_hdr_games_stack = VBoxContainer.new()
	_hdr_games_stack.custom_minimum_size.x = chart_games_col_w
	_hdr_games_stack.alignment = BoxContainer.ALIGNMENT_CENTER
	_hdr_games_pri = Label.new()
	_hdr_games_pri.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_hdr_games_pri.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_hdr_games_sub = Label.new()
	_hdr_games_sub.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_hdr_games_sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_hdr_games_stack.add_child(_hdr_games_pri)
	_hdr_games_stack.add_child(_hdr_games_sub)
	_style_header_sub_label(_hdr_games_sub)

	_hdr_kda_stack = VBoxContainer.new()
	_hdr_kda_stack.custom_minimum_size.x = chart_kda_col_w
	_hdr_kda_stack.alignment = BoxContainer.ALIGNMENT_CENTER
	_hdr_kda_pri = Label.new()
	_hdr_kda_pri.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_hdr_kda_pri.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_hdr_kda_pri.text = "K/D/A"
	_hdr_kda_stack.add_child(_hdr_kda_pri)

	_hdr_val_stack = VBoxContainer.new()
	_hdr_val_stack.custom_minimum_size.x = chart_val_col_w
	_hdr_val_stack.alignment = BoxContainer.ALIGNMENT_CENTER
	_hdr_val_pri = Label.new()
	_hdr_val_pri.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_hdr_val_pri.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_hdr_val_sub = Label.new()
	_hdr_val_sub.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_hdr_val_sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_hdr_val_stack.add_child(_hdr_val_pri)
	_hdr_val_stack.add_child(_hdr_val_sub)
	_style_header_sub_label(_hdr_val_sub)

	for lb: Label in [_hdr_name_pri, _hdr_bar_pri, _hdr_games_pri, _hdr_kda_pri, _hdr_val_pri]:
		lb.add_theme_font_size_override("font_size", UI_FONT_BODY)

	_hdr_name_sub.visible = false
	_hdr_bar_sub.visible = false
	_hdr_games_sub.visible = false
	_hdr_val_sub.visible = false

	_chart_hdr_row.add_child(_hdr_name_stack)
	_chart_hdr_row.add_child(_hdr_bar_stack)
	_chart_hdr_row.add_child(_hdr_val_stack)
	_chart_hdr_row.add_child(_hdr_games_stack)
	_chart_hdr_row.add_child(_hdr_kda_stack)
	var chart_hdr_wrap := MarginContainer.new()
	chart_hdr_wrap.add_theme_constant_override("margin_left", UI_ROW_MARGIN)
	chart_hdr_wrap.add_theme_constant_override("margin_right", UI_ROW_MARGIN)
	chart_hdr_wrap.add_child(_chart_hdr_row)
	right_vb.add_child(chart_hdr_wrap)

	_chart_host = Control.new()
	_chart_host.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_chart_host.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_chart_host.custom_minimum_size.y = 120
	var chart_scroll := ScrollContainer.new()
	_stretch_control_to_parent_full_rect(chart_scroll)
	chart_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	chart_scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	_chart_vbox = VBoxContainer.new()
	_chart_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	chart_scroll.add_child(_chart_vbox)
	_chart_host.add_child(chart_scroll)
	_axis_guides = StatsChartAxisGuidesScript.new()
	_axis_guides.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_stretch_control_to_parent_full_rect(_axis_guides)
	_chart_host.add_child(_axis_guides)
	right_vb.add_child(_chart_host)

	_build_chart_tooltip_overlay()

	_main_split = split
	call_deferred("_apply_sidebar_split_after_layout")


func _apply_sidebar_split_after_layout() -> void:
	if _main_split == null or _main_split.get_child_count() < 1:
		return
	# Wait until split + TabContainer report real combined mins (deferred alone is one frame too early on some platforms).
	await get_tree().process_frame
	await get_tree().process_frame
	var left: Control = _main_split.get_child(0) as Control
	if left == null:
		return
	var mw: int = maxi(int(ceili(left.get_combined_minimum_size().x)), UI_SIDEBAR_MIN_W)
	_main_split.split_offset = mw


func _section_label(text: String) -> Label:
	var lb := Label.new()
	lb.text = text
	lb.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	lb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	lb.add_theme_color_override("font_color", COLOR_HIGHLIGHT)
	lb.add_theme_font_size_override("font_size", UI_FONT_SECTION)
	return lb


func _style_header_sub_label(lb: Label) -> void:
	lb.add_theme_color_override("font_color", COLOR_SUBTLE)
	lb.add_theme_font_size_override("font_size", UI_HEADER_SUB_FONT)
	lb.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART


func _build_chart_tooltip_overlay() -> void:
	_tt_style = StyleBoxFlat.new()
	_tt_style.bg_color = COLOR_PANEL
	_tt_style.set_border_width_all(2)
	_tt_style.border_color = COLOR_SUBTLE
	_tt_style.set_corner_radius_all(4)
	_tt_style.set_content_margin_all(UI_TOOLTIP_CONTENT_MARGIN)
	_tt_panel = PanelContainer.new()
	_tt_panel.visible = false
	_tt_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_tt_panel.z_index = 80
	_tt_panel.add_theme_stylebox_override("panel", _tt_style)
	_tt_label = RichTextLabel.new()
	_tt_label.bbcode_enabled = true
	_tt_label.scroll_active = false
	_tt_label.fit_content = true
	_tt_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_tt_label.add_theme_color_override("default_color", COLOR_TEXT)
	_tt_label.add_theme_font_size_override("normal_font_size", UI_TOOLTIP_FONT_SIZE)
	_tt_label.add_theme_font_size_override("bold_font_size", UI_TOOLTIP_FONT_SIZE)
	_tt_label.add_theme_font_size_override("italics_font_size", UI_TOOLTIP_FONT_SIZE)
	_tt_label.add_theme_font_size_override("bold_italics_font_size", UI_TOOLTIP_FONT_SIZE)
	_tt_label.add_theme_font_size_override("mono_font_size", UI_TOOLTIP_FONT_SIZE)
	_tt_label.custom_minimum_size.x = UI_TOOLTIP_MIN_WIDTH
	_tt_panel.add_child(_tt_label)
	add_child(_tt_panel)


func _hide_chart_tooltip() -> void:
	if _tt_panel != null:
		_tt_panel.visible = false


func _position_chart_tooltip() -> void:
	if _tt_panel == null or not _tt_panel.visible:
		return
	var vp: Rect2 = get_viewport().get_visible_rect()
	var sz: Vector2 = _tt_panel.get_combined_minimum_size()
	var g: Vector2 = get_global_mouse_position() + UI_TOOLTIP_MOUSE_OFF
	g.x = clampf(g.x, vp.position.x + 4.0, vp.end.x - sz.x - 4.0)
	g.y = clampf(g.y, vp.position.y + 4.0, vp.end.y - sz.y - 4.0)
	_tt_panel.global_position = g


func _chart_row_tt_enter(row: PanelContainer) -> void:
	if _tt_panel == null:
		return
	_tt_label.text = str(row.get_meta(&"tt", ""))
	_tt_style.border_color = row.get_meta(&"tt_border", COLOR_SUBTLE) as Color
	_tt_panel.visible = true
	_tt_panel.reset_size()
	_position_chart_tooltip()


func _chart_row_tt_exit() -> void:
	_hide_chart_tooltip()


func _chart_row_tt_motion(event: InputEvent) -> void:
	if event is InputEventMouseMotion and _tt_panel != null and _tt_panel.visible:
		_position_chart_tooltip()


func _tooltip_border_for_row(key_s: String, is_synergy: bool) -> Color:
	if is_synergy:
		return COLOR_SUBTLE
	if _view_mode == &"champions":
		var rk: String = str(_unit_roles.get(key_s, ""))
		return ROLE_COLORS.get(rk, COLOR_SUBTLE) as Color
	if _view_mode == &"roles":
		return ROLE_COLORS.get(str(key_s).to_lower(), COLOR_SUBTLE) as Color
	return COLOR_SUBTLE


func _sync_chart_table_headers(label_width: float, is_synergy: bool) -> void:
	if _chart_hdr_row == null:
		return
	_chart_hdr_row.visible = true
	_hdr_name_stack.custom_minimum_size.x = label_width
	_hdr_val_stack.custom_minimum_size.x = chart_val_col_w
	_hdr_games_stack.custom_minimum_size.x = chart_games_col_w
	_hdr_kda_stack.custom_minimum_size.x = chart_kda_col_w
	var entity_pri := "Champion"
	if is_synergy:
		entity_pri = "Composition"
	elif _view_mode == &"roles":
		entity_pri = "Role"
	_hdr_name_pri.text = entity_pri
	var show_pct_axis: bool = _current_metric == &"winRate" or is_synergy
	_hdr_bar_titles_vb.visible = not show_pct_axis
	_hdr_bar_pct_axis.visible = show_pct_axis
	if not show_pct_axis:
		_hdr_bar_pri.text = "Metric bar"
	_hdr_games_pri.text = "Games"
	_hdr_val_pri.text = "Value"


func _hide_chart_chrome() -> void:
	_hide_chart_tooltip()
	if _chart_hdr_row != null:
		_chart_hdr_row.visible = false
	if _axis_guides != null:
		_axis_guides.show_percent_guides = false
		_axis_guides.show_bar_origin_line = false
		_axis_guides.queue_redraw()


func _show_chart_chrome() -> void:
	if _chart_hdr_row != null:
		_chart_hdr_row.visible = true


func _set_row_insides_mouse_ignore(root: Control) -> void:
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	for c in root.get_children():
		if c is Control:
			_set_row_insides_mouse_ignore(c as Control)


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
	# Only connect regen button if it exists (export UI built in popup)
	if _regen_button != null:
		_regen_button.pressed.connect(_on_regenerate_pressed)


func _setup_regen_native_confirm_dialog() -> void:
	_regen_native_confirm = ConfirmationDialog.new()
	_regen_native_confirm.title = "Native backend required"
	_regen_native_confirm.ok_button_text = "Retry export"
	_regen_native_confirm.cancel_button_text = "Cancel"
	_regen_native_confirm.dialog_text = (
		"native/bin/teamfight_simulation_core.dll was not found. "
		+ "Export requires the native TeamfightSimulationCore backend.\n\n"
		+ "Build the DLL, then click Retry export."
	)
	_regen_native_confirm.min_size = Vector2i(420, 180)
	_regen_native_confirm.exclusive = true
	_regen_native_confirm.always_on_top = true
	_regen_native_confirm.unresizable = true
	var dlg_lbl: Label = _regen_native_confirm.get_label()
	dlg_lbl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	dlg_lbl.add_theme_color_override("font_color", COLOR_RED)
	_regen_native_confirm.confirmed.connect(_on_regen_native_export_confirmed)
	_regen_native_confirm.canceled.connect(_on_regen_native_export_canceled)
	var win: Window = get_viewport().get_window()
	if win == null:
		add_child(_regen_native_confirm)
		return
	# Sub-Window must be a direct child of the main Window (not CanvasLayer) to show reliably.
	win.add_child(_regen_native_confirm)


func _setup_regen_timer() -> void:
	_regen_poll_timer = Timer.new()
	_regen_poll_timer.wait_time = 0.1
	_regen_poll_timer.one_shot = false
	add_child(_regen_poll_timer)
	_regen_poll_timer.timeout.connect(_on_regen_poll_tick)


func _set_regen_busy(busy: bool) -> void:
	_regen_button.disabled = busy
	for k in _regen_checks.keys():
		var cb: BaseButton = _regen_checks[k] as BaseButton
		if cb != null:
			cb.disabled = busy
	_regen_sample_edit.editable = not busy


func _resolve_writable_stats_dir() -> String:
	if _dir_probe_write("res://stats_output"):
		return "res://stats_output"
	return "user://stats_output"


func _dir_probe_write(dir: String) -> bool:
	var abs_base := ProjectSettings.globalize_path(dir)
	DirAccess.make_dir_recursive_absolute(abs_base)
	var probe_path := "%s/.write_probe" % dir.rstrip("/")
	var f := FileAccess.open(probe_path, FileAccess.WRITE)
	if f == null:
		return false
	f.flush()
	f.close()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(probe_path))
	return true


func _read_regen_export_params() -> Variant:
	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		_regen_status.text = "Simulation backend is not available."
		_regen_status.add_theme_color_override("font_color", COLOR_RED)
		return null
	var selected_sizes: Array = []
	for sz in SimConstantsScript.SIMULATION_TEAM_SIZES:
		var cb: CheckBox = _regen_checks.get(int(sz)) as CheckBox
		if cb != null and cb.button_pressed:
			selected_sizes.append(int(sz))
	if selected_sizes.is_empty():
		_regen_status.text = "Select at least one mode (1v1 … 5v5)."
		_regen_status.add_theme_color_override("font_color", COLOR_RED)
		return null
	var n_text := _regen_sample_edit.text.strip_edges()
	if n_text.is_empty() or not n_text.is_valid_int():
		_regen_status.text = "Invalid sample size."
		_regen_status.add_theme_color_override("font_color", COLOR_RED)
		return null
	var n: int = int(n_text)
	if n < 1:
		_regen_status.text = "Sample size must be >= 1."
		_regen_status.add_theme_color_override("font_color", COLOR_RED)
		return null
	var worker_text := _regen_worker_edit.text.strip_edges()
	if worker_text.is_empty() or not worker_text.is_valid_int():
		_regen_status.text = "Invalid worker count."
		_regen_status.add_theme_color_override("font_color", COLOR_RED)
		return null
	var max_worker_threads: int = int(worker_text)
	if max_worker_threads < 1:
		_regen_status.text = "Worker count must be >= 1."
		_regen_status.add_theme_color_override("font_color", COLOR_RED)
		return null
	return {
		"output_dir": _resolve_writable_stats_dir(),
		"team_sizes": selected_sizes,
		"matches_per_size": n,
		"base_seed": int(Time.get_ticks_msec() & 0x7FFFFFFF),
		"max_worker_threads": max_worker_threads,
	}


func _run_regen_export_thread(params: Dictionary) -> void:
	# This function runs in a background thread, so no UI updates here
	
	# Clear champion catalog caches before export to ensure fresh data
	ChampionCatalog.clear_export_caches()
	
	# Export champion schema to ensure latest data is included
	var schema_success := SimulationSchema.write_champion_schema_to_file()
	if not schema_success:
		push_error("Failed to export champion schema")
	
	# Now run the actual simulations
	var runner := StatsSimulationCsvGenerator.new()
	var err: Error = runner.run_packed(params)
	if err != OK:
		push_error("Simulation generation failed: %s" % error_string(err))


func _on_regenerate_pressed() -> void:
	if DEBUG_FORCE_REGEN_CONFIRM_POPUP:
		_regen_stashed_export_params = _read_regen_export_params()
		call_deferred("_deferred_show_regen_native_confirm")
		return
	var params: Variant = _read_regen_export_params()
	if params == null:
		return
	# Use runtime (GDExtension actually loaded), not FileAccess: non-Windows always skipped file check;
	# a present DLL path can still fail to register TeamfightSimulationCore.
	if not _native_backend_ready():
		_regen_stashed_export_params = params
		call_deferred("_deferred_show_regen_native_confirm")
		return
	_regen_stashed_export_params = null
	
	# Set up UI for background processing
	var selected_sizes: Array = params["team_sizes"]
	var n: int = int(params["matches_per_size"])
	_regen_target_dir = str(params["output_dir"])
	_regen_total_matches = selected_sizes.size() * n
	_regen_status.text = "Generating…"
	_regen_status.remove_theme_color_override("font_color")
	_regen_progress.max_value = maxf(1.0, float(_regen_total_matches))
	_regen_progress.value = 0.0
	_regen_progress.visible = true
	_set_regen_busy(true)
	
	# Start background thread for export and simulations
	_regen_thread = Thread.new()
	var start_err: Error = _regen_thread.start(Callable(self, "_run_regen_export_thread").bind(params))
	if start_err != OK:
		_regen_thread = null
		_regen_progress.visible = false
		_set_regen_busy(false)
		_regen_status.text = "Could not start background thread."
		_regen_status.add_theme_color_override("font_color", COLOR_RED)
		return
	_regen_wall_start_msec = Time.get_ticks_msec()
	_regen_poll_timer.start()


func _deferred_show_regen_native_confirm() -> void:
	if _regen_native_confirm == null:
		push_error("StatsDashboard: regen confirmation dialog is null.")
		return
	_regen_native_confirm.reset_size()
	_regen_native_confirm.popup_centered()


func _on_regen_native_export_confirmed() -> void:
	var params: Variant = _regen_stashed_export_params
	_regen_stashed_export_params = null
	
	# Set up UI for background processing
	var selected_sizes: Array = params["team_sizes"]
	var n: int = int(params["matches_per_size"])
	_regen_target_dir = str(params["output_dir"])
	_regen_total_matches = selected_sizes.size() * n
	_regen_status.text = "Generating…"
	_regen_status.remove_theme_color_override("font_color")
	_regen_progress.max_value = maxf(1.0, float(_regen_total_matches))
	_regen_progress.value = 0.0
	_regen_progress.visible = true
	_set_regen_busy(true)
	
	# Start background thread for export and simulations
	_regen_thread = Thread.new()
	var start_err: Error = _regen_thread.start(Callable(self, "_run_regen_export_thread").bind(params))
	if start_err != OK:
		_regen_thread = null
		_regen_progress.visible = false
		_set_regen_busy(false)
		_regen_status.text = "Could not start background thread."
		_regen_status.add_theme_color_override("font_color", COLOR_RED)
		return
	_regen_wall_start_msec = Time.get_ticks_msec()
	_regen_poll_timer.start()


func _on_regen_native_export_canceled() -> void:
	_regen_stashed_export_params = null


func _on_regen_poll_tick() -> void:
	if _regen_thread == null:
		_regen_poll_timer.stop()
		return
	var cur: int = SimulationBatchWorkerScript.benchmark_progress_read_value()
	_regen_progress.value = minf(float(cur), _regen_progress.max_value)
	if _regen_thread.is_alive():
		return
	_regen_poll_timer.stop()
	var result = _regen_thread.wait_to_finish()
	var err: int = 0  # Default to OK since background thread doesn't return error codes
	_regen_thread = null
	_regen_runner = null
	_finish_regen_completed(err)


func _finish_regen_completed(err: int) -> void:
	_regen_progress.visible = false
	_set_regen_busy(false)
	if err != OK:
		_regen_status.text = "Regenerate failed: %s" % error_string(err)
		_regen_status.add_theme_color_override("font_color", COLOR_RED)
		return
	_stats_path = _regen_target_dir
	var lerr: Error = _loader.load_from_dir(_stats_path)
	if lerr != OK:
		_regen_status.text = "Wrote CSVs but reload failed: %s" % error_string(lerr)
		_regen_status.add_theme_color_override("font_color", COLOR_RED)
		return
	_clamp_current_size_to_loaded()
	_sync_team_size_buttons()
	var elapsed_s: float = (Time.get_ticks_msec() - _regen_wall_start_msec) / 1000.0
	var mps: float = float(_regen_total_matches) / maxf(elapsed_s, 0.0001)
	_regen_status.text = (
		"Reloaded data from %s — %d matches in %.2f s (%.0f matches/s)"
		% [_stats_path, _regen_total_matches, elapsed_s, mps]
	)
	_regen_status.add_theme_color_override("font_color", COLOR_SUBTLE)
	_refresh_all()


func _clamp_current_size_to_loaded() -> void:
	if _loader.summary_stats.is_empty():
		return
	if _loader.summary_stats.has(_current_size):
		return
	var keys: Array = _loader.summary_stats.keys()
	keys.sort()
	_current_size = int(keys[0])


func _sync_team_size_buttons() -> void:
	for sz in _team_size_buttons.keys():
		var btn: Button = _team_size_buttons[sz] as Button
		if btn == null:
			continue
		btn.button_pressed = int(sz) == _current_size


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
	# Refresh matchups tab when data is updated
	_refresh_matchup_data()
	if not _current_champion.is_empty():
		_update_matchup_display()


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


func _current_metric_label_string() -> String:
	for m in METRICS:
		if m["key"] == _current_metric:
			return m["label"]
	return String(_current_metric)


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


func _chart_scroll_min_x(label_width: float) -> int:
	var tail: float = float(
		chart_col_sep + chart_val_col_w + chart_col_sep + chart_games_col_w + chart_col_sep + chart_kda_col_w
	)
	return int(label_width + float(chart_scroll_min_bar_region) + tail + float(chart_scroll_width_slack))


func _default_export_worker_threads() -> int:
	return maxi(
		1,
		mini(
			OS.get_processor_count(),
			StatsSimulationCsvGeneratorScript.DEFAULT_EXPORT_MAX_WORKER_THREADS
		) / 2
	)


func _refresh_chart() -> void:
	_hide_chart_tooltip()
	for c in _chart_vbox.get_children():
		c.queue_free()

	var is_synergy: bool = _current_metric == &"synergy"
	var use_ci: bool = _ci_mode_active()
	_chart_uses_ci = use_ci
	var label_width: float = float(chart_label_w_synergy) if is_synergy else float(chart_label_w)

	if is_synergy and _current_size == 1:
		_hide_chart_chrome()
		var lb := Label.new()
		lb.text = "Synergies N/A for 1v1"
		lb.add_theme_color_override("font_color", COLOR_TEXT)
		lb.add_theme_font_size_override("font_size", UI_FONT_BODY)
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
		_hide_chart_chrome()
		var lb2 := Label.new()
		lb2.text = "No Data for this team size"
		lb2.add_theme_color_override("font_color", COLOR_TEXT)
		lb2.add_theme_font_size_override("font_size", UI_FONT_BODY)
		_chart_vbox.add_child(lb2)
		_title_label.text = "%s — %dv%d%s" % [
			_current_metric_label_string(),
			_current_size,
			_current_size,
			" (CI)" if use_ci else "",
		]
		return

	var display_data: Dictionary = data_context.duplicate(true)
	
	# Calculate K/D/A for roles on-the-fly from champion data
	if _view_mode == &"roles":
		var unit_data: Dictionary = _loader.all_stats.get(_current_size, {}).get("units", {})
		for role_key in display_data.keys():
			var role_role: String = str(role_key).to_lower()
			var total_kills: float = 0.0
			var total_deaths: float = 0.0
			var total_assists: float = 0.0
			var champion_count: int = 0
			
			for champion_key in unit_data.keys():
				var champion_role: String = str(_unit_roles.get(String(champion_key), "")).to_lower()
				if champion_role == role_role:
					var champ_data: Dictionary = unit_data[champion_key]
					total_kills += float(champ_data.get("kills", 0.0))
					total_deaths += float(champ_data.get("deaths", 0.0))
					total_assists += float(champ_data.get("assists", 0.0))
					champion_count += 1
			
			if champion_count > 0:
				display_data[role_key]["kills"] = total_kills
				display_data[role_key]["deaths"] = total_deaths
				display_data[role_key]["assists"] = total_assists
				display_data[role_key]["kda"] = (total_kills + total_assists) / maxf(1.0, total_deaths)
	
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
		_hide_chart_chrome()
		_title_label.text = "%s — %dv%d%s (no rows)" % [
			_current_metric_label_string(),
			_current_size,
			_current_size,
			" (CI)" if use_ci else "",
		]
		_update_balance_bar()
		_chart_vbox.custom_minimum_size.x = _chart_scroll_min_x(label_width)
		return

	_show_chart_chrome()
	_sync_chart_table_headers(label_width, is_synergy)
	var show_pct_axis: bool = _current_metric == &"winRate" or is_synergy
	if _axis_guides != null:
		_axis_guides.set_layout(
			label_width,
			float(chart_val_col_w),
			float(chart_games_col_w),
			float(chart_kda_col_w),
			float(chart_col_sep),
			float(UI_ROW_MARGIN)
		)
		_axis_guides.show_percent_guides = show_pct_axis
		_axis_guides.show_bar_origin_line = true
		_axis_guides.queue_redraw()

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

	_update_balance_bar()

	_chart_vbox.custom_minimum_size.x = _chart_scroll_min_x(label_width)
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
		row.mouse_filter = Control.MOUSE_FILTER_STOP
		var sb_row := StyleBoxFlat.new()
		sb_row.bg_color = COLOR_SECTION_BG if row_idx % 2 == 0 else Color(0, 0, 0, 0)
		sb_row.set_content_margin_all(UI_ROW_MARGIN)
		row.add_theme_stylebox_override("panel", sb_row)

		var hb := HBoxContainer.new()
		hb.size_flags_vertical = Control.SIZE_EXPAND_FILL
		hb.add_theme_constant_override("separation", chart_col_sep)
		row.add_child(hb)

		var name_w := Control.new()
		name_w.custom_minimum_size.x = label_width
		var name_lb := Label.new()
		name_lb.text = _display_name(key_s, is_synergy)
		name_lb.autowrap_mode = TextServer.AUTOWRAP_OFF
		name_lb.add_theme_font_size_override("font_size", UI_FONT_BODY)
		if not is_synergy and _view_mode == &"champions":
			var role: String = str(_unit_roles.get(key_s, ""))
			var rc: Color = ROLE_COLORS.get(role, COLOR_TEXT) as Color
			name_lb.add_theme_color_override("font_color", rc)
			var marker := ColorRect.new()
			marker.custom_minimum_size = Vector2(UI_ROLE_MARKER_S, UI_ROLE_MARKER_S)
			marker.color = rc
			var name_h := HBoxContainer.new()
			name_h.add_theme_constant_override("separation", UI_NAME_H_SEP)
			name_h.add_child(marker)
			name_h.add_child(name_lb)
			name_w.add_child(name_h)
		elif not is_synergy and _view_mode == &"roles":
			var role_key: String = str(key_s).to_lower()
			var rc_r: Color = ROLE_COLORS.get(role_key, COLOR_TEXT) as Color
			name_lb.add_theme_color_override("font_color", rc_r)
			var marker_r := ColorRect.new()
			marker_r.custom_minimum_size = Vector2(UI_ROLE_MARKER_S, UI_ROLE_MARKER_S)
			marker_r.color = rc_r
			var name_r := HBoxContainer.new()
			name_r.add_theme_constant_override("separation", UI_NAME_H_SEP)
			name_r.add_child(marker_r)
			name_r.add_child(name_lb)
			name_w.add_child(name_r)
		else:
			name_lb.add_theme_color_override("font_color", COLOR_TEXT)
			name_w.add_child(name_lb)
		hb.add_child(name_w)

		var bar_holder := StatsBarControlScript.new()
		bar_holder.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		bar_holder.custom_minimum_size = Vector2(chart_bar_holder_min_w, chart_row_bar_h)
		var fill_ratio: float = clampf(val / bar_scale_max, 0.0, 1.0)
		if (_current_metric == &"winRate" or is_synergy) and not use_ci:
			var tg: int = StatsDashboardLoaderScript.get_count(u_data)
			if tg > 0:
				fill_ratio = clampf(float(u_data.get("wins", 0)) / float(tg), 0.0, 1.0)
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
		bar_holder.set_visual(fill_ratio, bar_col, ci_lo_r, ci_hi_r, show_pct_axis, -1.0)
		hb.add_child(bar_holder)

		var count: int = (
			maxi(1, int(u_data.get("samples", 0)))
			if use_ci
			else StatsDashboardLoaderScript.get_count(u_data)
		)

		var val_lb := Label.new()
		val_lb.custom_minimum_size.x = chart_val_col_w
		val_lb.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		val_lb.add_theme_font_size_override("font_size", UI_FONT_BODY)
		if _current_metric == &"winRate" or is_synergy:
			val_lb.text = "%.2f%%" % (val * 100.0)
		elif _current_metric == &"kda":
			val_lb.text = "%.2f" % val
		else:
			val_lb.text = "%.0f" % val
		val_lb.add_theme_color_override("font_color", COLOR_TEXT)
		hb.add_child(val_lb)

		var games_lb := Label.new()
		games_lb.custom_minimum_size.x = chart_games_col_w
		games_lb.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		games_lb.add_theme_font_size_override("font_size", UI_FONT_BODY)
		games_lb.text = str(count)
		games_lb.add_theme_color_override("font_color", COLOR_SUBTLE)
		hb.add_child(games_lb)

		var kda_lb := Label.new()
		kda_lb.custom_minimum_size.x = chart_kda_col_w
		kda_lb.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		kda_lb.add_theme_font_size_override("font_size", UI_FONT_BODY)
		kda_lb.text = _format_kda_per_game_cells(u_data, count)
		kda_lb.add_theme_color_override("font_color", COLOR_SUBTLE)
		hb.add_child(kda_lb)

		var tt: String = _build_tooltip(key_s, u_data, use_ci, is_synergy)
		row.set_meta(&"tt", tt)
		row.set_meta(&"tt_border", _tooltip_border_for_row(key_s, is_synergy))
		row.mouse_entered.connect(_chart_row_tt_enter.bind(row))
		row.mouse_exited.connect(_chart_row_tt_exit)
		row.gui_input.connect(_chart_row_tt_motion)
		_set_row_insides_mouse_ignore(hb)

		_chart_vbox.add_child(row)
		row_idx += 1


func _format_kda_per_game_cells(u_data: Dictionary, count: int) -> String:
	var cf: float = float(maxi(1, count))
	var kk: float = float(u_data.get("kills", 0.0)) / cf
	var dd: float = float(u_data.get("deaths", 0.0)) / cf
	var aa: float = float(u_data.get("assists", 0.0)) / cf
	return "%.1f/%.1f/%.1f" % [kk, dd, aa]


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
	if _current_sort == &"kda":
		var ka: float = float(ua.get("kda", 0.0))
		var kb: float = float(ub.get("kda", 0.0))
		if ka != kb:
			return ka > kb if rev else ka < kb
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


func _update_balance_bar() -> void:
	var s: Dictionary = _loader.summary_stats.get(_current_size, {})
	_balance_bar.set_balance(
		int(s.get("p1", 0)),
		int(s.get("p2", 0)),
		int(s.get("draws", 0)),
		int(s.get("total", 0))
	)


func _escape_bbcode_plain(s: String) -> String:
	return str(s).replace("[", "[lb]").replace("]", "[rb]")


func _tooltip_entity_line_bbcode(key: String, display_name: String, is_synergy: bool) -> String:
	var safe_name: String = _escape_bbcode_plain(display_name)
	if is_synergy:
		return "%s" % [safe_name]
	var c: Color = COLOR_TEXT
	if _view_mode == &"champions":
		var rk: String = str(_unit_roles.get(key, ""))
		c = ROLE_COLORS.get(rk, COLOR_TEXT) as Color
	elif _view_mode == &"roles":
		c = ROLE_COLORS.get(str(key).to_lower(), COLOR_TEXT) as Color
	return "[color=%s]%s[/color]" % [c.to_html(false), safe_name]


func _build_tooltip(key: String, u_data: Dictionary, use_ci: bool, is_synergy: bool) -> String:
	var display_name: String = _display_name(key, is_synergy)
	var count: int = maxi(1, int(u_data.get("samples", 0))) if use_ci else StatsDashboardLoaderScript.get_count(u_data)
	var lines: PackedStringArray = PackedStringArray()
	var entity_caps := "CHAMPION"
	if is_synergy:
		entity_caps = "COMPOSITION"
	elif _view_mode == &"roles":
		entity_caps = "ROLE"
	if use_ci:
		var ci: Dictionary = u_data.get("ci", {})
		var wr: Array = ci.get("winRate", [u_data.get("winRate", 0.0), u_data.get("winRate", 0.0)])
		var dd: Array = ci.get("damage_dealt", [u_data.get("damage_dealt", 0.0), u_data.get("damage_dealt", 0.0)])
		var dr: Array = ci.get("damage_received", [u_data.get("damage_received", 0.0), u_data.get("damage_received", 0.0)])
		var dm: Array = ci.get("damage_mitigated", [u_data.get("damage_mitigated", 0.0), u_data.get("damage_mitigated", 0.0)])
		var hd: Array = ci.get("healing_done", [u_data.get("healing_done", 0.0), u_data.get("healing_done", 0.0)])
		lines.append(_tooltip_entity_line_bbcode(key, display_name, is_synergy))
		lines.append("Samples: %d" % int(count))
		lines.append("Win Rate: %.2f%% [%.2f%%, %.2f%%]" % [
			float(u_data.get("winRate", 0.0)) * 100.0,
			float(wr[0]) * 100.0,
			float(wr[1]) * 100.0,
		])
		lines.append("Average damage: %.0f [%.0f, %.0f]" % [
			float(u_data.get("damage_dealt", 0.0)),
			float(dd[0]),
			float(dd[1]),
		])
		lines.append("Average received: %.0f [%.0f, %.0f]" % [
			float(u_data.get("damage_received", 0.0)),
			float(dr[0]),
			float(dr[1]),
		])
		lines.append("Average mitigated: %.0f [%.0f, %.0f]" % [
			float(u_data.get("damage_mitigated", 0.0)),
			float(dm[0]),
			float(dm[1]),
		])
		lines.append("Average healing: %.0f [%.0f, %.0f]" % [
			float(u_data.get("healing_done", 0.0)),
			float(hd[0]),
			float(hd[1]),
		])
	else:
		lines.append(_tooltip_entity_line_bbcode(key, display_name, is_synergy))
		lines.append("Games Played: %d" % int(count))
		lines.append(
			"Win Rate: %.2f%%" % (float(u_data.get("winRate", 0.0)) * 100.0)
		)
		lines.append(
			"Record: %d-%d-%d (W-L-D)"
			% [int(u_data.get("wins", 0)), int(u_data.get("losses", 0)), int(u_data.get("draws", 0))]
		)
		lines.append("")
		var kills: float = float(u_data.get("kills", 0.0))
		var deaths: float = float(u_data.get("deaths", 0.0))
		var assists: float = float(u_data.get("assists", 0.0))
		var cf: float = float(count)
		lines.append(
			"K/D/A: %.1f/%.1f/%.1f (%.2f)"
			% [kills / cf, deaths / cf, assists / cf, float(u_data.get("kda", 0.0))]
		)
		lines.append(
			"Average damage: %.0f (Received: %.0f)"
			% [
				float(u_data.get("damage_dealt", 0.0)) / cf,
				float(u_data.get("damage_received", 0.0)) / cf,
			]
		)
		lines.append("Average mitigated: %.0f" % (float(u_data.get("damage_mitigated", 0.0)) / cf))
		lines.append(
			"Average healing: %.0f"
			% (float(u_data.get("healing_done", 0.0)) / cf)
		)
		lines.append(
			"Average shielding: %.0f"
			% (float(u_data.get("shielding_done", 0.0)) / cf)
		)
		lines.append("Stuns: %.1f" % (float(u_data.get("stuns", 0.0)) / cf))
		if u_data.has("breakdown"):
			var b: Dictionary = u_data["breakdown"]
			lines.append("")
			lines.append("Damage Dealt Breakdown:")
			lines.append(
				"Auto-attacks: %.0f | Abilities: %.0f | Ultimates: %.0f | Passive: %.0f"
				% [
					float(b.get("auto", 0.0)) / cf,
					float(b.get("ability", 0.0)) / cf,
					float(b.get("ultimate", 0.0)) / cf,
					float(b.get("passive", 0.0)) / cf,
				]
			)
			lines.append("")
			lines.append("Healing Done Breakdown:")
			lines.append(
				"Auto-attacks: %.0f | Abilities: %.0f | Ultimates: %.0f | Passive: %.0f"
				% [
					float(b.get("heal_auto", 0.0)) / cf,
					float(b.get("heal_ability", 0.0)) / cf,
					float(b.get("heal_ultimate", 0.0)) / cf,
					float(b.get("heal_passive", 0.0)) / cf,
				]
			)
			lines.append("")
			lines.append("Shielding Done Breakdown:")
			lines.append(
				"Auto-attacks: %.0f | Abilities: %.0f | Ultimates: %.0f | Passive: %.0f"
				% [
					float(b.get("shield_auto", 0.0)) / cf,
					float(b.get("shield_ability", 0.0)) / cf,
					float(b.get("shield_ultimate", 0.0)) / cf,
					float(b.get("shield_passive", 0.0)) / cf,
				]
			)
	for i in range(1, lines.size()):
		lines[i] = "  " + lines[i]
	return "\n".join(lines)


func _on_tab_changed(tab_index: int) -> void:
	# Hide right panel when switching to matchups tab, show for filters tab
	if _right_panel != null:
		_right_panel.visible = (tab_index == 0)


func _on_new_data_pressed() -> void:
	if _export_popup == null:
		_build_export_popup()
		add_child(_export_popup)  # Add to scene tree before showing
	_export_popup.popup_centered(Vector2i(700, 900))


func _build_export_popup() -> void:
	_export_popup = Window.new()
	_export_popup.title = "Generate New Data"
	_export_popup.min_size = Vector2i(700, 900)
	_export_popup.unresizable = false
	_export_popup.close_requested.connect(func(): _export_popup.hide())
	
	var main_vb := VBoxContainer.new()
	main_vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	main_vb.size_flags_vertical = Control.SIZE_EXPAND_FILL
	main_vb.add_theme_constant_override("separation", 20)
	_export_popup.add_child(main_vb)
	
	# Add export content
	_export_popup_content = VBoxContainer.new()
	_export_popup_content.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_export_popup_content.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_export_popup_content.add_theme_constant_override("separation", 18)
	main_vb.add_child(_export_popup_content)
	
	# Build export UI (moved from Matchups tab)
	_build_export_ui_content()
	
	# Close button
	var close_button := Button.new()
	close_button.text = "Close"
	close_button.custom_minimum_size = Vector2(140, 56)
	close_button.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	close_button.pressed.connect(func(): _export_popup.hide())
	main_vb.add_child(close_button)


func _build_export_ui_content() -> void:
	_export_popup_content.add_child(_section_label("EXPORT"))
	var export_card := PanelContainer.new()
	var export_card_style := StyleBoxFlat.new()
	export_card_style.bg_color = COLOR_SECTION_BG
	export_card_style.set_corner_radius_all(8)
	export_card_style.set_content_margin_all(16)
	export_card.add_theme_stylebox_override("panel", export_card_style)
	var export_inner := VBoxContainer.new()
	export_inner.add_theme_constant_override("separation", 18)
	export_card.add_child(export_inner)
	_export_popup_content.add_child(export_card)

	_native_required_notice = Label.new()
	_native_required_notice.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_native_required_notice.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_native_required_notice.add_theme_color_override("font_color", COLOR_RED)
	_native_required_notice.add_theme_font_size_override("font_size", UI_FONT_BODY)
	_native_required_notice.text = (
		"Native simulation is required for export and batch tooling. "
		+ "Build and place teamfight_simulation_core.dll in native/bin/."
	)
	_native_required_notice.visible = not _native_backend_ready()
	export_inner.add_child(_native_required_notice)

	var regen_hint := Label.new()
	regen_hint.text = "Select which modes to export. Each checked size runs the match count below."
	regen_hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	regen_hint.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	regen_hint.add_theme_color_override("font_color", COLOR_SUBTLE)
	regen_hint.add_theme_font_size_override("font_size", UI_FONT_BODY)
	export_inner.add_child(regen_hint)

	var modes_row := HFlowContainer.new()
	modes_row.add_theme_constant_override("h_separation", 12)
	modes_row.add_theme_constant_override("v_separation", 10)
	var sizes: Array = Array(SimConstantsScript.SIMULATION_TEAM_SIZES)
	for sz_idx in range(sizes.size()):
		var sz2: int = int(sizes[sz_idx])
		var cb := CheckBox.new()
		cb.text = "%dv%d" % [sz2, sz2]
		cb.custom_minimum_size.y = 56
		cb.button_pressed = true
		cb.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
		_regen_checks[sz2] = cb
		modes_row.add_child(cb)
	export_inner.add_child(modes_row)

	var sample_block := VBoxContainer.new()
	sample_block.add_theme_constant_override("separation", 8)
	var sample_lbl := Label.new()
	sample_lbl.text = "Matches per selected mode"
	sample_lbl.add_theme_color_override("font_color", COLOR_TEXT)
	sample_lbl.add_theme_font_size_override("font_size", UI_FONT_BODY)
	sample_block.add_child(sample_lbl)
	_regen_sample_edit = LineEdit.new()
	_regen_sample_edit.text = ""
	_regen_sample_edit.placeholder_text = "Integer ≥ 1"
	_regen_sample_edit.custom_minimum_size = Vector2(0, 56)
	_regen_sample_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sample_block.add_child(_regen_sample_edit)
	export_inner.add_child(sample_block)

	var worker_block := VBoxContainer.new()
	worker_block.add_theme_constant_override("separation", 8)
	var worker_max := maxi(
		1,
		mini(
			OS.get_processor_count(),
			StatsSimulationCsvGeneratorScript.DEFAULT_EXPORT_MAX_WORKER_THREADS
		)
	)
	var worker_lbl := Label.new()
	worker_lbl.text = "Worker threads (< %d)" % worker_max
	worker_lbl.add_theme_color_override("font_color", COLOR_TEXT)
	worker_lbl.add_theme_font_size_override("font_size", UI_FONT_BODY)
	worker_block.add_child(worker_lbl)
	_regen_worker_edit = LineEdit.new()
	_regen_worker_edit.text = str(_default_export_worker_threads())
	_regen_worker_edit.placeholder_text = "Integer >= 0"
	_regen_worker_edit.custom_minimum_size = Vector2(0, 56)
	_regen_worker_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	worker_block.add_child(_regen_worker_edit)
	export_inner.add_child(worker_block)

	_regen_button = Button.new()
	_regen_button.text = "Generate"
	_regen_button.tooltip_text = "Regenerate stats CSVs (native sim)"
	_regen_button.clip_text = true
	_regen_button.custom_minimum_size = Vector2(0, 56)
	_regen_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_regen_button.pressed.connect(_on_regenerate_pressed)
	export_inner.add_child(_regen_button)
	_regen_progress = ProgressBar.new()
	_regen_progress.visible = false
	_regen_progress.min_value = 0.0
	_regen_progress.max_value = 1.0
	_regen_progress.custom_minimum_size.y = 40
	_regen_progress.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	export_inner.add_child(_regen_progress)
	_regen_status = Label.new()
	_regen_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_regen_status.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_regen_status.add_theme_color_override("font_color", COLOR_SUBTLE)
	_regen_status.add_theme_font_size_override("font_size", UI_FONT_BODY)
	export_inner.add_child(_regen_status)


func _build_matchup_ui() -> void:
	# Load matchup data
	_matchup_loader.load_data()
	
	# Create main layout
	var main_hb := HBoxContainer.new()
	main_hb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	main_hb.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_matchup_vb.add_child(main_hb)
	
	# Left sidebar
	var left_panel := VBoxContainer.new()
	left_panel.custom_minimum_size.x = UI_SIDEBAR_MIN_W
	left_panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	left_panel.size_flags_stretch_ratio = 0.0
	left_panel.add_theme_constant_override("separation", 12)
	main_hb.add_child(left_panel)
	
	# Champion selector
	left_panel.add_child(_section_label("CHAMPION"))
	var champion_search := LineEdit.new()
	champion_search.placeholder_text = "Search champion..."
	champion_search.custom_minimum_size.y = UI_MIN_CONTROL_H
	champion_search.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	champion_search.text_changed.connect(_on_champion_search_changed)
	left_panel.add_child(champion_search)
	
	var champion_list := ItemList.new()
	champion_list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	champion_list.size_flags_vertical = Control.SIZE_EXPAND_FILL
	left_panel.add_child(champion_list)
	champion_list.item_selected.connect(_on_champion_selected)
	
	# Populate champion list
	for champion in _matchup_loader.champions:
		champion_list.add_item(champion)
	
	# View mode
	left_panel.add_child(_section_label("VIEW MODE"))
	var view_mode := OptionButton.new()
	view_mode.custom_minimum_size.y = UI_MIN_CONTROL_H
	view_mode.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	view_mode.add_item("Counters (vs)")
	view_mode.add_item("Synergies (with)")
	view_mode.add_item("Both")
	view_mode.item_selected.connect(_on_view_mode_changed)
	left_panel.add_child(view_mode)
	
	# Sort options
	left_panel.add_child(_section_label("SORT BY"))
	var sort_mode := OptionButton.new()
	sort_mode.custom_minimum_size.y = UI_MIN_CONTROL_H
	sort_mode.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sort_mode.add_item("Winrate (High to Low)")
	sort_mode.add_item("Winrate (Low to High)")
	sort_mode.add_item("Wins")
	sort_mode.add_item("Losses")
	sort_mode.add_item("Alphabetical")
	sort_mode.item_selected.connect(_on_sort_mode_changed)
	left_panel.add_child(sort_mode)
	
	# Refresh button
	_refresh_btn = Button.new()
	_refresh_btn.text = "Refresh Data"
	_refresh_btn.custom_minimum_size.y = UI_MIN_CONTROL_H
	_refresh_btn.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_refresh_btn.pressed.connect(_refresh_matchup_data)
	left_panel.add_child(_refresh_btn)
	
	# Right content area
	var right_panel := VBoxContainer.new()
	right_panel.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
	right_panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	right_panel.add_theme_constant_override("separation", 16)
	main_hb.add_child(right_panel)
	
	# Store reference for deferred call
	_matchup_right_panel = right_panel
	call_deferred("_set_matchup_right_panel_to_center")
	
	# Content will be populated when champion is selected
	_show_matchup_placeholder(right_panel)


func _set_matchup_right_panel_to_center() -> void:
	await get_tree().process_frame
	if _matchup_right_panel == null:
		return
	var viewport_width: int = get_viewport().get_visible_rect().size.x
	var center_width: int = viewport_width / 2
	var right_panel_width: int = center_width - UI_SIDEBAR_MIN_W
	_matchup_right_panel.custom_minimum_size.x = right_panel_width


func _show_matchup_error(error_message: String) -> void:
	var error_label := Label.new()
	error_label.text = "Error: " + error_message
	error_label.add_theme_color_override("font_color", COLOR_RED)
	error_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_matchup_vb.add_child(error_label)


func _show_matchup_placeholder(parent: Control) -> void:
	var placeholder := Label.new()
	placeholder.text = "Select a champion to view matchup analysis"
	placeholder.add_theme_color_override("font_color", COLOR_SUBTLE)
	placeholder.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	placeholder.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	placeholder.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	placeholder.size_flags_vertical = Control.SIZE_EXPAND_FILL
	parent.add_child(placeholder)


func _on_champion_search_changed(text: String) -> void:
	pass  # TODO: Implement search filtering


func _on_champion_selected(index: int) -> void:
	if index < 0 or index >= _matchup_loader.champions.size():
		return
	
	_current_champion = _matchup_loader.champions[index]
	_update_matchup_display()


func _on_view_mode_changed(index: int) -> void:
	_current_view_mode = index
	_update_matchup_display()


func _on_sort_mode_changed(index: int) -> void:
	_current_sort_mode = index
	_update_matchup_display()


func _update_matchup_display() -> void:
	if _current_champion.is_empty():
		return
	
	# Ensure UI structure exists
	if _matchup_vb.get_child_count() == 0:
		_build_matchup_ui()
	
	# Clear existing content
	var right_panel = _matchup_vb.get_child(0).get_child(1)  # Get right panel
	for child in right_panel.get_children():
		child.queue_free()
	
	# Get matchup data
	var matchups = _matchup_loader.get_champion_matchups(_current_champion)
	
	# Create header
	var header := Label.new()
	header.text = "Matchup Analysis: " + _current_champion.to_upper()
	header.add_theme_color_override("font_color", COLOR_TEXT)
	header.add_theme_font_size_override("font_size", UI_FONT_SECTION)
	right_panel.add_child(header)
	
	# Create content based on view mode
	match _current_view_mode:
		0:  # Counters (vs)
			_show_vs_matchups(right_panel, matchups)
		1:  # Synergies (with)
			_show_with_matchups(right_panel, matchups)
		2:  # Both
			_show_both_matchups(right_panel, matchups)


func _show_vs_matchups(parent: Control, matchups: Dictionary) -> void:
	var vs_data = matchups.get("vs", {})
	if vs_data.is_empty():
		_show_no_data(parent, "No counter matchup data available")
		return
	
	# Summary section
	var summary_card := _create_summary_card("COUNTER ANALYSIS", _current_champion, "vs")
	parent.add_child(summary_card)
	
	# Matchup table
	var table := _create_matchup_table(vs_data, "vs")
	parent.add_child(table)


func _show_with_matchups(parent: Control, matchups: Dictionary) -> void:
	var with_data = matchups.get("with", {})
	if with_data.is_empty():
		_show_no_data(parent, "No synergy matchup data available")
		return
	
	# Summary section
	var summary_card := _create_summary_card("SYNERGY ANALYSIS", _current_champion, "with")
	parent.add_child(summary_card)
	
	# Matchup table
	var table := _create_matchup_table(with_data, "with")
	parent.add_child(table)


func _show_both_matchups(parent: Control, matchups: Dictionary) -> void:
	var vs_data = matchups.get("vs", {})
	var with_data = matchups.get("with", {})
	
	if vs_data.is_empty() and with_data.is_empty():
		_show_no_data(parent, "No matchup data available")
		return
	
	# Create horizontal container for side-by-side layout
	var both_hb := HBoxContainer.new()
	both_hb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	both_hb.size_flags_vertical = Control.SIZE_EXPAND_FILL
	both_hb.add_theme_constant_override("separation", 16)
	parent.add_child(both_hb)
	
	# Synergies section (left side)
	if not with_data.is_empty():
		var with_vb := VBoxContainer.new()
		with_vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		with_vb.size_flags_vertical = Control.SIZE_EXPAND_FILL
		with_vb.size_flags_stretch_ratio = 1.0
		with_vb.add_theme_constant_override("separation", 12)
		both_hb.add_child(with_vb)
		
		var with_summary := _create_summary_card("SYNERGY ANALYSIS", _current_champion, "with")
		with_vb.add_child(with_summary)
		var with_table := _create_matchup_table(with_data, "with")
		with_vb.add_child(with_table)
	
	# Counters section (right side)
	if not vs_data.is_empty():
		var vs_vb := VBoxContainer.new()
		vs_vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		vs_vb.size_flags_vertical = Control.SIZE_EXPAND_FILL
		vs_vb.size_flags_stretch_ratio = 1.0
		vs_vb.add_theme_constant_override("separation", 12)
		both_hb.add_child(vs_vb)
		
		var vs_summary := _create_summary_card("COUNTER ANALYSIS", _current_champion, "vs")
		vs_vb.add_child(vs_summary)
		var vs_table := _create_matchup_table(vs_data, "vs")
		vs_vb.add_child(vs_table)


func _create_summary_card(title: String, champion: String, matchup_type: String) -> PanelContainer:
	var card := PanelContainer.new()
	var style := StyleBoxFlat.new()
	style.bg_color = COLOR_SECTION_BG
	style.set_corner_radius_all(8)
	style.set_content_margin_all(12)
	card.add_theme_stylebox_override("panel", style)
	
	var content := VBoxContainer.new()
	content.add_theme_constant_override("separation", 8)
	card.add_child(content)
	
	# Title
	var title_label := Label.new()
	title_label.text = title
	title_label.add_theme_color_override("font_color", COLOR_TEXT)
	title_label.add_theme_font_size_override("font_size", UI_FONT_SECTION - 2)
	content.add_child(title_label)
	
	# Get summary data
	var best = []
	var worst = []
	
	if matchup_type == "vs":
		best = _matchup_loader.get_best_counters(champion, 3)
		worst = _matchup_loader.get_weak_against(champion, 3)
	else:  # with
		best = _matchup_loader.get_best_synergies(champion, 3)
		worst = _matchup_loader.get_poor_synergies(champion, 3)
	
	# Best section
	var best_label := Label.new()
	best_label.text = "Best " + ("Counters" if matchup_type == "vs" else "Synergies") + ":"
	best_label.add_theme_color_override("font_color", COLOR_GREEN)
	content.add_child(best_label)
	
	if not best.is_empty():
		for item in best:
			var item_label := Label.new()
			item_label.text = "  • %s (%.2f%%)" % [item.name, item.winrate * 100]
			item_label.add_theme_color_override("font_color", COLOR_SUBTLE)
			content.add_child(item_label)
	else:
		var none_label := Label.new()
		none_label.text = "  None"
		none_label.add_theme_color_override("font_color", COLOR_SUBTLE)
		content.add_child(none_label)
	
	# Worst section
	var worst_label := Label.new()
	worst_label.text = "Worst " + ("Counters" if matchup_type == "vs" else "Synergies") + ":"
	worst_label.add_theme_color_override("font_color", COLOR_RED)
	content.add_child(worst_label)
	
	if not worst.is_empty():
		for item in worst:
			var item_label := Label.new()
			item_label.text = "  • %s (%.2f%%)" % [item.name, item.winrate * 100]
			item_label.add_theme_color_override("font_color", COLOR_SUBTLE)
			content.add_child(item_label)
	else:
		var none_label := Label.new()
		none_label.text = "  None"
		none_label.add_theme_color_override("font_color", COLOR_SUBTLE)
		content.add_child(none_label)
	
	return card


func _create_matchup_table(data: Dictionary, matchup_type: String) -> Control:
	var main_container := VBoxContainer.new()
	main_container.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	main_container.size_flags_vertical = Control.SIZE_EXPAND_FILL
	
	# Fixed header
	var header_row := HBoxContainer.new()
	header_row.add_theme_constant_override("separation", 8)
	
	var opponent_label := Label.new()
	opponent_label.text = ("Opponent" if matchup_type == "vs" else "Ally").to_upper()
	opponent_label.custom_minimum_size.x = 150
	opponent_label.add_theme_color_override("font_color", COLOR_TEXT)
	header_row.add_child(opponent_label)
	
	var wins_label := Label.new()
	wins_label.text = "WINS"
	wins_label.custom_minimum_size.x = 80
	wins_label.add_theme_color_override("font_color", COLOR_TEXT)
	header_row.add_child(wins_label)
	
	var losses_label := Label.new()
	losses_label.text = "LOSSES"
	losses_label.custom_minimum_size.x = 80
	losses_label.add_theme_color_override("font_color", COLOR_TEXT)
	header_row.add_child(losses_label)
	
	var winrate_label := Label.new()
	winrate_label.text = "WINRATE"
	winrate_label.custom_minimum_size.x = 100
	winrate_label.add_theme_color_override("font_color", COLOR_TEXT)
	header_row.add_child(winrate_label)
	
	main_container.add_child(header_row)
	
	# Scrollable data area
	var scroll := ScrollContainer.new()
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	main_container.add_child(scroll)
	
	var table := VBoxContainer.new()
	table.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(table)
	
	# Sort data with special logic for counter matchups
	var sorted_items = _sort_matchup_data(data, matchup_type)
	
	# Data rows
	for item in sorted_items:
		var row := HBoxContainer.new()
		row.add_theme_constant_override("separation", 8)
		
		var name_label := Label.new()
		name_label.text = item.name
		name_label.custom_minimum_size.x = 150
		name_label.add_theme_color_override("font_color", COLOR_TEXT)
		row.add_child(name_label)
		
		var data_wins_label := Label.new()
		data_wins_label.text = str(item.wins)
		data_wins_label.custom_minimum_size.x = 80
		data_wins_label.add_theme_color_override("font_color", COLOR_SUBTLE)
		row.add_child(data_wins_label)
		
		var data_losses_label := Label.new()
		data_losses_label.text = str(item.losses)
		data_losses_label.custom_minimum_size.x = 80
		data_losses_label.add_theme_color_override("font_color", COLOR_SUBTLE)
		row.add_child(data_losses_label)
		
		var data_winrate_label := Label.new()
		data_winrate_label.text = "%.2f%%" % (item.winrate * 100)
		data_winrate_label.custom_minimum_size.x = 100
		var color = _get_winrate_color(item.winrate)
		data_winrate_label.add_theme_color_override("font_color", color)
		row.add_child(data_winrate_label)
		
		table.add_child(row)
	
	return main_container


func _sort_matchup_data(data: Dictionary, matchup_type: String) -> Array:
	var items := []
	
	for opponent_name in data.keys():
		var matchup = data[opponent_name]
		items.append({
			"name": opponent_name,
			"wins": matchup.wins,
			"losses": matchup.losses,
			"winrate": matchup.winrate
		})
	
	match _current_sort_mode:
		0:  # Winrate (High to Low)
			items.sort_custom(func(a, b): return a.winrate > b.winrate)
		1:  # Winrate (Low to High)
			items.sort_custom(func(a, b): return a.winrate < b.winrate)
		2:  # Wins
			items.sort_custom(func(a, b): return a.wins > b.wins)
		3:  # Losses
			items.sort_custom(func(a, b): return a.losses > b.losses)
		4:  # Alphabetical
			items.sort_custom(func(a, b): return a.name < b.name)
	
	return items


func _get_winrate_color(winrate: float) -> Color:
	if winrate >= 0.6:
		return COLOR_GREEN
	elif winrate >= 0.4:
		return COLOR_SUBTLE
	else:
		return COLOR_RED


func _show_no_data(parent: Control, message: String) -> void:
	var no_data := Label.new()
	no_data.text = message
	no_data.add_theme_color_override("font_color", COLOR_SUBTLE)
	no_data.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	no_data.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	no_data.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	no_data.size_flags_vertical = Control.SIZE_EXPAND_FILL
	parent.add_child(no_data)


func _refresh_matchup_data() -> void:
	var loaded = _matchup_loader.load_data()
	
	# Rebuild the matchup UI to update champion dropdown
	if loaded and _matchup_vb != null:
		# Clear existing UI
		for child in _matchup_vb.get_children():
			child.queue_free()
		# Rebuild with new data
		_build_matchup_ui()


func _on_back_to_menu() -> void:
	if ResourceLoader.exists("res://scenes/game_root.tscn"):
		get_tree().change_scene_to_file("res://scenes/game_root.tscn")
	else:
		push_error("Cannot find game_root.tscn file")
