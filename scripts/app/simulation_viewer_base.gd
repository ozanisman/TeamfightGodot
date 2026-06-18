class_name SimulationViewerBase
extends Control

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const ViewerUnitNodeScript := preload("res://scripts/app/viewer_unit_node.gd")
const SimulationViewerArenaScript := preload("res://scripts/app/simulation_viewer_arena.gd")
const AoeRingNodeScript := preload("res://scripts/app/aoe_ring_node.gd")
const RosterChampionCardScript := preload("res://scripts/app/roster_champion_card.gd")
const UiTokensScript := preload("res://scripts/ui/ui_tokens.gd")
const UiStylesScript := preload("res://scripts/ui/ui_styles.gd")
const DraftLayoutScript := preload("res://scripts/ui/draft_layout.gd")
const DraftChampionTileScene := preload("res://scenes/components/draft_champion_tile.tscn")
const DraftScreenShellScene := preload("res://scenes/components/draft_screen_shell.tscn")

## World units map to this fraction of min(viewport) for a consistent projectile read.
const VIEWER_BASE_MIN_AXIS: float = 720.0
## Screen-space radius for projectiles (scales slightly with window).
const PROJECTILE_BASE_RADIUS_PX: float = 4.0

const ROSTER_EDGE_MARGIN_PX: float = 12.0
const ROSTER_COLUMN_WIDTH_PX: float = 240.0
const ROSTER_TOP_BELOW_HUD_PX: float = 64.0
const ROSTER_BOTTOM_MARGIN_PX: float = 16.0
const BOTTOM_BAR_HEIGHT_PX: float = 60.0
const COMMENCE_BATTLE_MIN_WIDTH_PX: float = 240.0
const BOTTOM_HBOX_MARGIN_H_PX: float = 10.0
const BOTTOM_HBOX_MARGIN_V_PX: float = 8.0
const ROSTER_CARD_GAP_PX: int = 6
const ROSTER_TILE_START_PX: int = 96
## Keep in sync with roster_champion_card.gd MIN_SQUARE_PX.
const ROSTER_TILE_MIN_SQUARE_PX: int = 40
const UI_WINDOW_MIN := Vector2(1920, 1080)

# Game states
enum GameState {
	DRAFTING,
	PREPARATION,
	COMBAT,
	MATCH_OVER
}

# Draft configuration - Modified for manual selection of both teams
const MAX_TEAM_SIZE := 5
const AI_SELECTION_TEMPERATURE: float = 0.5  # Softmax temperature (0.3=very top-heavy, 0.5=top-heavy, 1.0=balanced, 2.0=random)
const AI_SCORE_SCALE: float = 100.0  # Multiply raw scores before softmax so small-magnitude differences matter

# Which side is human vs AI. Can be "blue" or "red".
var _player_side: String = "blue"
var _ai_side: String = "red"

# Simulation configuration
var _backend: RefCounted = null
var _game_state: GameState = GameState.DRAFTING
var _sim_time_accumulator: float = 0.0
var _sim_speed: float = 1.0
var _combat_paused: bool = false

# Draft state
var _player_picks: Array[StringName] = []
var _enemy_picks: Array[StringName] = []
var _draft_step_index: int = 0
var _ai_draft_processing: bool = false

# Unit visualization
var _unit_nodes: Dictionary = {}  # instance_id -> Node2D
var _projectile_nodes: Dictionary = {}  # id -> Node2D
var _projectile_offsets: Dictionary = {}  # projectile_id -> Vector2 (cached spread offsets)
var _projectile_slot_usage: Dictionary = {}  # pool_key -> Array[int] (used slot indices per pool)
var _projectile_slot_owners: Dictionary = {}  # projectile_id -> Dictionary {"slot": int, "pool": String}
var _floating_texts: Array[Node] = []  # Array of floating text nodes
var _hot_status_rings: Dictionary = {}  # instance_id -> Node2D (current HoT ring per unit)
var _passive_aoe_rings: Dictionary = {}  # instance_id -> Array[Node2D] (persistent passive AOE rings)
var _active_tweens: Array = []  # Array of active Tween nodes

# Selection
var _selected_unit_id: int = 0
var _debug_mode: bool = true

# Draft filtering
var _active_role_filters: Array[StringName] = []
var _player_bans: Array[StringName] = []
var _enemy_bans: Array[StringName] = []
var _base_champion_tile_size: int = 0

# UI references
var _world_layer: Node2D
## Layer 1: AoE / impact rings above world.
var _aoe_fx_layer: CanvasLayer
var _ui_layer: Control
var _header_panel: Panel
var _grid_panel: Panel
var _title_label: Label
var _turn_label: Label
var _player_team_label: Label
var _player_team_list: VBoxContainer
var _player_team_panel: Panel
var _player_bans_label: Label
var _player_bans_list: VBoxContainer
var _player_bans_panel: Panel
var _enemy_team_label: Label
var _enemy_team_list: VBoxContainer
var _enemy_team_panel: Panel
var _enemy_bans_label: Label
var _enemy_bans_list: VBoxContainer
var _enemy_bans_panel: Panel
var _role_filter_container: HBoxContainer
var _champion_grid: GridContainer
var _champion_grid_ref: GridContainer
## Catalog overlay tooltips in draft / combat; null until [method _init_champion_catalog_tooltip] runs.
var _champ_catalog_tt: Node = null
var _random_draft_button: Button
var _start_match_button: Button
var _control_panel: Panel
var _inspection_panel: Panel
var _arena_layer: Node2D
## Full-viewport under world; do not use root Control _draw (sorts over child Node2D with low z, hides grid).
var _battle_letterbox: ColorRect
var _commence_button: Button
var _match_overlay: ColorRect
var _match_title: Label
var _match_stats_container: VBoxContainer
var _lbl_timer: Label
var _lbl_score: Label
var _lbl_combat_state: Label
var _hud_pause: Label
var _resize_debounce_timer: Timer
var _speed_button: Button
var _p1_roster_labels: VBoxContainer
var _p2_roster_labels: VBoxContainer
var _overtime_border: Panel
var _draft_action_box: VBoxContainer
var _champion_scroll: ScrollContainer
var _draft_shell: Control


func _ready() -> void:
	_backend = NativeSimulationBackendScript.new()
	if not _backend.is_available():
		push_error("Native simulation backend not available. Simulation viewer requires native backend.")
		return

	# Set window minimum size and maximize on startup
	custom_minimum_size = UI_WINDOW_MIN
	var win: Window = get_viewport().get_window()
	if win != null:
		win.min_size = UI_WINDOW_MIN
		win.mode = Window.MODE_MAXIMIZED

	# Create UI elements programmatically to avoid scene instantiation issues
	_create_ui_structure()

	_apply_color_scheme()
	_setup_draft_ui()
	_setup_control_panel()
	_setup_inspection_panel()
	_setup_resize_debounce_timer()
	_update_turn_display()
	_update_team_rosters()
	_update_start_match_enabled()

	if _debug_mode:
		print("Simulation Viewer ready. Game state: ", GameState.keys()[_game_state])

	_on_ready_extra()


func _on_ready_extra() -> void:
	pass


func _on_before_battle_start() -> void:
	pass


func _on_draft_shell_created() -> void:
	pass


func _notification(what: int) -> void:
	# When this Control is resized (layout or embedded viewport), keep UILayer matching our rect;
	# root size_changed can miss some cases.
	if what == NOTIFICATION_RESIZED and _ui_layer != null and is_instance_valid(_ui_layer):
		_ui_layer.size = size


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_SPACE and (_game_state == GameState.COMBAT or _game_state == GameState.PREPARATION):
			_toggle_pause()
			get_viewport().set_input_as_handled()
		if event.keycode == KEY_K and (_game_state == GameState.COMBAT or _game_state == GameState.PREPARATION):
			_on_speed_toggle()
			get_viewport().set_input_as_handled()


func _on_speed_toggle() -> void:
	_sim_speed = 0.5 if is_equal_approx(_sim_speed, 1.0) else 1.0
	if _speed_button != null:
		_speed_button.text = "Speed: 0.5x" if _sim_speed < 0.75 else "Speed: 1x"
	_update_tween_speed_scale()


func _create_ui_structure() -> void:
	var screen_size := get_viewport_rect().size

	_battle_letterbox = ColorRect.new()
	_battle_letterbox.name = "BattleLetterbox"
	_battle_letterbox.set_anchors_preset(Control.PRESET_FULL_RECT)
	_battle_letterbox.offset_left = 0.0
	_battle_letterbox.offset_top = 0.0
	_battle_letterbox.offset_right = 0.0
	_battle_letterbox.offset_bottom = 0.0
	_battle_letterbox.color = UiTokensScript.COLOR_BG
	_battle_letterbox.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_battle_letterbox.z_index = UiTokensScript.Z_BACKGROUND
	add_child(_battle_letterbox)

	# Create WorldLayer for combat visualization (behind UI)
	_world_layer = Node2D.new()
	_world_layer.name = "WorldLayer"
	_world_layer.position = Vector2(0.0, 0.0)
	_world_layer.visible = false
	add_child(_world_layer)

	_arena_layer = SimulationViewerArenaScript.new()
	_arena_layer.name = "ArenaGrid"
	_world_layer.add_child(_arena_layer)

	_aoe_fx_layer = CanvasLayer.new()
	_aoe_fx_layer.name = "AoeFxLayer"
	_aoe_fx_layer.layer = 1
	_aoe_fx_layer.visible = false
	add_child(_aoe_fx_layer)

	# Create UILayer for UI elements (in front of world)
	_ui_layer = Control.new()
	_ui_layer.name = "UILayer"
	_ui_layer.anchors_preset = Control.PRESET_FULL_RECT
	_ui_layer.offset_left = 0.0
	_ui_layer.offset_top = 0.0
	_ui_layer.offset_right = 0.0
	_ui_layer.offset_bottom = 0.0
	_ui_layer.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(_ui_layer)
	_ui_layer.size = get_viewport_rect().size
	_init_champion_catalog_tooltip()

	_create_draft_shell(screen_size)

	# Connect to window resize signal
	get_tree().root.connect("size_changed", _on_window_resized)
	_update_battle_layer_layout()

	# Combat HUD elements (hidden initially)
	_control_panel = Panel.new()
	_control_panel.name = "ControlPanel"
	_control_panel.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	_control_panel.offset_left = 0.0
	_control_panel.offset_top = -BOTTOM_BAR_HEIGHT_PX
	_control_panel.offset_right = 0.0
	_control_panel.offset_bottom = 0.0
	_control_panel.custom_minimum_size = Vector2(0.0, BOTTOM_BAR_HEIGHT_PX)
	_control_panel.visible = false
	_ui_layer.add_child(_control_panel)

	# InspectionPanel
	_inspection_panel = Panel.new()
	_inspection_panel.name = "InspectionPanel"
	_inspection_panel.position = Vector2(10.0, 10.0)
	_inspection_panel.custom_minimum_size = Vector2(240.0, 290.0)
	_inspection_panel.visible = false
	_ui_layer.add_child(_inspection_panel)

	_build_combat_overlay_hud()
	_build_match_over_overlay()
	_build_preparation_controls()
	if _battle_letterbox != null and is_instance_valid(_battle_letterbox):
		move_child(_battle_letterbox, 0)


func _create_draft_shell(screen_size: Vector2) -> void:
	_draft_shell = DraftScreenShellScene.instantiate() as Control
	if _draft_shell == null:
		push_error("DraftScreenShell instantiate failed")
		return
	_ui_layer.add_child(_draft_shell)
	_draft_shell.apply_layout(screen_size)
	_draft_shell.back_pressed.connect(_on_back_to_menu)
	_draft_shell.auto_fill_pressed.connect(_on_random_draft_clicked)
	_draft_shell.start_battle_pressed.connect(_on_start_match_clicked)

	_header_panel = _draft_shell.draft_panel
	_title_label = _draft_shell.title_label
	_turn_label = _draft_shell.turn_label
	if _draft_shell.debug_label != null:
		_draft_shell.debug_label.text = "DEBUG: ON | STACK: ON" if _debug_mode else "DEBUG MODE: OFF"
	_player_team_panel = _draft_shell.player_team_panel
	_player_team_label = _draft_shell.player_team_label
	_player_team_list = _draft_shell.player_team_list
	_player_bans_panel = _draft_shell.player_bans_panel
	_player_bans_label = _draft_shell.player_bans_label
	_player_bans_list = _draft_shell.player_bans_list
	_enemy_team_panel = _draft_shell.enemy_team_panel
	_enemy_team_label = _draft_shell.enemy_team_label
	_enemy_team_list = _draft_shell.enemy_team_list
	_enemy_bans_panel = _draft_shell.enemy_bans_panel
	_enemy_bans_label = _draft_shell.enemy_bans_label
	_enemy_bans_list = _draft_shell.enemy_bans_list
	_role_filter_container = _draft_shell.get_role_filter_container()
	_draft_action_box = _draft_shell.draft_action_box
	_random_draft_button = _draft_shell.auto_fill_button
	_start_match_button = _draft_shell.start_battle_button
	_champion_scroll = _draft_shell.champion_scroll
	_champion_grid = _draft_shell.get_champion_grid()
	_champion_grid_ref = _champion_grid

	_on_draft_shell_created()


## Viewer Control space (same as get_viewport); centered square, letterbox outside.
func world_to_screen(wx: float, wy: float) -> Vector2:
	return world_to_battle_local(wx, wy) + _viewer_battle_offset()


## Local 0..s in world layer; parent places layer at _viewer_battle_offset().
func world_to_battle_local(wx: float, wy: float) -> Vector2:
	var vp: Vector2 = get_viewport_rect().size
	var s: float = SimConstantsScript.viewer_battle_square_side(vp)
	var wxc: float = clampf(wx, 0.0, SimConstantsScript.WORLD_SIZE)
	var wyc: float = clampf(wy, 0.0, SimConstantsScript.WORLD_SIZE)
	return Vector2((wxc / SimConstantsScript.WORLD_SIZE) * s, (wyc / SimConstantsScript.WORLD_SIZE) * s)


func _viewer_battle_offset() -> Vector2:
	return SimConstantsScript.viewer_battle_square_offset(get_viewport_rect().size)


## Keeps _WorldLayer top-left on the square; re-draws arena in local s×s.
func _update_battle_layer_layout() -> void:
	if _world_layer == null or not is_instance_valid(_world_layer):
		return
	_world_layer.position = _viewer_battle_offset()
	if _arena_layer != null and is_instance_valid(_arena_layer) and _arena_layer.has_method("refresh"):
		_arena_layer.refresh()


func screen_to_world(screen: Vector2) -> Vector2:
	var vp: Vector2 = get_viewport_rect().size
	var s: float = SimConstantsScript.viewer_battle_square_side(vp)
	if s < 0.001:
		return Vector2.ZERO
	var off: Vector2 = SimConstantsScript.viewer_battle_square_offset(vp)
	var w: float = SimConstantsScript.WORLD_SIZE
	return Vector2(
		clampf((screen.x - off.x) / s, 0.0, 1.0) * w,
		clampf((screen.y - off.y) / s, 0.0, 1.0) * w
	)


func _build_combat_overlay_hud() -> void:
	# One centered strip: time, score, combat state, pause, speed. CenterContainer recenters on resize.
	const HUD_TOP_OFFSET_PX: int = 8
	const HUD_STRIP_MIN_HEIGHT_PX: int = 48
	const HUD_ROW_SEP_PX: int = 16
	var top := Control.new()
	top.name = "CombatHUD"
	top.visible = false
	top.set_anchors_preset(Control.PRESET_TOP_WIDE)
	top.offset_top = float(HUD_TOP_OFFSET_PX)
	top.offset_bottom = float(HUD_TOP_OFFSET_PX + HUD_STRIP_MIN_HEIGHT_PX)
	_ui_layer.add_child(top)

	var center := CenterContainer.new()
	center.set_anchors_preset(Control.PRESET_FULL_RECT)
	top.add_child(center)

	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_theme_constant_override("separation", HUD_ROW_SEP_PX)
	_lbl_timer = Label.new()
	row.add_child(_lbl_timer)
	_lbl_score = Label.new()
	_lbl_score.add_theme_color_override("font_color", UiTokensScript.COLOR_WARNING)
	row.add_child(_lbl_score)
	_lbl_combat_state = Label.new()
	_lbl_combat_state.add_theme_color_override("font_color", UiTokensScript.COLOR_SUBTLE)
	row.add_child(_lbl_combat_state)
	_hud_pause = Label.new()
	_hud_pause.visible = false
	_hud_pause.add_theme_color_override("font_color", UiTokensScript.COLOR_WARNING)
	row.add_child(_hud_pause)
	_speed_button = Button.new()
	_speed_button.text = "Speed: 1x"
	_speed_button.tooltip_text = "Toggle 0.5x / 1x (keyboard: K)"
	_speed_button.flat = true
	_speed_button.pressed.connect(_on_speed_toggle)
	row.add_child(_speed_button)
	center.add_child(row)

	var side_l := VBoxContainer.new()
	side_l.name = "P1RosterHUD"
	side_l.visible = false
	side_l.anchor_left = 0.0
	side_l.anchor_top = 0.0
	side_l.anchor_right = 0.0
	side_l.anchor_bottom = 1.0
	side_l.offset_left = ROSTER_EDGE_MARGIN_PX
	side_l.offset_top = ROSTER_TOP_BELOW_HUD_PX
	side_l.offset_right = ROSTER_EDGE_MARGIN_PX + ROSTER_COLUMN_WIDTH_PX
	side_l.offset_bottom = ROSTER_BOTTOM_MARGIN_PX
	_ui_layer.add_child(side_l)
	_p1_roster_labels = VBoxContainer.new()
	_p1_roster_labels.set_h_size_flags(Control.SIZE_EXPAND_FILL)
	_p1_roster_labels.set_v_size_flags(Control.SIZE_EXPAND_FILL)
	_p1_roster_labels.add_theme_constant_override("separation", ROSTER_CARD_GAP_PX)
	side_l.add_child(_p1_roster_labels)

	var side_r := VBoxContainer.new()
	side_r.name = "P2RosterHUD"
	side_r.visible = false
	side_r.anchor_left = 1.0
	side_r.anchor_top = 0.0
	side_r.anchor_right = 1.0
	side_r.anchor_bottom = 1.0
	side_r.offset_left = -ROSTER_EDGE_MARGIN_PX - ROSTER_COLUMN_WIDTH_PX
	side_r.offset_top = ROSTER_TOP_BELOW_HUD_PX
	side_r.offset_right = -ROSTER_EDGE_MARGIN_PX
	side_r.offset_bottom = ROSTER_BOTTOM_MARGIN_PX
	_ui_layer.add_child(side_r)
	_p2_roster_labels = VBoxContainer.new()
	# Top-align; cards flush right in the strip; vertical stack fills height.
	side_r.alignment = BoxContainer.ALIGNMENT_BEGIN
	_p2_roster_labels.set_h_size_flags(Control.SIZE_EXPAND_FILL)
	_p2_roster_labels.set_v_size_flags(Control.SIZE_EXPAND_FILL)
	_p2_roster_labels.add_theme_constant_override("separation", ROSTER_CARD_GAP_PX)
	side_r.add_child(_p2_roster_labels)


func _setup_resize_debounce_timer() -> void:
	_resize_debounce_timer = Timer.new()
	_resize_debounce_timer.wait_time = 0.1  # 100ms debounce
	_resize_debounce_timer.one_shot = true
	_resize_debounce_timer.timeout.connect(_on_resize_debounce_timeout)
	add_child(_resize_debounce_timer)

	# Create overtime border (red screen edge)
	var border_panel := Panel.new()
	border_panel.name = "OvertimeBorderPanel"
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.0, 0.0, 0.0, 0.0)  # Transparent background
	style.border_width_left = 8
	style.border_width_top = 8
	style.border_width_right = 8
	style.border_width_bottom = 8
	style.border_color = UiTokensScript.COLOR_OVERTIME_BORDER
	border_panel.add_theme_stylebox_override("panel", style)
	border_panel.set_anchors_preset(Control.PRESET_FULL_RECT)
	border_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	border_panel.z_index = UiTokensScript.Z_OVERTIME_BORDER
	border_panel.visible = false
	_ui_layer.add_child(border_panel)
	_overtime_border = border_panel


func _build_match_over_overlay() -> void:
	_match_overlay = ColorRect.new()
	_match_overlay.name = "MatchOver"
	_match_overlay.color = UiTokensScript.COLOR_MATCH_OVERLAY_BG
	_match_overlay.visible = false
	_match_overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
	_match_overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	_match_overlay.z_index = UiTokensScript.Z_MATCH_OVERLAY
	_ui_layer.add_child(_match_overlay)

	var vb := VBoxContainer.new()
	vb.set_anchors_preset(Control.PRESET_CENTER)
	vb.offset_left = -400.0
	vb.offset_right = 400.0
	vb.offset_top = -280.0
	vb.offset_bottom = 280.0
	_match_overlay.add_child(vb)

	_match_title = Label.new()
	_match_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_match_title.add_theme_font_size_override("font_size", 28)
	vb.add_child(_match_title)

	_match_stats_container = VBoxContainer.new()
	vb.add_child(_match_stats_container)

	var new_draft := Button.new()
	new_draft.text = "NEW DRAFT"
	new_draft.custom_minimum_size = Vector2(220, 44)
	new_draft.pressed.connect(_on_new_draft_from_match)
	vb.add_child(new_draft)


func _build_preparation_controls() -> void:
	_commence_button = Button.new()
	_commence_button.name = "CommenceBattle"
	_commence_button.text = "COMMENCE BATTLE"
	_commence_button.visible = false
	_commence_button.custom_minimum_size = Vector2(COMMENCE_BATTLE_MIN_WIDTH_PX, 44.0)
	_commence_button.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	var half_w: float = 0.5 * COMMENCE_BATTLE_MIN_WIDTH_PX
	_commence_button.offset_left = -half_w
	_commence_button.offset_top = -68.0
	_commence_button.offset_right = half_w
	_commence_button.offset_bottom = -24.0
	_commence_button.pressed.connect(_on_commence_battle)
	_ui_layer.add_child(_commence_button)


func _on_window_resized() -> void:
	# Debounce resize events to avoid stuttering during window resizing
	if _resize_debounce_timer != null:
		_resize_debounce_timer.stop()
		_resize_debounce_timer.start()


func _on_resize_debounce_timeout() -> void:
	var screen_size := get_viewport_rect().size
	if _ui_layer != null and is_instance_valid(_ui_layer):
		_ui_layer.size = screen_size
		_update_battle_layer_layout()
	_update_draft_layout(screen_size)
	
	# Update champion buttons in place instead of recreating them
	if _game_state == GameState.DRAFTING:
		_update_champion_buttons_in_place(screen_size)
	
	# Force redraw to clear visual artifacts
	if _header_panel != null:
		_header_panel.queue_redraw()
	if _ui_layer != null:
		_ui_layer.queue_redraw()
	if _game_state == GameState.COMBAT or _game_state == GameState.PREPARATION or _game_state == GameState.MATCH_OVER:
		call_deferred("_roster_reflow_squares")
	if (_game_state == GameState.COMBAT or _game_state == GameState.PREPARATION) and _backend != null:
		# Do not re-apply tick_fx (floaters / melee VFX) — same snapshot, layout-only.
		_update_visualization_from_snapshot(false)


func _update_champion_buttons_in_place(screen_size: Vector2) -> void:
	_update_draft_layout(screen_size)
	_populate_champion_grid()


func _update_draft_layout(screen_size: Vector2) -> void:
	if _draft_shell != null:
		_draft_shell.apply_layout(screen_size)
	if _champion_grid != null:
		_champion_grid.columns = DraftLayoutScript.calculate_grid_columns(screen_size.x, UiTokensScript.DRAFT_CHAMPION_TILE_PX)


func _init_champion_catalog_tooltip() -> void:
	if _ui_layer == null or _champ_catalog_tt != null:
		return
	var path: String = "res://scripts/app/champion_catalog_tooltip.gd"
	if not ResourceLoader.exists(path):
		push_error("Champion catalog tooltip: missing " + path)
		return
	var res: Resource = load(path)
	if res == null or not (res is Script):
		push_error("Champion catalog tooltip: load failed (parse error or missing) " + path)
		return
	_champ_catalog_tt = (res as Script).new() as Node
	if _champ_catalog_tt == null:
		return
	_champ_catalog_tt.name = "ChampionCatalogTooltip"
	_ui_layer.add_child(_champ_catalog_tt)
	_champ_catalog_tt.call("setup", _ui_layer)


func _register_champ_tooltip(ctl: Control, champion_id: StringName, unit_data: Dictionary = {}) -> void:
	if _champ_catalog_tt == null or not is_instance_valid(ctl):
		return
	_champ_catalog_tt.call("register_source", ctl, champion_id, unit_data)


func _register_roster_card_tooltip(card: Node, ud: Dictionary) -> void:
	var catch: Node = null
	if card is Node:
		catch = (card as Node).find_child("RosterChampionTooltipCatcher", true, false)
	if catch is Control:
		var hid: StringName = StringName(str(ud.get("unit_id", "")))
		_register_champ_tooltip(catch as Control, hid, ud)


func _remove_champion_buttons_from_header() -> void:
	_clear_champion_buttons()


func _clear_champion_buttons() -> void:
	if _champion_grid != null:
		for child in _champion_grid.get_children():
			if child is Button and (child as Button).name.begins_with("Champion"):
				_champion_grid.remove_child(child)
				child.free()
	var to_clear: Array[Node] = []
	for child in _header_panel.get_children():
		if child is Button and (child as Button).name.begins_with("Champion"):
			to_clear.append(child)
	for n: Node in to_clear:
		if is_instance_valid(n) and n.get_parent() == _header_panel:
			_header_panel.remove_child(n)
			n.free()


func _update_action_buttons(screen_size: Vector2) -> void:
	_update_draft_layout(screen_size)


func _update_role_filter_buttons(screen_size: Vector2) -> void:
	_update_draft_layout(screen_size)


func _apply_color_scheme() -> void:
	if _turn_label != null:
		_turn_label.add_theme_color_override("font_color", UiTokensScript.COLOR_WARNING)
	if _player_team_label != null:
		_player_team_label.add_theme_color_override("font_color", UiTokensScript.COLOR_PLAYER)
	if _enemy_team_label != null:
		_enemy_team_label.add_theme_color_override("font_color", UiTokensScript.COLOR_ENEMY)


func _process(delta: float) -> void:
	if _game_state == GameState.COMBAT and not _combat_paused:
		_step_simulation(delta)
	if _arena_layer != null and (_game_state == GameState.COMBAT or _game_state == GameState.PREPARATION):
		_arena_layer.refresh()


## Advances simulation time; may run multiple ticks per frame (capped). Tick FX runs only when exactly one tick ran—burst catch-up skips intermediate FX.
func _step_simulation(delta: float) -> void:
	if _backend == null:
		return
	
	_sim_time_accumulator += delta * _sim_speed
	var tick_rate: float = SimConstantsScript.DEFAULT_TICK_RATE
	var ticks_budget: int = SimConstantsScript.SIMULATION_VIEWER_MAX_TICKS_PER_FRAME
	var ticks_advanced: int = 0
	
	while ticks_budget > 0 and _sim_time_accumulator >= tick_rate:
		ticks_budget -= 1
		_backend.advance_one_tick()
		_sim_time_accumulator -= tick_rate
		ticks_advanced += 1
		
		if _backend.match_ticks_exhausted():
			_end_match()
			break
	
	if ticks_advanced > 0:
		_update_visualization_from_snapshot(ticks_advanced == 1)
		_update_active_tooltip_from_snapshot()


func _update_active_tooltip_from_snapshot() -> void:
	if _champ_catalog_tt == null or not is_instance_valid(_champ_catalog_tt):
		return
	var snapshot: Dictionary = _backend.get_tick_snapshot()
	if snapshot.is_empty():
		return
	var units: Array = snapshot.get("units")
	if units.is_empty():
		return
	
	# Get the active champion from the tooltip
	var active_champion: StringName = _champ_catalog_tt.call("get_active_champion")
	if active_champion.is_empty():
		return
	
	# Find the unit data for the active champion
	for unit_data in units:
		if unit_data is not Dictionary:
			continue
		var unit_dict: Dictionary = unit_data as Dictionary
		if String(unit_dict.get("unit_id", "")) == String(active_champion):
			_champ_catalog_tt.call("update_with_unit_data", unit_dict)
			break


func _update_visualization_from_snapshot(apply_tick_fx: bool = true) -> void:
	var snapshot: Dictionary = _backend.get_tick_snapshot()
	if snapshot.is_empty():
		return
	for field in ["time", "match_duration", "time_remaining", "player_kills", "enemy_kills", "live_winner", "units", "projectiles", "tick_fx"]:
		if not snapshot.has(field):
			_viewer_contract_mismatch("snapshot", field, "present", snapshot)
			return
	if not snapshot.get("units") is Array:
		_viewer_contract_mismatch("snapshot", "units", "Array", snapshot.get("units"))
		return
	if not snapshot.get("projectiles") is Array:
		_viewer_contract_mismatch("snapshot", "projectiles", "Array", snapshot.get("projectiles"))
		return
	if not snapshot.get("tick_fx") is Array:
		_viewer_contract_mismatch("snapshot", "tick_fx", "Array", snapshot.get("tick_fx"))
		return
	_refresh_combat_hud(snapshot)
	var units: Array = snapshot.get("units")
	for unit_data in units:
		if unit_data is not Dictionary:
			continue
		var unit_dict: Dictionary = unit_data as Dictionary
		_update_unit_node(unit_dict)
	_sync_target_lines_from_snapshot(units)
	# Selection rings
	for uid in _unit_nodes.keys():
		var n: Node2D = _unit_nodes[uid] as Node2D
		if n != null and n.has_method("set_selected"):
			n.set_selected(int(uid) == _selected_unit_id)
	var projectiles: Array = snapshot.get("projectiles")
	_update_projectiles(projectiles)
	if apply_tick_fx:
		_apply_tick_fx(snapshot)
	_refresh_side_rosters_from_snapshot(units)


func _update_unit_node(unit_dict: Dictionary) -> void:
	var instance_id: int = int(unit_dict.get("instance_id", 0))
	if instance_id == 0:
		return
	var team: StringName = StringName(unit_dict.get("team", ""))
	var unit_node: Node2D = _unit_nodes.get(instance_id)
	if unit_node == null:
		unit_node = _create_unit_node(instance_id, team)
		_unit_nodes[instance_id] = unit_node
		_world_layer.add_child(unit_node)
	var px: float = float(unit_dict.get("pos_x", 0.0))
	var py: float = float(unit_dict.get("pos_y", 0.0))
	unit_node.position = world_to_battle_local(px, py)
	if unit_node.has_method("set_snapshot_data"):
		unit_node.set_snapshot_data(unit_dict)
	unit_node.visible = _is_unit_on_battlefield(unit_dict)


func _is_unit_on_battlefield(u: Dictionary) -> bool:
	if _game_state != GameState.COMBAT and _game_state != GameState.PREPARATION:
		return true
	if str(u.get("state", "")) == SimConstants.UNIT_STATE_DEAD:
		return false
	if not bool(u.get("alive", true)):
		return false
	return true


func _sync_target_lines_from_snapshot(units: Array) -> void:
	var by_id: Dictionary = {}
	for unit_data in units:
		if unit_data is not Dictionary:
			continue
		var ud: Dictionary = unit_data as Dictionary
		var iid: int = int(ud.get("instance_id", 0))
		if iid != 0:
			by_id[iid] = ud
	for unit_id in _unit_nodes:
		var unit_node: Node2D = _unit_nodes[unit_id] as Node2D
		if unit_node == null or not is_instance_valid(unit_node):
			continue
		var line: Line2D = unit_node.get_node_or_null("TargetLine") as Line2D
		if line == null:
			continue
		if not unit_node.visible:
			line.visible = false
			continue
		var ud2: Dictionary = by_id.get(int(unit_id), {})
		var tid: int = int(ud2.get("target_id", 0))
		if tid != 0 and _game_state == GameState.COMBAT:
			var tnode: Node2D = _unit_nodes.get(tid) as Node2D
			if tnode != null and is_instance_valid(tnode) and tnode.visible:
				line.points = PackedVector2Array([Vector2.ZERO, tnode.position - unit_node.position])
				line.visible = true
			else:
				line.visible = false
		else:
			line.visible = false


func _create_unit_node(instance_id: int, team: StringName) -> Node2D:
	var unit_node: Node2D = ViewerUnitNodeScript.new()
	unit_node.name = "Unit_%d" % instance_id
	unit_node.setup(instance_id, team)
	unit_node.unit_clicked.connect(_on_viewer_unit_clicked)
	var target_line := Line2D.new()
	target_line.name = "TargetLine"
	target_line.width = 2.0
	target_line.default_color = UiTokensScript.COLOR_TARGET_LINE
	target_line.visible = false
	unit_node.add_child(target_line)
	return unit_node


func _on_viewer_unit_clicked(p_instance_id: int, p_team: StringName) -> void:
	_selected_unit_id = p_instance_id
	_update_inspection_panel(p_instance_id)


func _update_projectiles(projectiles: Array) -> void:
	# Clear existing projectiles
	for projectile_id in _projectile_nodes:
		var node: Node2D = _projectile_nodes[projectile_id]
		if is_instance_valid(node):
			node.queue_free()
	_projectile_nodes.clear()
	
	# Clear cached offsets and slot usage for projectiles that no longer exist
	var active_proj_ids := {}
	var active_projectile_ids := {}
	for proj_data in projectiles:
		if proj_data is Dictionary:
			var proj_dict: Dictionary = proj_data as Dictionary
			var proj_id: int = int(proj_dict.get("id", 0))
			var projectile_id: int = int(proj_dict.get("projectile_id", 0))
			active_proj_ids[proj_id] = true
			active_projectile_ids[projectile_id] = true
	
	for cached_pid in _projectile_offsets.keys():
		if not active_projectile_ids.has(cached_pid):
			_projectile_offsets.erase(cached_pid)
			if _projectile_slot_owners.has(cached_pid):
				var owner: Dictionary = _projectile_slot_owners[cached_pid]
				var pool_key: String = owner["pool"]
				var slot_index: int = int(owner["slot"])
				if _projectile_slot_usage.has(pool_key):
					var used_slots: Array = _projectile_slot_usage[pool_key]
					used_slots.erase(slot_index)
					if used_slots.is_empty():
						_projectile_slot_usage.erase(pool_key)
				_projectile_slot_owners.erase(cached_pid)

	# Create new projectiles
	for proj_data in projectiles:
		if proj_data is not Dictionary:
			continue
		var proj_dict: Dictionary = proj_data as Dictionary
		var proj_id: int = int(proj_dict.get("id", 0))
		var projectile_id: int = int(proj_dict.get("projectile_id", 0))
		var pos_x: float = float(proj_dict.get("pos_x", 0.0))
		var pos_y: float = float(proj_dict.get("pos_y", 0.0))
		var team_s: StringName = StringName(proj_dict.get("team", "player"))
		var reason: StringName = StringName(proj_dict.get("reason", ""))
		var action_kind: StringName = StringName(proj_dict.get("action_kind", ""))
		var source_id: int = int(proj_dict.get("source_id", 0))
		var proj_node := _create_projectile_node(proj_id, projectile_id, pos_x, pos_y, team_s, reason, action_kind, source_id)
		_projectile_nodes[proj_id] = proj_node
		_world_layer.add_child(proj_node)


func _projectile_screen_radius_px() -> float:
	var vp: Vector2 = get_viewport_rect().size
	return clampf(
		PROJECTILE_BASE_RADIUS_PX * minf(vp.x, vp.y) / VIEWER_BASE_MIN_AXIS,
		2.0,
		7.0
	)


func _create_projectile_node(proj_id: int, projectile_id: int, pos_x: float, pos_y: float, team: StringName, reason: StringName, action_kind: StringName, source_id: int) -> Node2D:
	var proj_node := Node2D.new()
	proj_node.name = "Projectile_%d" % proj_id
	
	# Apply spread pattern offset based on stable projectile_id to prevent visual stacking
	var spread_offset := Vector2.ZERO
	
	# Use cached random offset if available, otherwise generate and cache
	if not _projectile_offsets.has(projectile_id):
		# Group projectiles by source + reason to prevent overlap from same caster
		var pool_key := "%d_%s" % [source_id, reason]
		if not _projectile_slot_usage.has(pool_key):
			_projectile_slot_usage[pool_key] = []
		
		var used_slots: Array = _projectile_slot_usage[pool_key]
		
		# First projectile in this pool - place at center
		if used_slots.is_empty():
			spread_offset = Vector2.ZERO
			used_slots.append(-1)  # Mark center as used
			_projectile_slot_owners[projectile_id] = {"slot": -1, "pool": pool_key}
		else:
			# Subsequent projectiles - use spread pattern
			var available_slots := []
			for i in range(SimConstantsScript.VIEWER_PROJECTILE_SPREAD_NUM_SLOTS):
				if not used_slots.has(i):
					available_slots.append(i)
			
			# If all slots used, pick random anyway (fallback)
			var spread_index: int
			if available_slots.is_empty():
				spread_index = randi() % SimConstantsScript.VIEWER_PROJECTILE_SPREAD_NUM_SLOTS
			else:
				spread_index = available_slots[randi() % available_slots.size()]
				used_slots.append(spread_index)
			
			var spread_radius := SimConstantsScript.VIEWER_PROJECTILE_SPREAD_BASE_RADIUS + randf() * SimConstantsScript.VIEWER_PROJECTILE_SPREAD_RADIUS_VARIATION
			var angle_variation := (randf() - 0.5) * SimConstantsScript.VIEWER_PROJECTILE_SPREAD_ANGLE_VARIATION
			var spread_angle := TAU * float(spread_index) / float(SimConstantsScript.VIEWER_PROJECTILE_SPREAD_NUM_SLOTS) + angle_variation
			spread_offset = Vector2(cos(spread_angle), sin(spread_angle)) * spread_radius
			_projectile_slot_owners[projectile_id] = {"slot": spread_index, "pool": pool_key}
		
		_projectile_offsets[projectile_id] = spread_offset
	else:
		spread_offset = _projectile_offsets[projectile_id]
	
	proj_node.position = world_to_battle_local(pos_x, pos_y) + spread_offset
	proj_node.set_meta("reason", reason)
	proj_node.set_meta("action_kind", action_kind)
	var col: Color = UiTokensScript.COLOR_PLAYER if team == &"player" else UiTokensScript.COLOR_ENEMY
	var r: float = _projectile_screen_radius_px()
	var disc := Polygon2D.new()
	disc.name = "Disc"
	var ring := PackedVector2Array()
	var n: int = 12
	for i in range(n):
		var a: float = TAU * float(i) / float(n)
		ring.append(Vector2(cos(a), sin(a)) * r)
	disc.polygon = ring
	disc.color = col
	proj_node.add_child(disc)
	var core := Polygon2D.new()
	var r2: float = maxf(1.0, r * 0.35)
	var inner := PackedVector2Array()
	for j in range(n):
		var a2: float = TAU * float(j) / float(n)
		inner.append(Vector2(cos(a2), sin(a2)) * r2)
	core.polygon = inner
	core.color = UiTokensScript.COLOR_PROJECTILE_CORE
	proj_node.add_child(core)
	return proj_node


func _refresh_combat_hud(snapshot: Dictionary) -> void:
	if _lbl_timer != null:
		if not snapshot.has("time_remaining"):
			_viewer_contract_mismatch("snapshot", "time_remaining", "present", snapshot)
			return
		if not snapshot.has("time"):
			_viewer_contract_mismatch("snapshot", "time", "present", snapshot)
			return
		var time_remaining: float = float(snapshot.get("time_remaining"))
		var time: float = float(snapshot.get("time"))
		if time_remaining <= 0.0:
			var overtime: float = time - 60.0
			_lbl_timer.text = "OVERTIME: %.1fs" % overtime
			_lbl_timer.add_theme_color_override("font_color", UiTokensScript.COLOR_OVERTIME_TEXT)
			if _overtime_border != null:
				_overtime_border.visible = true
		else:
			_lbl_timer.text = "TIME: %.1fs" % time_remaining
			_lbl_timer.add_theme_color_override("font_color", UiTokensScript.COLOR_TEXT)
			if _overtime_border != null:
				_overtime_border.visible = false
	if _lbl_score != null:
		if not snapshot.has("player_kills"):
			_viewer_contract_mismatch("snapshot", "player_kills", "present", snapshot)
			return
		if not snapshot.has("enemy_kills"):
			_viewer_contract_mismatch("snapshot", "enemy_kills", "present", snapshot)
			return
		_lbl_score.text = "SCORE: %d - %d" % [int(snapshot.get("player_kills")), int(snapshot.get("enemy_kills"))]
	if _lbl_combat_state != null and _game_state != GameState.PREPARATION:
		if not snapshot.has("live_winner"):
			_viewer_contract_mismatch("snapshot", "live_winner", "present", snapshot)
			return
		_lbl_combat_state.text = GameState.keys()[_game_state]
	if _hud_pause != null:
		_hud_pause.visible = _combat_paused and _game_state == GameState.COMBAT
		if _hud_pause.visible:
			_hud_pause.text = "PAUSED (Space)  |  K: speed"


func _refresh_side_rosters_from_snapshot(units: Array) -> void:
	if _p1_roster_labels == null or _p2_roster_labels == null:
		return
	var p1: Array = []
	var p2: Array = []
	for u in units:
		if u is Dictionary:
			var ud2: Dictionary = u
			# Skip minions - side bars are for champions only
			var role: String = str(ud2.get("role", ""))
			if role.to_lower() == "minion":
				continue
			if String(ud2.get("team", "")) == "player":
				p1.append(ud2)
			elif String(ud2.get("team", "")) == "enemy":
				p2.append(ud2)
	p1.sort_custom(func(a, b): return int(a.get("instance_id", 0)) < int(b.get("instance_id", 0)))
	p2.sort_custom(func(a, b): return int(a.get("instance_id", 0)) < int(b.get("instance_id", 0)))
	var start_s: int = int(mini(ROSTER_COLUMN_WIDTH_PX, float(ROSTER_TILE_START_PX)))
	var in_p1: bool = _try_update_roster_column_in_place(_p1_roster_labels, p1)
	if not in_p1:
		for c in _p1_roster_labels.get_children():
			c.queue_free()
		for ud3 in p1:
			var card1: Node = RosterChampionCardScript.new() as Node
			if card1 == null or not card1.has_method("setup"):
				continue
			card1.call("setup", ud3, false, true, start_s)
			_p1_roster_labels.add_child(card1)
			_register_roster_card_tooltip(card1, ud3)
	var in_p2: bool = _try_update_roster_column_in_place(_p2_roster_labels, p2)
	if not in_p2:
		for c2 in _p2_roster_labels.get_children():
			c2.queue_free()
		for ud4 in p2:
			var card2: Node = RosterChampionCardScript.new() as Node
			if card2 == null or not card2.has_method("setup"):
				continue
			card2.call("setup", ud4, true, false, start_s)
			_p2_roster_labels.add_child(card2)
			_register_roster_card_tooltip(card2, ud4)
	call_deferred("_roster_reflow_squares")


func _is_roster_champion_card(n: Node) -> bool:
	return n != null and n.get_script() == RosterChampionCardScript


func _try_update_roster_column_in_place(vb: VBoxContainer, team_units: Array) -> bool:
	var n: int = team_units.size()
	if vb.get_child_count() != n or n < 1:
		return false
	for i in n:
		var ch: Node = vb.get_child(i)
		if not _is_roster_champion_card(ch) or not ch.has_method("apply_unit_data"):
			return false
		var ud0: Dictionary = team_units[i] as Dictionary
		if int(ch.get("instance_id")) != int(ud0.get("instance_id", 0)):
			return false
	for j in n:
		var c2: Node = vb.get_child(j)
		if c2.has_method("apply_unit_data"):
			c2.call("apply_unit_data", team_units[j] as Dictionary, 0, false)
		_register_roster_card_tooltip(c2, team_units[j] as Dictionary)
	return true


func _roster_reflow_squares() -> void:
	if _p1_roster_labels == null or _p2_roster_labels == null:
		return
	_reflow_one_roster_column(_p1_roster_labels)
	_reflow_one_roster_column(_p2_roster_labels)


func _reflow_one_roster_column(vb: VBoxContainer) -> void:
	var p: Control = vb.get_parent() as Control
	if p == null:
		return
	var n: int = vb.get_child_count()
	if n < 1:
		return
	var col_w: float = maxf(1.0, p.size.x)
	var col_h: float = maxf(1.0, p.size.y)
	if col_h < 2.0:
		col_h = maxf(
			1.0,
			get_viewport().get_visible_rect().size.y - ROSTER_TOP_BELOW_HUD_PX - ROSTER_BOTTOM_MARGIN_PX
		)
	var gap: float = float(ROSTER_CARD_GAP_PX)
	var per_row_h: float = (col_h - gap * float(n - 1)) / float(n)
	var s: int = int(floorf(minf(per_row_h, col_w)))
	if s < ROSTER_TILE_MIN_SQUARE_PX:
		s = ROSTER_TILE_MIN_SQUARE_PX
	for j in n:
		var ch2: Node = vb.get_child(j)
		if _is_roster_champion_card(ch2) and ch2.has_method("set_square_size"):
			ch2.call("set_square_size", s)


func _apply_tick_fx(snapshot: Dictionary) -> void:
	var evs: Array = snapshot.get("tick_fx", [])
	
	# Consolidate damage/heal/shield by target_id
	var consolidated: Dictionary = {}  # target_id -> {damage: float, heal: float, shield: float, damage_type: String, x: float, y: float}
	var other_events: Array = []
	
	for e in evs:
		if e is not Dictionary:
			continue
		var d: Dictionary = e
		var k: String = str(d.get("kind", ""))
		var target_id: int = int(d.get("target_id", 0))
		var wx: float = float(d.get("x", 0.0))
		var wy: float = float(d.get("y", 0.0))
		
		if k == "damage" or k == "dmg":
			if not consolidated.has(target_id):
				consolidated[target_id] = {damage = 0.0, heal = 0.0, shield = 0.0, damage_type = "physical", x = wx, y = wy}
			consolidated[target_id].damage += float(d.get("val", 0.0))
			var damage_type: String = str(d.get("damage_type", "physical"))
			# Prioritize magic/true damage types for color
			if damage_type == "magic" or damage_type == "true":
				consolidated[target_id].damage_type = damage_type
		elif k == "heal":
			if not consolidated.has(target_id):
				consolidated[target_id] = {damage = 0.0, heal = 0.0, shield = 0.0, damage_type = "physical", x = wx, y = wy}
			consolidated[target_id].heal += float(d.get("val", 0.0))
		elif k == "shield":
			if not consolidated.has(target_id):
				consolidated[target_id] = {damage = 0.0, heal = 0.0, shield = 0.0, damage_type = "physical", x = wx, y = wy}
			consolidated[target_id].shield += float(d.get("val", 0.0))
		else:
			other_events.append(d)
	
	# Spawn consolidated floating texts
	for target_id in consolidated:
		var data: Dictionary = consolidated[target_id]
		var sp: Vector2 = world_to_screen(data.x, data.y)
		
		if data.damage > 0.0:
			var color: Color
			if data.damage_type == "magic":
				color = UiTokensScript.COLOR_DAMAGE_MAGIC
			elif data.damage_type == "true":
				color = UiTokensScript.COLOR_DAMAGE_TRUE
			else:
				color = UiTokensScript.COLOR_DAMAGE_PHYSICAL
			_spawn_floating_text_screen("-%d" % int(ceil(data.damage)), sp, color)
		
		if data.heal > 0.0:
			_spawn_floating_text_screen("+%d" % int(ceil(data.heal)), sp, UiTokensScript.COLOR_SUCCESS)
		
		if data.shield > 0.0:
			_spawn_floating_text_screen("[%d]" % int(ceil(data.shield)), sp, UiTokensScript.COLOR_SHIELD_TEXT)
	
	# Process other events (hot_status, passive_aoe, melee_slash, aoe_*)
	for d in other_events:
		var k: String = str(d.get("kind", ""))
		var wx: float = float(d.get("x", 0.0))
		var wy: float = float(d.get("y", 0.0))
		var sp: Vector2 = world_to_screen(wx, wy)
		var sp_battle: Vector2 = world_to_battle_local(wx, wy)
		
		if k == "hot_status":
			var target_id: int = int(d.get("target_id", 0))
			var duration: float = float(d.get("val", 0.0))
			_spawn_hot_status_ring(target_id, duration, snapshot)
		elif k == "unit_death":
			var target_id: int = int(d.get("target_id", 0))
			_cleanup_unit_fx(target_id)
		elif k == "passive_aoe":
			var target_id: int = int(d.get("target_id", 0))
			var radius: float = float(d.get("radius", 0.0))
			var kind: String = str(d.get("extra", ""))
			_update_passive_aoe_ring(target_id, radius, kind, snapshot)
		elif k == "melee_slash":
			_spawn_melee_slash_fx(sp_battle)
		elif k.begins_with("aoe_"):
			var extra_v: Variant = d.get("extra", null)
			if extra_v is Dictionary:
				_spawn_aoe_shape_fx(d, snapshot)
			else:
				var r_w: float = float(d.get("r", d.get("radius", 0.0)))
				if r_w <= 0.0:
					_viewer_contract_mismatch(k, "r", "positive world radius or explicit extra.shape metadata", d)
					continue
				_spawn_aoe_ring_fx(wx, wy, r_w, k, int(d.get("src_id", 0)), snapshot)


func _viewer_contract_mismatch(kind: String, field: String, expected: String, payload: Variant) -> void:
	push_error(
		"Viewer contract mismatch [kind=%s field=%s expected=%s] payload=%s"
		% [kind, field, expected, JSON.stringify(payload, "\t")]
	)


func _snapshot_unit_by_id(snapshot: Dictionary, instance_id: int, scope: String) -> Dictionary:
	if instance_id == 0:
		_viewer_contract_mismatch(scope, "instance_id", "non-zero", snapshot)
		return {}
	var units: Array = snapshot.get("units", [])
	for u in units:
		if u is Dictionary and int((u as Dictionary).get("instance_id", 0)) == instance_id:
			return u as Dictionary
	_viewer_contract_mismatch(scope, "instance_id", "present in snapshot.units", snapshot)
	return {}


func _spawn_aoe_shape_fx(ev: Dictionary, snapshot: Dictionary) -> void:
	var kind: String = str(ev.get("kind", ""))
	var src_id: int = int(ev.get("src_id", 0))
	var source_u: Dictionary = _snapshot_unit_by_id(snapshot, src_id, "%s.src_id" % kind)
	if source_u.is_empty():
		return
	var extra_v: Variant = ev.get("extra", null)
	if not extra_v is Dictionary:
		_viewer_contract_mismatch(kind, "extra", "Dictionary", ev)
		return
	var extra: Dictionary = extra_v
	if extra.is_empty():
		_viewer_contract_mismatch(kind, "extra", "non-empty shape metadata", ev)
		return
	for field in ["shape", "anchor", "radius", "width", "height", "rotation_radians", "anchor_x", "anchor_y", "target_id", "forward_x", "forward_y"]:
		if not extra.has(field):
			_viewer_contract_mismatch(kind, "extra.%s" % field, "present", extra)
			return
	var shape: String = str(extra.get("shape", ""))
	if shape.is_empty():
		_viewer_contract_mismatch(kind, "extra.shape", "circle, cone, or rectangle", extra)
		return
	var anchor: String = str(extra.get("anchor", ""))
	if anchor.is_empty():
		_viewer_contract_mismatch(kind, "extra.anchor", "source, target, point, or forward", extra)
		return
	var wx: float = float(ev.get("x", 0.0))
	var wy: float = float(ev.get("y", 0.0))
	var target_id: int = int(ev.get("target_id", 0))
	if int(extra.get("target_id", -1)) != target_id:
		_viewer_contract_mismatch(kind, "target_id", "top-level and extra.target_id must match", ev)
		return
	if anchor == "target" and target_id == 0:
		_viewer_contract_mismatch(kind, "target_id", "non-zero target id for target-anchored AoE", ev)
		return
	if anchor == "target":
		var target_u: Dictionary = _snapshot_unit_by_id(snapshot, target_id, "%s.target_id" % kind)
		if target_u.is_empty():
			return
	var source_team: String = str(source_u.get("team", ""))
	if source_team != "player" and source_team != "enemy":
		_viewer_contract_mismatch(kind, "src_id/team", "player or enemy", source_u)
		return
	var col: Color = _aoe_ring_color_for(kind, source_team)
	var direction: Vector2 = Vector2.RIGHT
	if shape == "cone" or shape == "rectangle":
		if not extra.has("forward_x") or not extra.has("forward_y"):
			_viewer_contract_mismatch(kind, "extra.forward_x/extra.forward_y", "present", extra)
			return
		direction = Vector2(float(extra.get("forward_x", 0.0)), float(extra.get("forward_y", 0.0)))
		if direction.length_squared() <= 0.000001:
			_viewer_contract_mismatch(kind, "extra.forward_x/extra.forward_y", "non-zero direction vector", extra)
			return
		direction = direction.normalized()
	var n: Node2D = null
	match shape:
		"circle":
			n = _render_circle_aoe(extra, wx, wy, col)
		"cone":
			n = _render_cone_aoe(extra, wx, wy, direction, col)
		"rectangle":
			n = _render_rectangle_aoe(extra, wx, wy, direction, col)
		_:
			_viewer_contract_mismatch(kind, "extra.shape", "circle, cone, or rectangle", extra)
			return
	if n == null:
		return
	if _aoe_fx_layer != null and is_instance_valid(_aoe_fx_layer):
		n.position = world_to_screen(wx, wy)
		_aoe_fx_layer.add_child(n)
	else:
		n.z_index = UiTokensScript.Z_AOE_FX
		n.position = world_to_battle_local(wx, wy)
		_world_layer.add_child(n)
	var dur: float = SimConstantsScript.AOE_VISUAL_MAX_DURATION
	n.modulate = Color(1, 1, 1, 0.92)
	var tw := create_tween()
	_register_tween(tw)
	tw.tween_property(n, "modulate:a", 0.0, dur)
	tw.tween_callback(n.queue_free)


func _render_circle_aoe(params: Dictionary, wx: float, wy: float, col: Color) -> Node2D:
	var radius: float = float(params.get("radius", 0.0))
	if radius <= 0.0:
		_viewer_contract_mismatch("aoe_circle", "radius", "positive world radius", params)
		return null
	var screen_radius: float = _world_to_screen_aoe_size(radius)
	var n: Node2D = AoeRingNodeScript.new()
	var fill_c := Color(col.r, col.g, col.b, 0.22 * col.a)
	n.setup(screen_radius, screen_radius, fill_c, col, 3.0)
	return n


# Helper function to convert world units to screen pixels for AOE shapes
func _world_to_screen_aoe_size(world_size: float) -> float:
	var vp: Vector2 = get_viewport_rect().size
	var s: float = SimConstantsScript.viewer_battle_square_side(vp)
	var screen_size: float = world_size * (s / SimConstantsScript.WORLD_SIZE)
	return maxf(screen_size, SimConstantsScript.VIEWER_AOE_MIN_RING_RADIUS_PX)


func _render_cone_aoe(params: Dictionary, wx: float, wy: float, direction: Vector2, col: Color) -> Node2D:
	var radius: float = float(params.get("radius", 0.0))
	var width_angle: float = float(params.get("width", 0.0))
	if radius <= 0.0:
		_viewer_contract_mismatch("aoe_cone", "radius", "positive world radius", params)
		return null
	if width_angle <= 0.0 or width_angle > 360.0:
		_viewer_contract_mismatch("aoe_cone", "width", "degrees in the range (0, 360]", params)
		return null
	var screen_radius: float = _world_to_screen_aoe_size(radius)
	var n := Node2D.new()
	var cone := Polygon2D.new()
	var points: PackedVector2Array = []
	var segments: int = 32
	var half_angle: float = deg_to_rad(width_angle) * 0.5
	points.append(Vector2(0, 0))
	for i in range(segments + 1):
		var angle: float = (float(i) / float(segments)) * deg_to_rad(width_angle) - half_angle
		var dir: Vector2 = direction.rotated(angle)
		points.append(dir * screen_radius)
	
	cone.polygon = points
	cone.color = col
	n.add_child(cone)
	
	return n


func _render_rectangle_aoe(params: Dictionary, wx: float, wy: float, direction: Vector2, col: Color) -> Node2D:
	var width: float = float(params.get("width", 0.0))
	var height: float = float(params.get("height", 0.0))
	if width <= 0.0 or height <= 0.0:
		_viewer_contract_mismatch("aoe_rectangle", "width/height", "positive world dimensions", params)
		return null
	var screen_width: float = _world_to_screen_aoe_size(width)
	var screen_height: float = _world_to_screen_aoe_size(height)
	var n := Node2D.new()
	var rect := Polygon2D.new()
	var points: PackedVector2Array = []
	var half_w: float = screen_width * 0.5
	var half_h: float = screen_height * 0.5
	var right: Vector2 = Vector2(-direction.y, direction.x)
	var forward: Vector2 = direction
	
	points.append(forward * half_h + right * half_w)
	points.append(forward * half_h - right * half_w)
	points.append(-forward * half_h - right * half_w)
	points.append(-forward * half_h + right * half_w)
	
	rect.polygon = points
	rect.color = col
	n.add_child(rect)
	
	return n




func _aoe_ring_color_for(kind: String, team_s: String) -> Color:
	if kind == "aoe_taunt":
		return UiTokensScript.COLOR_AOE_TAUNT
	if kind == "aoe_splash":
		return UiTokensScript.COLOR_AOE_SPLASH
	if kind == "aoe_hot":
		return UiTokensScript.COLOR_AOE_HOT
	if kind == "passive_aoe":
		# Team-colored with 50% opacity for both fill and border
		if team_s == "player":
			return UiTokensScript.COLOR_PLAYER
		if team_s == "enemy":
			return UiTokensScript.COLOR_ENEMY
	# aoe_damage: tint by team
	if team_s == "player":
		return UiTokensScript.COLOR_AOE_PLAYER
	if team_s == "enemy":
		return UiTokensScript.COLOR_AOE_ENEMY
	return UiTokensScript.COLOR_AOE_NEUTRAL


## Creates a green progress ring around a unit that depletes with HoT duration
func _spawn_hot_status_ring(target_id: int, duration: float, snapshot: Dictionary) -> void:
	if target_id == 0 or duration <= 0.0:
		return
	
	# Get unit node to parent the ring to it
	var unit_node: Node2D = _unit_nodes.get(target_id) as Node2D
	if unit_node == null or not is_instance_valid(unit_node):
		return
	
	# Remove existing ring for this unit if present
	var old_ring: Node2D = _hot_status_rings.get(target_id) as Node2D
	if old_ring != null and is_instance_valid(old_ring):
		old_ring.queue_free()
		_hot_status_rings.erase(target_id)
	
	# Create progress ring around unit (in local space, so it follows unit movement)
	var ring_radius: float = 24.0  # Fixed screen-space radius
	var col: Color = UiTokensScript.COLOR_HOT_RING
	var n: Node2D = AoeRingNodeScript.new()
	n.name = "HotStatusRing"
	n.position = Vector2.ZERO  # Local space - will follow unit automatically
	var fill_c := Color(col.r, col.g, col.b, 0.15 * col.a)
	n.setup(ring_radius, ring_radius, fill_c, col, 2.5)
	
	# Parent to unit node so it follows movement
	unit_node.add_child(n)
	
	# Track this ring for the unit
	_hot_status_rings[target_id] = n
	
	# Tween to fade out over duration
	n.modulate = Color(1, 1, 1, 1.0)
	var tw := create_tween()
	_register_tween(tw)
	tw.tween_property(n, "modulate:a", 0.0, duration)
	tw.tween_callback(_cleanup_hot_status_ring.bind(target_id))

## Cleanup callback for HoT status ring
func _cleanup_hot_status_ring(target_id: int) -> void:
	var ring: Node2D = _hot_status_rings.get(target_id) as Node2D
	if ring != null and is_instance_valid(ring):
		ring.queue_free()
		_hot_status_rings.erase(target_id)

## Centralized cleanup for all unit-specific visual effects
func _cleanup_unit_fx(target_id: int) -> void:
	if target_id == 0:
		return

	# Cleanup HoT status ring
	_cleanup_hot_status_ring(target_id)

	# Cleanup passive AOE rings
	var rings: Array = _passive_aoe_rings.get(target_id, [])
	for ring in rings:
		if ring != null and is_instance_valid(ring):
			ring.queue_free()
	_passive_aoe_rings.erase(target_id)

## Creates or updates a persistent AOE ring for passive effects
func _update_passive_aoe_ring(target_id: int, radius: float, kind: String, snapshot: Dictionary) -> void:
	if target_id == 0:
		return
	
	# Get unit node to parent the ring to it
	var unit_node: Node2D = _unit_nodes.get(target_id) as Node2D
	if unit_node == null or not is_instance_valid(unit_node):
		return
	
	# If radius is 0, cleanup all FX for this unit (death signal)
	if radius <= 0.0:
		_cleanup_unit_fx(target_id)
		return
	
	# Get unit team for color
	var unit_u: Dictionary = _snapshot_unit_by_id(snapshot, target_id, "passive_aoe")
	if unit_u.is_empty():
		return
	var team_s: String = str(unit_u.get("team", ""))
	
	# Calculate screen-space radius
	var vp: Vector2 = get_viewport_rect().size
	var s: float = SimConstantsScript.viewer_battle_square_side(vp)
	var rpx: float = radius * (s / SimConstantsScript.WORLD_SIZE)
	rpx = maxf(rpx, SimConstantsScript.VIEWER_AOE_MIN_RING_RADIUS_PX)
	
	# Get color
	var col: Color = _aoe_ring_color_for("passive_aoe", team_s)
	
	# Create ring around unit (in local space, so it follows unit movement)
	var n: Node2D = AoeRingNodeScript.new()
	n.name = "PassiveAoeRing"
	n.position = Vector2.ZERO  # Local space - will follow unit automatically
	var fill_c := Color(col.r, col.g, col.b, 0.0)  # Transparent fill
	n.setup(rpx, rpx, fill_c, col, 2.5)
	
	# Parent to unit node so it follows movement
	unit_node.add_child(n)
	
	# Track this ring for the unit (support multiple rings per unit)
	if not _passive_aoe_rings.has(target_id):
		_passive_aoe_rings[target_id] = []
	var rings: Array = _passive_aoe_rings[target_id]
	rings.append(n)

## World-space [wx,wy], radius in world units; center matches sim AoE center.
func _spawn_aoe_ring_fx(wx: float, wy: float, world_r: float, kind: String, src_id: int, snapshot: Dictionary) -> void:
	if world_r <= 0.0:
		return
	var source_u: Dictionary = _snapshot_unit_by_id(snapshot, src_id, "%s.src_id" % kind)
	if source_u.is_empty():
		return
	var source_team: String = str(source_u.get("team", ""))
	if source_team != "player" and source_team != "enemy":
		_viewer_contract_mismatch(kind, "src_id/team", "player or enemy", source_u)
		return
	var vp: Vector2 = get_viewport_rect().size
	var s: float = SimConstantsScript.viewer_battle_square_side(vp)
	var sp: Vector2 = world_to_screen(wx, wy)
	var rpx: float = world_r * (s / SimConstantsScript.WORLD_SIZE)
	rpx = maxf(rpx, SimConstantsScript.VIEWER_AOE_MIN_RING_RADIUS_PX)
	var col: Color = _aoe_ring_color_for(kind, source_team)
	var n: Node2D = AoeRingNodeScript.new()
	n.name = "AoeRing"
	n.position = sp
	var fill_c := Color(col.r, col.g, col.b, 0.22 * col.a)
	n.setup(rpx, rpx, fill_c, col, 3.0)
	if _aoe_fx_layer != null and is_instance_valid(_aoe_fx_layer):
		_aoe_fx_layer.add_child(n)
	else:
		n.z_index = UiTokensScript.Z_AOE_FX
		n.position = world_to_battle_local(wx, wy)
		_world_layer.add_child(n)
	var dur: float = SimConstantsScript.AOE_VISUAL_MAX_DURATION
	n.modulate = Color(1, 1, 1, 0.92)
	var tw := create_tween()
	_register_tween(tw)
	tw.tween_property(n, "modulate:a", 0.0, dur)
	tw.tween_callback(n.queue_free)


func _spawn_melee_slash_fx(screen_pos: Vector2) -> void:
	var s := Node2D.new()
	s.position = screen_pos
	s.z_index = UiTokensScript.Z_MELEE_SLASH
	var line := Line2D.new()
	line.add_point(Vector2(-10, -10))
	line.add_point(Vector2(10, 10))
	line.width = 2.0
	line.default_color = UiTokensScript.COLOR_MELEE_SLASH
	s.add_child(line)
	_world_layer.add_child(s)
	var tw := create_tween()
	_register_tween(tw)
	tw.tween_property(line, "modulate:a", 0.0, 0.4)
	tw.tween_callback(s.queue_free)


func _spawn_floating_text_screen(text: String, screen_pos: Vector2, color: Color) -> void:
	var label := Label.new()
	label.text = text
	label.add_theme_color_override("font_color", color)
	label.z_index = UiTokensScript.Z_FLOATING_TEXT
	label.position = screen_pos
	_ui_layer.add_child(label)
	_floating_texts.append(label)
	var tween := create_tween()
	_register_tween(tween)
	tween.parallel().tween_property(label, "position", screen_pos + Vector2(0, -40), 0.8)
	tween.parallel().tween_property(label, "modulate:a", 0.0, 0.8)
	tween.tween_callback(label.queue_free)


func _remove_floating_text(label: Label) -> void:
	_floating_texts.erase(label)


func _setup_draft_ui() -> void:
	var screen_size := get_viewport_rect().size

	var rf: Array[Node] = []
	var role_parent: Control = _role_filter_container if _role_filter_container != null else _header_panel
	for child in role_parent.get_children():
		if child is Button and (child as Button).name.begins_with("RoleFilter"):
			rf.append(child)
	for n: Node in rf:
		if is_instance_valid(n) and n.get_parent() == role_parent:
			role_parent.remove_child(n)
			n.free()

	_update_draft_layout(screen_size)

	# Set up role filter buttons in one centered row.
	var roles: Array[StringName] = [&"tank", &"fighter", &"assassin", &"marksman", &"mage", &"support"]

	for i in range(roles.size()):
		var role: StringName = roles[i]
		var filter_button := Button.new()
		filter_button.name = "RoleFilter_" + String(role)
		filter_button.text = String(role).to_upper()
		filter_button.custom_minimum_size = Vector2(UiTokensScript.DRAFT_ROLE_W_PX, UiTokensScript.DRAFT_ROLE_H_PX)
		filter_button.pressed.connect(_on_role_filter_toggled.bind(role, filter_button))
		role_parent.add_child(filter_button)
		# Don't add to active filters initially - all roles should be visible (see all)
		_update_role_filter_button_style(filter_button, role, false)

	# Populate champion grid with proportional sizing
	_populate_champion_grid()
	if _role_filter_container != null:
		_role_filter_container.visible = true
	if _champion_scroll != null:
		_champion_scroll.visible = true


func _populate_champion_grid() -> void:
	_update_draft_layout(get_viewport_rect().size)
	_clear_champion_buttons()

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	var visible_champions: Array[StringName] = []

	# Calculate base tile size from ALL champions (not filtered) to maintain consistent size
	if _base_champion_tile_size == 0:
		_base_champion_tile_size = DraftLayoutScript.calculate_champion_tile_size(champion_ids)

	# Filter by role
	for champion_id in champion_ids:
		if _active_role_filters.is_empty():
			visible_champions.append(champion_id)
		else:
			var champion: Variant = ChampionCatalogScript.get_champion(champion_id)
			if champion != null:
				var champion_dict: Dictionary = champion.to_dict()
				var stats_dict: Dictionary = champion_dict.get("stats", {})
				var role: StringName = StringName(stats_dict.get("role", ""))
				if _active_role_filters.has(role):
					visible_champions.append(champion_id)

	# Sort by name
	visible_champions.sort_custom(func(a, b): return String(a) < String(b))
	_update_champion_grid_columns(_base_champion_tile_size)

	for champion_id in visible_champions:
		var champion: Variant = ChampionCatalogScript.get_champion(champion_id)
		if champion == null:
			continue
		
		var champion_dict: Dictionary = champion.to_dict()
		var stats_dict: Dictionary = champion_dict.get("stats", {})
		var role: StringName = StringName(stats_dict.get("role", ""))

		var is_taken: bool = champion_id in _player_picks or champion_id in _enemy_picks or champion_id in _player_bans or champion_id in _enemy_bans
		var is_banned: bool = champion_id in _player_bans or champion_id in _enemy_bans
		var team_owner := _team_owner_for_champion(champion_id)
		var button := DraftChampionTileScene.instantiate() as Button
		button.name = "Champion_" + String(champion_id)
		var role_color: Color = SimConstantsScript.ROLE_COLORS.get(String(role), UiTokensScript.COLOR_BUTTON)
		button.call("setup", champion_id, role_color, is_taken, is_banned, team_owner, _base_champion_tile_size)
		button.tooltip_text = ""
		_register_champ_tooltip(button, champion_id)
		button.connect("champion_pressed", _on_champion_clicked)
		_champion_grid.add_child(button)

	# Force redraw after creating new buttons
	_champion_grid.queue_redraw()
	
	# Center the grid after it's populated
	if _draft_shell != null:
		_draft_shell.center_champion_grid()


func _update_champion_grid_columns(tile_size: int = UiTokensScript.DRAFT_CHAMPION_TILE_PX) -> void:
	if _champion_grid == null:
		return


func _team_owner_for_champion(champion_id: StringName) -> StringName:
	if champion_id in _player_picks or champion_id in _player_bans:
		return &"player"
	if champion_id in _enemy_picks or champion_id in _enemy_bans:
		return &"enemy"
	return &""


func _on_role_filter_toggled(role: StringName, button: Button) -> void:
	# If the clicked role is already selected, deselect it (go back to see all)
	if _active_role_filters.has(role):
		_active_role_filters.clear()
	else:
		# Clear any existing filter and set only the clicked role
		_active_role_filters.clear()
		_active_role_filters.append(role)
	
	# Update all role filter button styles
	var role_parent: Control = _role_filter_container if _role_filter_container != null else _header_panel
	for child in role_parent.get_children():
		if child is Button and child.name.begins_with("RoleFilter"):
			var button_role := String(child.name.replace("RoleFilter_", ""))
			var is_active := StringName(button_role) in _active_role_filters
			_update_role_filter_button_style(child, StringName(button_role), is_active)

	# Fresh pull and redraw of champion UI buttons
	_populate_champion_grid()


func _update_role_filter_button_style(button: Button, role: StringName, is_active: bool) -> void:
	var role_color: Color = SimConstantsScript.ROLE_COLORS.get(String(role), UiTokensScript.COLOR_BUTTON)
	
	# Reset modulate to white to avoid affecting StyleBoxFlat
	button.modulate = Color.WHITE
	
	if is_active or _active_role_filters.is_empty():
		# Bright when this specific filter is active OR all filters are empty (see all)
		var fill_color := UiStylesScript.fill_for_role(role_color)
		var style_box := UiStylesScript.panel(fill_color, role_color, 6)
		style_box.set_corner_radius_all(4)
		button.add_theme_stylebox_override("normal", style_box)
		button.add_theme_stylebox_override("hover", style_box)
		button.add_theme_stylebox_override("pressed", style_box)
		button.add_theme_color_override("font_color", Color.WHITE)
		button.add_theme_color_override("font_outline_color", Color.BLACK)
		button.add_theme_constant_override("outline_size", 1)
	else:
		# Dimmed when this specific filter is inactive (others are active)
		var dimmed_color := Color(
			max(0.0, role_color.r * 0.5),
			max(0.0, role_color.g * 0.5),
			max(0.0, role_color.b * 0.5)
		)
		var style_box := UiStylesScript.panel(UiTokensScript.COLOR_PANEL, dimmed_color, 2)
		style_box.set_corner_radius_all(4)
		button.add_theme_stylebox_override("normal", style_box)
		button.add_theme_stylebox_override("hover", style_box)
		button.add_theme_stylebox_override("pressed", style_box)
		button.add_theme_color_override("font_color", dimmed_color)


func _update_turn_display() -> void:
	if _turn_label == null:
		return

	if _draft_step_index < SimConstantsScript.DRAFT_SEQUENCE.size():
		var current_turn: String = SimConstantsScript.DRAFT_SEQUENCE[_draft_step_index]
		var phase := "BAN PHASE" if current_turn.ends_with("_BAN") else "PICK PHASE"
		var team := "BLUE" if _turn_belongs_to_player(current_turn) else "RED"
		
		# Set team color for entire label
		var team_color := UiTokensScript.COLOR_ZONE_P1 if _turn_belongs_to_player(current_turn) else UiTokensScript.COLOR_ZONE_P2
		_turn_label.modulate = team_color
		
		# Add dark background for better contrast
		_turn_label.add_theme_color_override("font_color", team_color)
		_turn_label.add_theme_color_override("font_shadow_color", UiTokensScript.COLOR_TIMER_SHADOW)
		_turn_label.add_theme_constant_override("shadow_offset_x", 2)
		_turn_label.add_theme_constant_override("shadow_offset_y", 2)
		# Add white border/outline
		_turn_label.add_theme_color_override("font_outline_color", Color.WHITE)
		_turn_label.add_theme_constant_override("outline_size", 2)
		
		# Add spacing between turn label and action box by setting fixed offset
		if _draft_action_box != null:
			_draft_action_box.offset_top = UiTokensScript.DRAFT_ACTION_TOP_PX + 30
		
		# Set larger font size for entire label
		_turn_label.remove_theme_font_size_override("font_size")
		_turn_label.add_theme_font_size_override("font_size", 48)

		if current_turn.ends_with("_BAN"):
			_turn_label.text = "%s - %s BAN" % [phase, team]
		elif current_turn.ends_with("_PICK"):
			var pick_count := _player_picks.size() if _turn_belongs_to_player(current_turn) else _enemy_picks.size()
			_turn_label.text = "%s - %s PICK (%d/%d)" % [phase, team, pick_count + 1, MAX_TEAM_SIZE]
		else:
			_turn_label.text = "TURN: %s" % current_turn.replace("_", " ")
	else:
		_turn_label.text = "TEAMS READY - START MATCH"
		_turn_label.modulate = Color.WHITE
		_turn_label.add_theme_font_size_override("font_size", 24)
		_turn_label.remove_theme_color_override("font_color")
		_turn_label.remove_theme_color_override("font_shadow_color")
		_turn_label.remove_theme_constant_override("shadow_offset_x")
		_turn_label.remove_theme_constant_override("shadow_offset_y")
		_turn_label.remove_theme_color_override("font_outline_color")
		_turn_label.remove_theme_constant_override("outline_size")
	_turn_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER


func _update_team_rosters() -> void:
	for child in _player_team_list.get_children():
		_player_team_list.remove_child(child)
		child.free()
	for child in _enemy_team_list.get_children():
		_enemy_team_list.remove_child(child)
		child.free()
	if _player_bans_list != null:
		for child in _player_bans_list.get_children():
			_player_bans_list.remove_child(child)
			child.free()
	if _enemy_bans_list != null:
		for child in _enemy_bans_list.get_children():
			_enemy_bans_list.remove_child(child)
			child.free()

	_player_team_list.add_theme_constant_override("separation", 2)
	_enemy_team_list.add_theme_constant_override("separation", 2)
	if _player_bans_list != null:
		_player_bans_list.add_theme_constant_override("separation", 2)
	if _enemy_bans_list != null:
		_enemy_bans_list.add_theme_constant_override("separation", 2)

	# VBoxContainer lays out; do not set per-label y (was stacking with flow + races from await).
	for i in range(_player_picks.size()):
		var champion_id: StringName = _player_picks[i]
		var label := Label.new()
		label.text = "- %s" % String(champion_id).capitalize()
		_player_team_list.add_child(label)

	for i in range(_enemy_picks.size()):
		var champion_id: StringName = _enemy_picks[i]
		var label2 := Label.new()
		label2.text = "- %s" % String(champion_id).capitalize()
		_enemy_team_list.add_child(label2)

	if _player_bans_list != null:
		for j in range(_player_bans.size()):
			var b_id: StringName = _player_bans[j]
			var bl := Label.new()
			var champion: Variant = ChampionCatalogScript.get_champion(b_id)
			if champion != null:
				var champion_dict: Dictionary = champion.to_dict()
				var stats_dict: Dictionary = champion_dict.get("stats", {})
				bl.text = String(stats_dict.get("name", String(b_id)))
			else:
				bl.text = String(b_id)
			bl.add_theme_color_override("font_color", UiTokensScript.COLOR_SUBTLE)
			_player_bans_list.add_child(bl)

	if _enemy_bans_list != null:
		for j in range(_enemy_bans.size()):
			var b_id: StringName = _enemy_bans[j]
			var bl := Label.new()
			var champion: Variant = ChampionCatalogScript.get_champion(b_id)
			if champion != null:
				var champion_dict: Dictionary = champion.to_dict()
				var stats_dict: Dictionary = champion_dict.get("stats", {})
				bl.text = String(stats_dict.get("name", String(b_id)))
			else:
				bl.text = String(b_id)
			bl.add_theme_color_override("font_color", UiTokensScript.COLOR_SUBTLE)
			_enemy_bans_list.add_child(bl)


func _update_start_match_enabled() -> void:
	if _start_match_button == null:
		return
	var can: bool = _draft_step_index >= SimConstantsScript.DRAFT_SEQUENCE.size() or _debug_mode
	_start_match_button.disabled = not can


func _power_score_for_pick(champion_id: StringName) -> float:
	var ch: Variant = ChampionCatalogScript.get_champion(champion_id)
	if ch == null:
		return 0.0
	var d: Dictionary = ch.to_dict()
	var st: Dictionary = d.get("stats", {})
	var max_hp: float = float(st.get("max_hp", 1.0))
	var ad: float = float(st.get("attack_damage", 0.0))
	var as_sp: float = maxf(0.01, float(st.get("attack_speed", 0.5)))
	return max_hp * ad * as_sp


func _softmax_select(recommendations: Array, temperature: float = 1.0) -> StringName:
	if temperature <= 0.0:
		push_error("_softmax_select: temperature must be positive, got %f" % temperature)
		temperature = 0.01
	elif temperature > 100.0:
		push_warning("_softmax_select: temperature unusually high (%f), clamping to 100.0" % temperature)
		temperature = 100.0
	
	var scores: Array[float] = []
	var champions: Array[StringName] = []
	for rec in recommendations:
		var score: float = rec.get("total_score", 0.0)
		var champion: StringName = StringName(rec.get("candidate", ""))
		if not champion.is_empty():
			scores.append(score)
			champions.append(champion)
	
	if champions.is_empty():
		return StringName()
	
	# Numerical stability: subtract max scaled score before exp
	var max_scaled: float = scores[0] * AI_SCORE_SCALE
	for score in scores:
		var scaled: float = score * AI_SCORE_SCALE
		if scaled > max_scaled:
			max_scaled = scaled
	
	var exp_values: Array[float] = []
	var exp_sum: float = 0.0
	for score in scores:
		var temp_score: float = (score * AI_SCORE_SCALE - max_scaled) / temperature
		var exp_val: float = exp(temp_score)
		exp_values.append(exp_val)
		exp_sum += exp_val
	
	var probabilities: Array[float] = []
	for exp_val in exp_values:
		probabilities.append(exp_val / exp_sum)
	
	if _debug_mode:
		print("  --- Softmax Details (temp=%.2f, scale=%.1f) ---" % [temperature, AI_SCORE_SCALE])
		for i in range(champions.size()):
			print("    #%d: %s | raw_score=%.4f | scaled=%.4f | exp_val=%.4f | prob=%.2f%%" % [i + 1, champions[i], scores[i], scores[i] * AI_SCORE_SCALE, exp_values[i], probabilities[i] * 100])
	
	var rand_val: float = randf()
	if _debug_mode:
		print("  Random sample: %.4f" % rand_val)
	var cumulative: float = 0.0
	for i in range(probabilities.size()):
		cumulative += probabilities[i]
		if rand_val <= cumulative:
			if _debug_mode:
				print("  Selected rank #%d (score: %.4f, prob: %.2f%%)" % [i + 1, scores[i], probabilities[i] * 100])
			return champions[i]
	
	return champions[champions.size() - 1]


## Returns true if the given draft turn string (e.g. "B_PICK", "R_BAN") belongs to the player side.
func _turn_belongs_to_player(current_turn: String) -> bool:
	if _player_side == "blue":
		return current_turn.begins_with("B_")
	return current_turn.begins_with("R_")


func _try_enemy_draft_ai() -> void:
	if _draft_step_index >= SimConstantsScript.DRAFT_SEQUENCE.size():
		return
	if _ai_draft_processing:
		return
	
	var current_turn: String = SimConstantsScript.DRAFT_SEQUENCE[_draft_step_index]
	
	# Only act on AI turns
	if _turn_belongs_to_player(current_turn):
		return
	
	# Get available champions
	var taken: Array[StringName] = _player_picks + _enemy_picks + _player_bans + _enemy_bans
	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	var available: Array[StringName] = []
	for cid in champion_ids:
		if cid not in taken:
			available.append(cid)
	
	if available.is_empty():
		if _debug_mode:
			print("No champions available for AI draft")
		return
	
	# Get draft recommendations from backend
	var is_ban: bool = current_turn.ends_with("_BAN")
	var stats_dir: String = "res://model_stats/stats_output_100k"
	
	var recommendations: Array
	if is_ban:
		recommendations = _backend.get_draft_ai_ban_recommendations(
			stats_dir,
			available,
			_enemy_picks,
			_player_picks,
			5,  # max_results
			_draft_step_index,
			_ai_side,  # acting_side
			Dictionary(),  # weight_overrides
			0  # strategy (NATIVE)
		)
	else:
		recommendations = _backend.get_draft_ai_pick_recommendations(
			stats_dir,
			available,
			_enemy_picks,
			_player_picks,
			5,  # max_results
			_draft_step_index,
			0  # strategy (NATIVE)
		)
	
	if _debug_mode:
		print("\n=== Team 2 (RED) AI Draft Recommendation ===")
		print("Turn: ", current_turn)
		print("Available champions: ", available)
		print("Full recommendation output:")
		print(JSON.stringify(recommendations, "\t"))
	
	var selected_champion: StringName
	if recommendations.is_empty():
		push_error("_try_enemy_draft_ai: no recommendations returned from draft AI")
		var random_index: int = randi() % available.size()
		selected_champion = available[random_index]
		if _debug_mode:
			print("Random fallback selected: ", selected_champion)
	else:
		selected_champion = _softmax_select(recommendations, AI_SELECTION_TEMPERATURE)
		if selected_champion.is_empty():
			push_error("_try_enemy_draft_ai: softmax selection failed")
			var random_index: int = randi() % available.size()
			selected_champion = available[random_index]
			if _debug_mode:
				print("Random fallback selected: ", selected_champion)
	
	if selected_champion.is_empty():
		push_error("_try_enemy_draft_ai: failed to select champion")
		return
	
	if _debug_mode:
		print("\nAI selected: ", selected_champion)
		print("===========================================\n")
	
	_ai_draft_processing = true
	var delay: float = randf_range(1.5, 3.0)
	if _debug_mode:
		print("AI thinking... (delay: %.2fs)" % delay)
	if not _draft_shell.is_ai_delay_disabled():
		await get_tree().create_timer(delay).timeout
	_ai_draft_processing = false
	
	_perform_draft_action(selected_champion)


func _pick_p2_champion(available: Array[StringName]) -> StringName:
	if available.is_empty():
		return StringName()
	var enemy_roles: Array[StringName] = []
	for h in _enemy_picks:
		var c: Variant = ChampionCatalogScript.get_champion(h)
		if c != null:
			var r: StringName = StringName(c.to_dict().get("stats", {}).get("role", &""))
			enemy_roles.append(r)
	if not enemy_roles.has(&"tank"):
		var tanks: Array[StringName] = []
		for h in available:
			var c2: Variant = ChampionCatalogScript.get_champion(h)
			if c2 != null and StringName(c2.to_dict().get("stats", {}).get("role", &"")) == &"tank":
				tanks.append(h)
		if not tanks.is_empty():
			var best: StringName = tanks[0]
			var best_hp: float = -1.0
			for t in tanks:
				var c3: Variant = ChampionCatalogScript.get_champion(t)
				if c3 == null:
					continue
				var mhp: float = float(c3.to_dict().get("stats", {}).get("max_hp", 0.0))
				if mhp > best_hp:
					best_hp = mhp
					best = t
			return best
	if not enemy_roles.has(&"marksman") and not enemy_roles.has(&"mage"):
		var ranged: Array[StringName] = []
		for h in available:
			var c4: Variant = ChampionCatalogScript.get_champion(h)
			if c4 == null:
				continue
			var role: StringName = StringName(c4.to_dict().get("stats", {}).get("role", &""))
			if role == &"marksman" or role == &"mage":
				ranged.append(h)
		if not ranged.is_empty():
			var pick: StringName = ranged[0]
			var best_p: float = -1.0
			for r in ranged:
				var s: float = _power_score_for_pick(r)
				if s > best_p:
					best_p = s
					pick = r
			return pick
	var top: StringName = available[0]
	var top_s: float = _power_score_for_pick(top)
	for h in available:
		var sc: float = _power_score_for_pick(h)
		if sc > top_s:
			top_s = sc
			top = h
	return top


func _on_random_draft_clicked() -> void:
	while _draft_step_index < SimConstantsScript.DRAFT_SEQUENCE.size():
		var current_turn: String = SimConstantsScript.DRAFT_SEQUENCE[_draft_step_index]
		
		if not _turn_belongs_to_player(current_turn):
			# AI turn: trigger if not already running, then yield frame to let it progress
			if not _ai_draft_processing:
				_try_enemy_draft_ai()
			await get_tree().process_frame
			continue
		
		var taken: Array[StringName] = _player_picks + _enemy_picks + _player_bans + _enemy_bans
		var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
		var available: Array[StringName] = []
		for cid in champion_ids:
			if not cid in taken:
				available.append(cid)

		if available.is_empty():
			break

		var random_champion: StringName = available[randi() % available.size()]
		_on_champion_clicked(random_champion)


func _on_start_match_clicked() -> void:
	if _backend == null:
		return
	if not _can_start_match():
		return
	var match_input: Object = MatchReplayInputScript.build_match_input(
		int(Time.get_ticks_msec() & 0x7FFFFFFF),
		_player_picks,
		_enemy_picks,
		SimConstantsScript.DEFAULT_TICK_RATE,
		true,      # debug_combat_trace
		false      # debug_targeting_scoring
	)
	
	_backend.begin_match(match_input)
	_game_state = GameState.COMBAT
	_sim_time_accumulator = 0.0
	_show_world_and_hud_for_battle()
	if _arena_layer != null:
		_arena_layer.show_preparation_zones = false
	if _commence_button != null:
		_commence_button.visible = false
	_lbl_combat_state.text = GameState.keys()[GameState.COMBAT]
	_hud_pause.visible = false
	_update_visualization_from_snapshot()


func _setup_control_panel() -> void:
	if _control_panel == null:
		return
	var hbox := HBoxContainer.new()
	hbox.set_anchors_preset(Control.PRESET_FULL_RECT)
	hbox.offset_left = BOTTOM_HBOX_MARGIN_H_PX
	hbox.offset_top = BOTTOM_HBOX_MARGIN_V_PX
	hbox.offset_right = -BOTTOM_HBOX_MARGIN_H_PX
	hbox.offset_bottom = -BOTTOM_HBOX_MARGIN_V_PX
	_control_panel.add_child(hbox)

	var pause_button := Button.new()
	pause_button.text = "Pause"
	pause_button.custom_minimum_size = Vector2(100, 40)
	pause_button.pressed.connect(_toggle_pause)
	hbox.add_child(pause_button)

	var restart_button := Button.new()
	restart_button.text = "Restart"
	restart_button.custom_minimum_size = Vector2(100, 40)
	restart_button.pressed.connect(_restart_match)
	hbox.add_child(restart_button)

	var menu_button := Button.new()
	menu_button.text = "Back to Menu"
	menu_button.custom_minimum_size = Vector2(120, 40)
	menu_button.pressed.connect(_on_back_to_menu)
	hbox.add_child(menu_button)


func _setup_inspection_panel() -> void:
	if _inspection_panel == null:
		return
	_inspection_panel.visible = false

	# Create inspection label
	var label := Label.new()
	label.name = "InspectionLabel"
	label.text = "Select a unit to inspect"
	_inspection_panel.add_child(label)


func _update_inspection_panel(instance_id: int) -> void:
	if _inspection_panel == null:
		return

	_inspection_panel.visible = true
	var label: Label = _inspection_panel.get_node("InspectionLabel")

	var snapshot: Dictionary = _backend.get_tick_snapshot()
	if snapshot.is_empty():
		label.text = "No snapshot available"
		return

	var units: Array = snapshot.get("units", [])
	for unit_data in units:
		var unit_dict: Dictionary = Dictionary(unit_data)
		var uid: int = int(unit_dict.get("instance_id", 0))
		if uid == instance_id:
			var unit_id: StringName = StringName(unit_dict.get("unit_id", ""))
			var ut: String = String(unit_dict.get("team", ""))
			var hp: float = float(unit_dict.get("hp", 0.0))
			var max_hp: float = float(unit_dict.get("max_hp", 0.0))
			var state: StringName = StringName(unit_dict.get("state", ""))
			var target_id: int = int(unit_dict.get("target_id", 0))

			label.text = "Unit: %s\nTeam: %s\nHP: %.1f/%.1f\nState: %s\nTarget: %d" % [unit_id, ut, hp, max_hp, state, target_id]
			return

	label.text = "Unit %d not found in snapshot" % instance_id


## Applies a champion selection for the current draft turn, advancing state and UI.
## Called directly by both human clicks and the AI after its delay.
func _perform_draft_action(champion_id: StringName) -> void:
	if _game_state != GameState.DRAFTING:
		return
	if _draft_step_index >= SimConstantsScript.DRAFT_SEQUENCE.size():
		return

	var current_turn: String = SimConstantsScript.DRAFT_SEQUENCE[_draft_step_index]
	var is_player_turn: bool = _turn_belongs_to_player(current_turn)

	# Handle ban phases
	if current_turn.ends_with("_BAN"):
		if champion_id not in _player_bans and champion_id not in _enemy_bans and champion_id not in _player_picks and champion_id not in _enemy_picks:
			if is_player_turn:
				_player_bans.append(champion_id)
			else:
				_enemy_bans.append(champion_id)
		_draft_step_index += 1
		_update_turn_display()
		_update_team_rosters()
		_update_champion_button_style(champion_id)
		_update_start_match_enabled()
		_try_enemy_draft_ai()
	# Handle pick phases
	elif current_turn.ends_with("_PICK"):
		if is_player_turn:
			if _player_picks.size() < MAX_TEAM_SIZE:
				_player_picks.append(champion_id)
		else:
			if _enemy_picks.size() < MAX_TEAM_SIZE:
				_enemy_picks.append(champion_id)

		_draft_step_index += 1
		_update_turn_display()
		_update_team_rosters()
		_update_champion_button_style(champion_id)
		_update_start_match_enabled()
		_try_enemy_draft_ai()


func _on_champion_clicked(champion_id: StringName) -> void:
	if _ai_draft_processing:
		return
	if _game_state != GameState.DRAFTING:
		return
	if _draft_step_index >= SimConstantsScript.DRAFT_SEQUENCE.size():
		return

	var current_turn: String = SimConstantsScript.DRAFT_SEQUENCE[_draft_step_index]
	# Only allow human input on player turns
	if not _turn_belongs_to_player(current_turn):
		return

	_perform_draft_action(champion_id)



func _update_champion_button_style(champion_id: StringName) -> void:
	var button: Button = _get_champion_button(champion_id)
	if button == null:
		return
	var is_taken: bool = champion_id in _player_picks or champion_id in _enemy_picks or champion_id in _player_bans or champion_id in _enemy_bans
	var is_banned: bool = champion_id in _player_bans or champion_id in _enemy_bans
	button.call("refresh_state", is_taken, is_banned, _team_owner_for_champion(champion_id))


func _get_champion_button(champion_id: StringName) -> Button:
	var button_name := "Champion_" + String(champion_id)
	if _champion_grid != null:
		var grid_button: Button = _champion_grid.get_node_or_null(button_name) as Button
		if grid_button != null:
			return grid_button
	if _header_panel != null:
		return _header_panel.get_node_or_null(button_name) as Button
	return null


func _toggle_pause() -> void:
	_combat_paused = not _combat_paused
	if _hud_pause != null:
		_hud_pause.visible = _combat_paused and _game_state == GameState.COMBAT
		if _hud_pause.visible:
			_hud_pause.text = "PAUSED (Space)  |  K: speed"
	_update_tween_pause_state()


func _update_tween_speed_scale() -> void:
	for tween in _active_tweens:
		if is_instance_valid(tween):
			tween.set_speed_scale(_sim_speed)


func _update_tween_pause_state() -> void:
	for tween in _active_tweens:
		if is_instance_valid(tween):
			if _combat_paused:
				tween.pause()
			else:
				tween.play()


func _register_tween(tween: Tween) -> void:
	tween.set_speed_scale(_sim_speed)
	if _combat_paused:
		tween.pause()
	_active_tweens.append(tween)
	tween.finished.connect(_cleanup_tween.bind(tween))


func _cleanup_tween(tween: Tween) -> void:
	_active_tweens.erase(tween)


func _restart_match() -> void:
	if _match_overlay != null:
		_match_overlay.visible = false
	if _overtime_border != null:
		_overtime_border.visible = false
	_reset_to_draft()


func _enter_preparation() -> void:
	if _backend == null:
		return
	if not _can_start_match():
		return
	var match_input: Object = MatchReplayInputScript.build_match_input(
		int(Time.get_ticks_msec() & 0x7FFFFFFF),
		_player_picks,
		_enemy_picks,
		SimConstantsScript.DEFAULT_TICK_RATE
	)
	_backend.begin_match(match_input)
	_game_state = GameState.PREPARATION
	_sim_time_accumulator = 0.0
	_on_before_battle_start()
	_show_world_and_hud_for_battle()
	if _arena_layer != null:
		_arena_layer.show_preparation_zones = true
	if _commence_button != null:
		_commence_button.visible = true
	_lbl_combat_state.text = GameState.keys()[GameState.PREPARATION]
	_hud_pause.visible = false
	_update_visualization_from_snapshot()


func _can_start_match() -> bool:
	if _player_picks.is_empty() or _enemy_picks.is_empty():
		return false
	if _draft_step_index < SimConstantsScript.DRAFT_SEQUENCE.size() and not _debug_mode:
		return false
	return true


func _on_back_to_menu() -> void:
	get_tree().change_scene_to_file("res://scenes/game_root.tscn")


func _show_world_and_hud_for_battle() -> void:
	_update_battle_layer_layout()
	if _draft_shell != null:
		_draft_shell.set_draft_visible(false)
		_draft_shell.set_battle_actions_visible(false)
	if _world_layer != null:
		_world_layer.visible = true
	if _aoe_fx_layer != null:
		_aoe_fx_layer.visible = true
	if _control_panel != null:
		_control_panel.visible = true
	var top_hud: Control = _ui_layer.get_node_or_null("CombatHUD")
	if top_hud != null:
		top_hud.visible = true
	if _p1_roster_labels != null:
		_p1_roster_labels.get_parent().visible = true
	if _p2_roster_labels != null:
		_p2_roster_labels.get_parent().visible = true


func _on_commence_battle() -> void:
	if _game_state != GameState.PREPARATION:
		return
	if _commence_button != null:
		_commence_button.visible = false
	if _arena_layer != null:
		_arena_layer.show_preparation_zones = false
	_game_state = GameState.COMBAT
	_sim_time_accumulator = 0.0
	_lbl_combat_state.text = GameState.keys()[GameState.COMBAT]


func _end_match() -> void:
	_game_state = GameState.MATCH_OVER
	var summary: Dictionary = _backend.finish_and_summarize()
	_combat_paused = false
	
	# Clean up unit nodes to prevent memory leaks
	_clear_units()
	
	_show_match_results(summary)


func _show_match_results(summary: Dictionary) -> void:
	if _match_overlay == null or _match_title == null or _match_stats_container == null:
		return
	_match_overlay.visible = true
	# Hide overtime border when match ends
	if _overtime_border != null:
		_overtime_border.visible = false
	var w: String = str(summary.get("winner_team", "draw"))
	_match_title.remove_theme_color_override("font_color")
	if w == "player":
		_match_title.text = "PLAYER 1 VICTORY"
		_match_title.add_theme_color_override("font_color", UiTokensScript.COLOR_PLAYER)
	elif w == "enemy":
		_match_title.text = "PLAYER 2 VICTORY"
		_match_title.add_theme_color_override("font_color", UiTokensScript.COLOR_ENEMY)
	else:
		_match_title.text = "MATCH DRAW"
		_match_title.add_theme_color_override("font_color", UiTokensScript.COLOR_TEXT)
	for c in _match_stats_container.get_children():
		c.queue_free()
	var pi: int = int(summary.get("player_kills", 0))
	var ei: int = int(summary.get("enemy_kills", 0))
	var duration: float = float(summary.get("duration", 0.0))
	var sudden_death_ticks: int = int(summary.get("sudden_death_ticks", 0))
	# If summary has no kills at root, 0-0
	var sc_line := Label.new()
	sc_line.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	sc_line.text = "Final score (kills): %d - %d" % [pi, ei]
	_match_stats_container.add_child(sc_line)
	var duration_line := Label.new()
	duration_line.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	if sudden_death_ticks > 0:
		var overtime: float = duration - 60.0
		duration_line.text = "Duration: %.1fs (OVERTIME: %.1fs)" % [duration, overtime]
		duration_line.add_theme_color_override("font_color", UiTokensScript.COLOR_OVERTIME_TEXT)
	else:
		duration_line.text = "Duration: %.1fs" % duration
	_match_stats_container.add_child(duration_line)
	var us: Array = Array(summary.get("unit_stats", []))
	us.sort_custom(
		func(a, b):
			return float((a as Dictionary).get("damage_dealt", 0.0)) > float((b as Dictionary).get("damage_dealt", 0.0))
	)
	for item in us:
		if item is not Dictionary:
			continue
		var u: Dictionary = item
		var row := Label.new()
		row.text = "%s  |  %s  |  K/D/A %d/%d/%d  |  Dmg %.0f  |  Heal %.0f  |  Mitig %.0f" % [
			String(u.get("archetype", u.get("unit_id", "?"))),
			String(u.get("team", "?")),
			int(u.get("kills", 0)),
			int(u.get("deaths", 0)),
			int(u.get("assists", 0)),
			float(u.get("damage_dealt", 0.0)),
			float(u.get("healing_done", 0.0)),
			float(u.get("damage_mitigated", 0.0)),
		]
		_match_stats_container.add_child(row)
	var ch := _ui_layer.get_node_or_null("CombatHUD")
	if ch != null:
		ch.visible = false
	if _p1_roster_labels != null:
		var p1p = _p1_roster_labels.get_parent()
		if p1p != null:
			p1p.visible = false
	if _p2_roster_labels != null:
		var p2p = _p2_roster_labels.get_parent()
		if p2p != null:
			p2p.visible = false


func _on_new_draft_from_match() -> void:
	if _match_overlay != null:
		_match_overlay.visible = false
	if _overtime_border != null:
		_overtime_border.visible = false
	_reset_to_draft()


func _reset_to_draft() -> void:
	_clear_units()
	_game_state = GameState.DRAFTING
	_combat_paused = false
	_draft_step_index = 0
	_player_picks.clear()
	_enemy_picks.clear()
	_player_bans.clear()
	_enemy_bans.clear()
	_sim_time_accumulator = 0.0
	_selected_unit_id = 0
	if _commence_button != null:
		_commence_button.visible = false
	if _arena_layer != null:
		_arena_layer.show_preparation_zones = false
	if _draft_shell != null:
		_draft_shell.set_draft_visible(true)
		_draft_shell.set_battle_actions_visible(true)
	if _world_layer != null:
		_world_layer.visible = false
	if _aoe_fx_layer != null:
		_aoe_fx_layer.visible = false
	if _control_panel != null:
		_control_panel.visible = false
	var ch2: Control = _ui_layer.get_node_or_null("CombatHUD") as Control
	if ch2 != null:
		ch2.visible = false
	if _p1_roster_labels != null:
		var p1a = _p1_roster_labels.get_parent()
		if p1a != null:
			p1a.visible = false
	if _p2_roster_labels != null:
		var p2a = _p2_roster_labels.get_parent()
		if p2a != null:
			p2a.visible = false
	_on_reset_to_draft_extra()
	_update_turn_display()
	_update_team_rosters()
	_populate_champion_grid()
	_update_start_match_enabled()
	if _inspection_panel != null:
		_inspection_panel.visible = false


func _on_reset_to_draft_extra() -> void:
	pass


func _clear_units() -> void:
	if _aoe_fx_layer != null:
		for c in _aoe_fx_layer.get_children():
			if is_instance_valid(c):
				c.queue_free()

	# Cleanup all unit-specific FX using centralized function
	for unit_id in _unit_nodes:
		_cleanup_unit_fx(unit_id)

	for unit_id in _unit_nodes:
		var unit_node: Node2D = _unit_nodes[unit_id]
		if unit_node != null and is_instance_valid(unit_node):
			unit_node.queue_free()
	_unit_nodes.clear()

	for proj_id in _projectile_nodes:
		var proj_node: Node2D = _projectile_nodes[proj_id]
		if proj_node != null and is_instance_valid(proj_node):
			proj_node.queue_free()
	_projectile_nodes.clear()

	for floating_text in _floating_texts:
		if is_instance_valid(floating_text):
			floating_text.queue_free()
	_floating_texts.clear()
