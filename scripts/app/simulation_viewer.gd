extends Control

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const ViewerUnitNodeScript := preload("res://scripts/app/viewer_unit_node.gd")
const SimulationViewerArenaScript := preload("res://scripts/app/simulation_viewer_arena.gd")
const AoeRingNodeScript := preload("res://scripts/app/aoe_ring_node.gd")

# Colors from Python pygame_viewer.py
const COLOR_BG := Color(0.078, 0.078, 0.102, 1.0)
const COLOR_PANEL := Color(0.11, 0.11, 0.149, 1.0)
const COLOR_GRID := Color(0.157, 0.157, 0.204, 1.0)
const COLOR_ZONE_P1 := Color(0.157, 0.196, 0.314, 1.0)
const COLOR_ZONE_P2 := Color(0.314, 0.157, 0.157, 1.0)
const COLOR_PLAYER := Color(0.275, 0.51, 1.0)
const COLOR_ENEMY := Color(0.882, 0.314, 0.314, 1.0)
const COLOR_HP_BG := Color(0.314, 0.314, 0.314, 1.0)
const COLOR_HP_FILL := Color(0.314, 0.824, 0.314, 1.0)
const COLOR_TARGET_LINE := Color(0.824, 0.824, 0.431, 1.0)
const COLOR_TEXT := Color(0.902, 0.902, 0.902, 1.0)
const COLOR_SUBTLE := Color(0.706, 0.706, 0.745, 1.0)
const COLOR_BUTTON := Color(0.275, 0.51, 0.863, 1.0)
const COLOR_BUTTON_DISABLED := Color(0.275, 0.275, 0.314, 1.0)
const COLOR_BUTTON_TEXT := Color(0.941, 0.941, 0.941, 1.0)
const COLOR_WARNING := Color(0.922, 0.667, 0.392, 1.0)
const COLOR_SUCCESS := Color(0.196, 0.902, 0.196, 1.0)
const COLOR_HIGHLIGHT := Color(0.471, 0.863, 0.549, 1.0)
const COLOR_SECTION_BG := Color(0.133, 0.133, 0.18, 1.0)
const COLOR_SECTION_BORDER := Color(0.275, 0.275, 0.353, 1.0)
## World units map to this fraction of min(viewport) for a consistent projectile read.
const VIEWER_BASE_MIN_AXIS: float = 720.0
## Screen-space radius for projectiles (scales slightly with window).
const PROJECTILE_BASE_RADIUS_PX: float = 4.0

const ROLE_COLORS: Dictionary = {
	"tank": Color(0.8, 0.2, 0.2),
	"fighter": Color(0.824, 0.412, 0.118),
	"assassin": Color(0.6, 0.196, 0.8),
	"marksman": Color(0.133, 0.545, 0.133),
	"mage": Color(0.3, 0.6, 0.8),
	"support": Color(0.855, 0.647, 0.125),
}

# Game states
const DRAFTING := "DRAFTING"
const PREPARATION := "PREPARATION"
const COMBAT := "COMBAT"
const MATCH_OVER := "MATCH_OVER"

# Draft configuration
const MAX_TEAM_SIZE := 5
const DRAFT_SEQUENCE := [
	"P1_PICK", "P2_PICK",
	"P1_PICK", "P2_PICK",
	"P1_PICK", "P2_PICK",
	"P1_PICK", "P2_PICK",
	"P1_PICK", "P2_PICK"
]

# Simulation configuration
var _backend: RefCounted = null
var _game_state: String = DRAFTING
var _sim_time_accumulator: float = 0.0
var _sim_speed: float = 1.0
var _combat_paused: bool = false

# Draft state
var _player_picks: Array[StringName] = []
var _enemy_picks: Array[StringName] = []
var _draft_step_index: int = 0

# Unit visualization
var _unit_nodes: Dictionary = {}  # instance_id -> Node2D
var _projectile_nodes: Dictionary = {}  # projectile_id -> Node2D
var _floating_texts: Array = []  # Array of floating text nodes

# Selection
var _selected_unit_id: int = 0
var _debug_mode: bool = true

# Draft filtering
var _active_role_filters: Array[StringName] = []
var _banned_heroes: Array[StringName] = []

# UI references
var _world_layer: Node2D
## Layer 1: AoE / impact rings (above world; helps when native tick_fx is missing aoe_ entries or Line2D glitches)
var _aoe_fx_layer: CanvasLayer
var _ui_layer: Control
var _header_panel: Panel
var _grid_panel: Panel
var _title_label: Label
var _turn_label: Label
var _player_team_label: Label
var _player_team_list: VBoxContainer
var _enemy_team_label: Label
var _enemy_team_list: VBoxContainer
var _banned_list: VBoxContainer
var _banned_list_ref: VBoxContainer
var _role_filter_container: HBoxContainer
var _champion_grid: GridContainer
var _champion_grid_ref: GridContainer
var _random_draft_button: Button
var _start_match_button: Button
var _control_panel: Panel
var _inspection_panel: Panel
var _arena_layer: Node2D
var _commence_button: Button
var _match_overlay: ColorRect
var _match_title: Label
var _match_stats_container: VBoxContainer
var _lbl_timer: Label
var _lbl_score: Label
var _lbl_combat_state: Label
var _hud_pause: Label
var _speed_button: Button
var _p1_roster_labels: VBoxContainer
var _p2_roster_labels: VBoxContainer


func _ready() -> void:
	_backend = NativeSimulationBackendScript.new()
	if not _backend.is_available():
		push_error("Native simulation backend not available. Simulation viewer requires native backend.")
		return

	# Create UI elements programmatically to avoid scene instantiation issues
	_create_ui_structure()

	_apply_color_scheme()
	_setup_draft_ui()
	_setup_control_panel()
	_setup_inspection_panel()
	_update_turn_display()
	_update_team_rosters()
	_update_start_match_enabled()

	if _debug_mode:
		print("Simulation Viewer ready. Game state: ", _game_state)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_SPACE and (_game_state == COMBAT or _game_state == PREPARATION):
			_toggle_pause()
			get_viewport().set_input_as_handled()
		if event.keycode == KEY_K and (_game_state == COMBAT or _game_state == PREPARATION):
			_on_speed_toggle()
			get_viewport().set_input_as_handled()


func _on_speed_toggle() -> void:
	_sim_speed = 0.5 if is_equal_approx(_sim_speed, 1.0) else 1.0
	if _speed_button != null:
		_speed_button.text = "Speed: 0.5x" if _sim_speed < 0.75 else "Speed: 1x"


func _create_ui_structure() -> void:
	var screen_size := get_viewport_rect().size

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
	_ui_layer.size = screen_size
	_ui_layer.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(_ui_layer)

	# Create DraftPanel (full screen for drafting)
	_header_panel = Panel.new()
	_header_panel.name = "DraftPanel"
	_header_panel.position = Vector2(0.0, 0.0)
	_header_panel.size = screen_size
	_header_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_ui_layer.add_child(_header_panel)

	# Connect to window resize signal
	get_tree().root.connect("size_changed", _on_window_resized)

	# Create TitleLabel (centered at top)
	_title_label = Label.new()
	_title_label.name = "TitleLabel"
	_title_label.text = "HERO SELECTION"
	_title_label.position = Vector2(screen_size.x / 2.0 - 70.0, 14.0)
	_header_panel.add_child(_title_label)

	# Create TurnLabel (centered below title)
	_turn_label = Label.new()
	_turn_label.name = "TurnLabel"
	_turn_label.text = "TURN: P1_PICK"
	_turn_label.position = Vector2(screen_size.x / 2.0 - 50.0, 50.0)
	_header_panel.add_child(_turn_label)

	# Create DebugLabel (top-right)
	var debug_label := Label.new()
	debug_label.name = "DebugLabel"
	debug_label.text = "DEBUG MODE: ON"
	debug_label.position = Vector2(screen_size.x - 150.0, 10.0)
	debug_label.visible = _debug_mode
	debug_label.add_theme_color_override("font_color", COLOR_WARNING)
	_header_panel.add_child(debug_label)

	# Create PlayerTeamLabel (top-left)
	_player_team_label = Label.new()
	_player_team_label.name = "PlayerTeamLabel"
	_player_team_label.text = "PLAYER 1 TEAM"
	_player_team_label.position = Vector2(40.0, 12.0)
	_header_panel.add_child(_player_team_label)

	# Create PlayerTeamList (below player team label)
	_player_team_list = VBoxContainer.new()
	_player_team_list.name = "PlayerTeamList"
	_player_team_list.position = Vector2(40.0, 50.0)
	_header_panel.add_child(_player_team_list)

	# Create EnemyTeamLabel (top-right)
	_enemy_team_label = Label.new()
	_enemy_team_label.name = "EnemyTeamLabel"
	_enemy_team_label.text = "PLAYER 2 TEAM"
	_enemy_team_label.position = Vector2(screen_size.x - 240.0, 12.0)
	_header_panel.add_child(_enemy_team_label)

	# Create EnemyTeamList (below enemy team label)
	_enemy_team_list = VBoxContainer.new()
	_enemy_team_list.name = "EnemyTeamList"
	_enemy_team_list.position = Vector2(screen_size.x - 240.0, 50.0)
	_header_panel.add_child(_enemy_team_list)

	# Create BannedLabel (centered lower)
	var banned_label := Label.new()
	banned_label.name = "BannedLabel"
	banned_label.text = "BANNED"
	banned_label.position = Vector2(screen_size.x / 2.0 - 100.0, 220.0)
	banned_label.add_theme_color_override("font_color", COLOR_SUBTLE)
	_header_panel.add_child(banned_label)

	# Create BannedList (below banned label)
	_banned_list = VBoxContainer.new()
	_banned_list.name = "BannedList"
	_banned_list.position = Vector2(screen_size.x / 2.0 - 100.0, 250.0)
	_header_panel.add_child(_banned_list)

	# Store reference for banned list
	_banned_list_ref = _banned_list

	# Role filter buttons will be created in _setup_draft_ui
	_role_filter_container = HBoxContainer.new()
	_role_filter_container.name = "RoleFilterContainer"
	_role_filter_container.position = Vector2(0.0, 0.0)
	_role_filter_container.visible = false
	_header_panel.add_child(_role_filter_container)

	# RandomDraftButton (centered, increased size)
	_random_draft_button = Button.new()
	_random_draft_button.name = "RandomDraftButton"
	_random_draft_button.text = "RANDOM DRAFT"
	_random_draft_button.size = Vector2(screen_size.x * 0.26, screen_size.y * 0.07)
	_random_draft_button.position = Vector2(screen_size.x / 2.0 - (screen_size.x * 0.26) / 2.0, screen_size.y * 0.17)
	_header_panel.add_child(_random_draft_button)

	# StartMatchButton (centered, increased size)
	_start_match_button = Button.new()
	_start_match_button.name = "StartMatchButton"
	_start_match_button.text = "START MATCH"
	_start_match_button.size = Vector2(screen_size.x * 0.26, screen_size.y * 0.07)
	_start_match_button.position = Vector2(screen_size.x / 2.0 - (screen_size.x * 0.26) / 2.0, screen_size.y * 0.24)
	_header_panel.add_child(_start_match_button)

	# ChampionGrid (will be populated manually)
	_champion_grid = GridContainer.new()
	_champion_grid.name = "ChampionGrid"
	_champion_grid.position = Vector2(20.0, 310.0)
	_champion_grid.visible = false
	_header_panel.add_child(_champion_grid)

	# Store champion grid reference for manual positioning
	_champion_grid_ref = _champion_grid

	# Combat HUD elements (hidden initially)
	_control_panel = Panel.new()
	_control_panel.name = "ControlPanel"
	_control_panel.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	_control_panel.offset_top = -60.0
	_control_panel.custom_minimum_size = Vector2(0.0, 60.0)
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


func world_to_screen(wx: float, wy: float) -> Vector2:
	var vp: Vector2 = get_viewport_rect().size
	var wxc: float = clampf(wx, 0.0, SimConstantsScript.WORLD_SIZE)
	var wyc: float = clampf(wy, 0.0, SimConstantsScript.WORLD_SIZE)
	return Vector2((wxc / SimConstantsScript.WORLD_SIZE) * vp.x, (wyc / SimConstantsScript.WORLD_SIZE) * vp.y)


func screen_to_world(screen: Vector2) -> Vector2:
	var vp: Vector2 = get_viewport_rect().size
	if vp.x < 0.001 or vp.y < 0.001:
		return Vector2.ZERO
	return Vector2(
		(screen.x / vp.x) * SimConstantsScript.WORLD_SIZE,
		(screen.y / vp.y) * SimConstantsScript.WORLD_SIZE
	)


func _build_combat_overlay_hud() -> void:
	var top := HBoxContainer.new()
	top.name = "CombatHUD"
	top.visible = false
	top.set_anchors_preset(Control.PRESET_TOP_WIDE)
	top.offset_top = 8.0
	top.offset_bottom = 52.0
	top.alignment = BoxContainer.ALIGNMENT_CENTER
	top.add_theme_constant_override("separation", 24)
	_ui_layer.add_child(top)

	_lbl_timer = Label.new()
	_lbl_timer.add_theme_color_override("font_color", COLOR_TEXT)
	_lbl_timer.add_theme_font_size_override("font_size", 20)
	top.add_child(_lbl_timer)

	_lbl_score = Label.new()
	_lbl_score.add_theme_color_override("font_color", COLOR_WARNING)
	_lbl_score.add_theme_font_size_override("font_size", 20)
	top.add_child(_lbl_score)

	_lbl_combat_state = Label.new()
	_lbl_combat_state.add_theme_color_override("font_color", COLOR_SUBTLE)
	_lbl_combat_state.add_theme_font_size_override("font_size", 16)
	top.add_child(_lbl_combat_state)

	_hud_pause = Label.new()
	_hud_pause.visible = false
	_hud_pause.add_theme_color_override("font_color", COLOR_WARNING)
	top.add_child(_hud_pause)

	_speed_button = Button.new()
	_speed_button.text = "Speed: 1x"
	_speed_button.tooltip_text = "Toggle 0.5x / 1x (keyboard: K)"
	_speed_button.flat = true
	_speed_button.add_theme_color_override("font_color", COLOR_TEXT)
	_speed_button.pressed.connect(_on_speed_toggle)
	top.add_child(_speed_button)

	var side_l := VBoxContainer.new()
	side_l.name = "P1RosterHUD"
	side_l.visible = false
	side_l.set_anchors_preset(Control.PRESET_LEFT_WIDE)
	side_l.offset_left = 12.0
	side_l.offset_top = 64.0
	side_l.offset_right = 240.0
	side_l.offset_bottom = 400.0
	_ui_layer.add_child(side_l)
	_p1_roster_labels = VBoxContainer.new()
	side_l.add_child(_p1_roster_labels)

	var side_r := VBoxContainer.new()
	side_r.name = "P2RosterHUD"
	side_r.visible = false
	side_r.set_anchors_preset(Control.PRESET_RIGHT_WIDE)
	side_r.offset_right = -12.0
	side_r.offset_top = 64.0
	side_r.offset_left = -260.0
	side_r.offset_bottom = 400.0
	_ui_layer.add_child(side_r)
	_p2_roster_labels = VBoxContainer.new()
	# Top-align labels in the right strip (END stacks from bottom).
	side_r.alignment = BoxContainer.ALIGNMENT_BEGIN
	_p2_roster_labels.add_theme_constant_override("separation", 4)
	_p1_roster_labels.add_theme_constant_override("separation", 4)
	side_r.add_child(_p2_roster_labels)


func _build_match_over_overlay() -> void:
	_match_overlay = ColorRect.new()
	_match_overlay.name = "MatchOver"
	_match_overlay.color = Color(0.08, 0.08, 0.1, 0.86)
	_match_overlay.visible = false
	_match_overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
	_match_overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	_match_overlay.z_index = 50
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
	_commence_button.custom_minimum_size = Vector2(240, 44)
	_commence_button.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	_commence_button.offset_bottom = -24.0
	_commence_button.offset_top = -68.0
	_commence_button.pressed.connect(_on_commence_battle)
	_style_button(_commence_button)
	_ui_layer.add_child(_commence_button)


func _on_window_resized() -> void:
	var screen_size := get_viewport_rect().size
	
	# Update panel size
	if _header_panel != null:
		_header_panel.size = screen_size
	
	# Update title position
	if _title_label != null:
		_title_label.position = Vector2(screen_size.x / 2.0 - 70.0, 14.0)
	
	# Update turn position
	if _turn_label != null:
		_turn_label.position = Vector2(screen_size.x / 2.0 - 50.0, 50.0)
	
	# Update debug label position
	var debug_label := _header_panel.get_node_or_null("DebugLabel")
	if debug_label != null:
		debug_label.position = Vector2(screen_size.x - 150.0, 10.0)
	
	# Update enemy team label position
	if _enemy_team_label != null:
		_enemy_team_label.position = Vector2(screen_size.x - 240.0, 12.0)
	
	# Update enemy team list position
	if _enemy_team_list != null:
		_enemy_team_list.position = Vector2(screen_size.x - 240.0, 50.0)
	
	# Update banned label position
	var banned_label := _header_panel.get_node_or_null("BannedLabel")
	if banned_label != null:
		banned_label.position = Vector2(screen_size.x / 2.0 - 100.0, 220.0)
	
	# Update banned list position
	if _banned_list != null:
		_banned_list.position = Vector2(screen_size.x / 2.0 - 100.0, 250.0)
	
	# Update action buttons with proportional sizing
	_update_action_buttons(screen_size)
	
	# Update champion grid position
	if _champion_grid != null:
		_champion_grid.position = Vector2(20.0, 310.0)
	
	# Update role filter buttons with new sizes
	_update_role_filter_buttons(screen_size)
	
	# Update champion buttons in place instead of recreating them
	if _game_state == DRAFTING:
		_update_champion_buttons_in_place(screen_size)
	
	# Force redraw to clear visual artifacts
	if _header_panel != null:
		_header_panel.queue_redraw()
	if _ui_layer != null:
		_ui_layer.queue_redraw()
	if (_game_state == COMBAT or _game_state == PREPARATION) and _backend != null:
		# Do not re-apply tick_fx (floaters / melee VFX) — same snapshot, layout-only.
		_update_visualization_from_snapshot(false)


func _update_champion_buttons_in_place(screen_size: Vector2) -> void:
	var start_y_ratio := 0.41  # 41% of screen height
	var square_size_ratio := 0.10  # 10% of screen width
	var square_margin_ratio := 0.015  # 1.5% of screen width
	var start_x_ratio := 0.025  # 2.5% of screen width

	var square_size := screen_size.x * square_size_ratio
	var square_margin := screen_size.x * square_margin_ratio
	var cols: int = max(1, int((screen_size.x - screen_size.x * start_x_ratio * 2) / (square_size + square_margin)))

	_remove_champion_buttons_from_header()

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	var visible_heroes: Array[StringName] = []

	# Filter by role
	for champion_id in champion_ids:
		if _active_role_filters.is_empty():
			visible_heroes.append(champion_id)
		else:
			var champion: Variant = ChampionCatalogScript.get_champion(champion_id)
			if champion != null:
				var champion_dict: Dictionary = champion.to_dict()
				var stats_dict: Dictionary = champion_dict.get("stats", {})
				var role: StringName = StringName(stats_dict.get("role", ""))
				if _active_role_filters.has(role):
					visible_heroes.append(champion_id)


	# Sort by name
	visible_heroes.sort_custom(func(a, b): return String(a) < String(b))

	# Create buttons with proportional sizing (manual positioning)
	for i in range(visible_heroes.size()):
		var champion_id: StringName = visible_heroes[i]
		var champion: Variant = ChampionCatalogScript.get_champion(champion_id)
		if champion == null:
			continue
		
		var champion_dict: Dictionary = champion.to_dict()
		var stats_dict: Dictionary = champion_dict.get("stats", {})
		var role: StringName = StringName(stats_dict.get("role", ""))
		var is_taken: bool = champion_id in _player_picks or champion_id in _enemy_picks or champion_id in _banned_heroes

		var row := i / cols
		var col := i % cols
		var button_size := Vector2(square_size, square_size)
		var button_position := Vector2(screen_size.x * (start_x_ratio + float(col) * (square_size_ratio + square_margin_ratio)), screen_size.y * (start_y_ratio + float(row) * (square_size_ratio * screen_size.x / screen_size.y + square_margin_ratio * screen_size.x / screen_size.y)))

		var button_name = "Champion_" + String(champion_id)
		var new_size = Vector2(square_size, square_size)
		var new_position = Vector2(screen_size.x * (start_x_ratio + float(col) * (square_size_ratio + square_margin_ratio)), screen_size.y * (start_y_ratio + float(row) * (square_size_ratio * screen_size.x / screen_size.y + square_margin_ratio * screen_size.x / screen_size.y)))

		var button := Button.new()
		button.name = button_name
		button.text = String(stats_dict.get("name", String(champion_id)))
		button.size = new_size
		button.position = new_position
		var role_color: Color = ROLE_COLORS.get(String(role), COLOR_BUTTON)
		_style_champion_button(button, role_color, champion_id, is_taken)
		button.tooltip_text = _champion_tooltip_lines(champion_id)
		button.pressed.connect(_on_champion_clicked.bind(champion_id))
		_header_panel.add_child(button)

	# Force redraw after creating new buttons
	_header_panel.queue_redraw()


func _remove_champion_buttons_from_header() -> void:
	var to_clear: Array[Node] = []
	for child in _header_panel.get_children():
		if child is Button and (child as Button).name.begins_with("Champion"):
			to_clear.append(child)
	for n: Node in to_clear:
		if is_instance_valid(n) and n.get_parent() == _header_panel:
			_header_panel.remove_child(n)
			n.free()


func _update_action_buttons(screen_size: Vector2) -> void:
	var button_w_ratio := 0.26  # 26% of screen width
	var button_h_ratio := 0.07  # 7% of screen height
	var button_y1_ratio := 0.17  # 17% of screen height
	var button_y2_ratio := 0.24  # 24% of screen height

	if _random_draft_button != null:
		var button_size := Vector2(screen_size.x * button_w_ratio, screen_size.y * button_h_ratio)
		_random_draft_button.size = button_size
		_random_draft_button.position = Vector2(screen_size.x / 2.0 - button_size.x / 2.0, screen_size.y * button_y1_ratio)
	if _start_match_button != null:
		var button_size := Vector2(screen_size.x * button_w_ratio, screen_size.y * button_h_ratio)
		_start_match_button.size = button_size
		_start_match_button.position = Vector2(screen_size.x / 2.0 - button_size.x / 2.0, screen_size.y * button_y2_ratio)


func _update_role_filter_buttons(screen_size: Vector2) -> void:
	var roles: Array[StringName] = [&"tank", &"fighter", &"assassin", &"marksman", &"mage", &"support"]
	var filter_w_ratio := 0.15  # 15% of screen width
	var filter_h_ratio := 0.06  # 6% of screen height
	var filter_spacing_ratio := 0.02  # 2% of screen width
	var filter_start_x_ratio := 0.02  # 2% of screen width
	var filter_start_y_ratio := 0.33  # 33% of screen height

	for i in range(roles.size()):
		var role: StringName = roles[i]
		var filter_button := _header_panel.get_node_or_null("RoleFilter_" + String(role))
		if filter_button != null:
			var new_size := Vector2(screen_size.x * filter_w_ratio, screen_size.y * filter_h_ratio)
			filter_button.size = new_size
			filter_button.position = Vector2(screen_size.x * (filter_start_x_ratio + float(i) * (filter_w_ratio + filter_spacing_ratio)), screen_size.y * filter_start_y_ratio)


func _apply_color_scheme() -> void:
	if _header_panel != null:
		# Don't use modulate on panel - it washes out children
		_header_panel.add_theme_stylebox_override("panel", StyleBoxFlat.new())
		var style: StyleBoxFlat = _header_panel.get_theme_stylebox("panel")
		if style != null:
			style.bg_color = COLOR_PANEL
	if _title_label != null:
		_title_label.add_theme_color_override("font_color", COLOR_TEXT)
	if _turn_label != null:
		_turn_label.add_theme_color_override("font_color", COLOR_WARNING)
	if _player_team_label != null:
		_player_team_label.add_theme_color_override("font_color", COLOR_PLAYER)
	if _enemy_team_label != null:
		_enemy_team_label.add_theme_color_override("font_color", COLOR_ENEMY)


func _process(delta: float) -> void:
	if _game_state == COMBAT and not _combat_paused:
		_step_simulation(delta)
	if _arena_layer != null and (_game_state == COMBAT or _game_state == PREPARATION):
		_arena_layer.refresh()


func _step_simulation(delta: float) -> void:
	if _backend == null:
		return
	
	_sim_time_accumulator += delta * _sim_speed
	var tick_rate: float = SimConstantsScript.DEFAULT_TICK_RATE
	
	while _sim_time_accumulator >= tick_rate:
		_backend.advance_one_tick()
		_sim_time_accumulator -= tick_rate
		_update_visualization_from_snapshot()
		
		if _backend.match_ticks_exhausted():
			_end_match()
			break


func _update_visualization_from_snapshot(apply_tick_fx: bool = true) -> void:
	var snapshot: Dictionary = _backend.get_tick_snapshot()
	if snapshot.is_empty():
		return
	_refresh_combat_hud(snapshot)
	var units: Array = snapshot.get("units", [])
	for unit_data in units:
		if unit_data is not Dictionary:
			continue
		var unit_dict: Dictionary = Dictionary(unit_data)
		_update_unit_node(unit_dict)
	_sync_target_lines_from_snapshot(units)
	# Selection rings
	for uid in _unit_nodes.keys():
		var n: Node2D = _unit_nodes[uid] as Node2D
		if n != null and n.has_method("set_selected"):
			n.set_selected(int(uid) == _selected_unit_id)
	var projectiles: Array = snapshot.get("projectiles", [])
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
	unit_node.position = world_to_screen(px, py)
	if unit_node.has_method("set_snapshot_data"):
		unit_node.set_snapshot_data(unit_dict)
	unit_node.visible = _is_unit_on_battlefield(unit_dict)


func _is_unit_on_battlefield(u: Dictionary) -> bool:
	if _game_state != COMBAT and _game_state != PREPARATION:
		return true
	if str(u.get("state", "")) == "DEAD":
		return false
	if not bool(u.get("alive", true)):
		return false
	return true


func _sync_target_lines_from_snapshot(units: Array) -> void:
	var by_id: Dictionary = {}
	for unit_data in units:
		if unit_data is not Dictionary:
			continue
		var ud: Dictionary = Dictionary(unit_data)
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
		if tid != 0 and _game_state == COMBAT:
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
	target_line.default_color = COLOR_TARGET_LINE
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

	# Create new projectiles
	for proj_data in projectiles:
		var proj_dict: Dictionary = Dictionary(proj_data)
		var proj_id: int = int(proj_dict.get("id", 0))
		var pos_x: float = float(proj_dict.get("pos_x", 0.0))
		var pos_y: float = float(proj_dict.get("pos_y", 0.0))
		var team_s: StringName = StringName(proj_dict.get("team", "player"))
		var proj_node := _create_projectile_node(proj_id, pos_x, pos_y, team_s)
		_projectile_nodes[proj_id] = proj_node
		_world_layer.add_child(proj_node)


func _projectile_screen_radius_px() -> float:
	var vp: Vector2 = get_viewport_rect().size
	return clampf(
		PROJECTILE_BASE_RADIUS_PX * minf(vp.x, vp.y) / VIEWER_BASE_MIN_AXIS,
		2.0,
		7.0
	)


func _create_projectile_node(proj_id: int, pos_x: float, pos_y: float, team: StringName) -> Node2D:
	var proj_node := Node2D.new()
	proj_node.name = "Projectile_%d" % proj_id
	proj_node.position = world_to_screen(pos_x, pos_y)
	var col: Color = COLOR_PLAYER if team == &"player" else COLOR_ENEMY
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
	core.color = Color(0.95, 0.9, 0.45, 0.95)
	proj_node.add_child(core)
	return proj_node


func _refresh_combat_hud(snapshot: Dictionary) -> void:
	if _lbl_timer != null:
		_lbl_timer.text = "TIME: %.1fs" % float(snapshot.get("time_remaining", 0.0))
	if _lbl_score != null:
		_lbl_score.text = "SCORE: %d - %d" % [int(snapshot.get("player_kills", 0)), int(snapshot.get("enemy_kills", 0))]
	if _lbl_combat_state != null and _game_state != PREPARATION:
		_lbl_combat_state.text = "COMBAT" if _game_state == COMBAT else String(_game_state)
	if _hud_pause != null:
		_hud_pause.visible = _combat_paused and _game_state == COMBAT
		if _hud_pause.visible:
			_hud_pause.text = "PAUSED (Space)  |  K: speed"


func _refresh_side_rosters_from_snapshot(units: Array) -> void:
	if _p1_roster_labels == null or _p2_roster_labels == null:
		return
	for c in _p1_roster_labels.get_children():
		c.queue_free()
	for c in _p2_roster_labels.get_children():
		c.queue_free()
	var p1: Array = []
	var p2: Array = []
	for u in units:
		if u is Dictionary:
			var ud: Dictionary = u
			if String(ud.get("team", "")) == "player":
				p1.append(ud)
			elif String(ud.get("team", "")) == "enemy":
				p2.append(ud)
	p1.sort_custom(func(a, b): return int(a.get("instance_id", 0)) < int(b.get("instance_id", 0)))
	p2.sort_custom(func(a, b): return int(a.get("instance_id", 0)) < int(b.get("instance_id", 0)))
	for ud in p1:
		var lb := Label.new()
		var nm: String = str(ud.get("archetype_id", "?"))
		var k: int = int(ud.get("kills", 0))
		var d: int = int(ud.get("deaths", 0))
		var a: int = int(ud.get("assists", 0))
		var st: String = str(ud.get("state", "ALIVE"))
		var s: String = "%s  (%d/%d/%d)" % [nm, k, d, a]
		if st == "DEAD" or not bool(ud.get("alive", true)):
			s += "  %.1fs" % float(ud.get("respawn_timer", 0.0))
			lb.add_theme_color_override("font_color", COLOR_SUBTLE)
		else:
			lb.add_theme_color_override("font_color", COLOR_PLAYER)
		lb.text = s
		_p1_roster_labels.add_child(lb)
	for ud in p2:
		var lb2 := Label.new()
		var nm2: String = str(ud.get("archetype_id", "?"))
		var k2: int = int(ud.get("kills", 0))
		var d2: int = int(ud.get("deaths", 0))
		var a2: int = int(ud.get("assists", 0))
		var st2: String = str(ud.get("state", "ALIVE"))
		var s2: String = "(%d/%d/%d)  %s" % [k2, d2, a2, nm2]
		if st2 == "DEAD" or not bool(ud.get("alive", true)):
			s2 = "%.1fs  " % float(ud.get("respawn_timer", 0.0)) + s2
			lb2.add_theme_color_override("font_color", COLOR_SUBTLE)
		else:
			lb2.add_theme_color_override("font_color", COLOR_ENEMY)
		lb2.text = s2
		_p2_roster_labels.add_child(lb2)


func _apply_tick_fx(snapshot: Dictionary) -> void:
	var evs: Array = Array(snapshot.get("tick_fx", []))
	var aoe_world_centers: Array = []  # Vector2, world (x,y) for dedupe; only "real" radii (else infer is blocked with no draw)
	for e0 in evs:
		if e0 is not Dictionary:
			continue
		var d0: Dictionary = e0
		var k0: String = str(d0.get("kind", ""))
		if k0.begins_with("aoe_"):
			var r0: float = float(d0.get("r", d0.get("radius", 0.0)))
			if r0 <= 0.0 and k0 == "aoe_splash":
				r0 = SimConstantsScript.VIEWER_AOE_FALLBACK_SPLASH_RADIUS_WORLD
			if r0 > 0.0:
				aoe_world_centers.append(Vector2(float(d0.get("x", 0.0)), float(d0.get("y", 0.0))))
	for e in evs:
		if e is not Dictionary:
			continue
		var d: Dictionary = e
		var k: String = str(d.get("kind", ""))
		var wx: float = float(d.get("x", 0.0))
		var wy: float = float(d.get("y", 0.0))
		var sp: Vector2 = world_to_screen(wx, wy)
		if k == "damage" or k == "dmg":
			var amt: float = float(d.get("val", 0.0))
			_spawn_floating_text_screen("-%d" % int(ceil(amt)), sp, Color(0.9, 0.45, 0.45))
			# Infer splash/impact ring for ranged autos if native aoe_ events are absent (e.g. older DLL) or r missing.
			var src_id: int = int(d.get("src_id", 0))
			if src_id > 0 and not _aoe_world_position_has_fx(aoe_world_centers, wx, wy):
				var u_src: Dictionary = _find_unit_in_snapshot_by_id(snapshot, src_id)
				if not u_src.is_empty() and float(u_src.get("attack_range", 0.0)) > SimConstantsScript.RANGED_THRESHOLD:
					_spawn_aoe_ring_fx(
						wx,
						wy,
						SimConstantsScript.VIEWER_AOE_FALLBACK_SPLASH_RADIUS_WORLD,
						"aoe_splash",
						src_id,
						snapshot
					)
		elif k == "heal":
			var h: float = float(d.get("val", 0.0))
			_spawn_floating_text_screen("+%d" % int(ceil(h)), sp, COLOR_SUCCESS)
		elif k == "shield":
			var sh: float = float(d.get("val", 0.0))
			_spawn_floating_text_screen("[%d]" % int(ceil(sh)), sp, Color(0.55, 0.75, 1.0))
		elif k == "melee_slash":
			_spawn_melee_slash_fx(sp)
		elif k.begins_with("aoe_"):
			var r_w: float = float(d.get("r", d.get("radius", 0.0)))
			if r_w <= 0.0 and k == "aoe_splash":
				r_w = SimConstantsScript.VIEWER_AOE_FALLBACK_SPLASH_RADIUS_WORLD
			if r_w > 0.0:
				_spawn_aoe_ring_fx(wx, wy, r_w, k, int(d.get("src_id", 0)), snapshot)


func _find_unit_in_snapshot_by_id(snapshot: Dictionary, instance_id: int) -> Dictionary:
	if instance_id == 0:
		return {}
	for u in snapshot.get("units", []):
		if u is Dictionary and int((u as Dictionary).get("instance_id", 0)) == instance_id:
			return u
	return {}


func _aoe_world_position_has_fx(centers: Array, wx: float, wy: float) -> bool:
	## Same tick may emit damage and aoe at the same (x,y); avoid double ring.
	var e_w: float = 0.12
	for p in centers:
		if p is Vector2 and absf(p.x - wx) < e_w and absf(p.y - wy) < e_w:
			return true
	return false


func _aoe_ring_color_for(kind: String, src_id: int, snapshot: Dictionary) -> Color:
	var team_s: String = ""
	if src_id != 0:
		for u in snapshot.get("units", []):
			if u is Dictionary and int((u as Dictionary).get("instance_id", 0)) == src_id:
				team_s = str((u as Dictionary).get("team", ""))
				break
	if kind == "aoe_taunt":
		return Color(0.72, 0.42, 0.95, 0.62)
	if kind == "aoe_splash":
		return Color(1.0, 0.78, 0.2, 0.75)
	# aoe_damage: tint by team
	if team_s == "player":
		return Color(0.35, 0.55, 1.0, 0.58)
	if team_s == "enemy":
		return Color(1.0, 0.4, 0.35, 0.58)
	return Color(0.95, 0.5, 0.35, 0.5)


## World-space [wx,wy], radius in world units; center matches sim AoE center.
func _spawn_aoe_ring_fx(wx: float, wy: float, world_r: float, kind: String, src_id: int, snapshot: Dictionary) -> void:
	if world_r <= 0.0:
		return
	var sp: Vector2 = world_to_screen(wx, wy)
	var vp: Vector2 = get_viewport_rect().size
	var rpx: float = world_r * (vp.x / SimConstantsScript.WORLD_SIZE)
	if rpx < 1.5:
		return
	var col: Color = _aoe_ring_color_for(kind, src_id, snapshot)
	# Slight screen-space oval so the ring matches non-square viewports the same as world_to_screen.
	var rpx_y: float = world_r * (vp.y / SimConstantsScript.WORLD_SIZE)
	var n: Node2D = AoeRingNodeScript.new()
	n.name = "AoeRing"
	n.position = sp
	var fill_c := Color(col.r, col.g, col.b, 0.22 * col.a)
	n.setup(rpx, rpx_y, fill_c, col, 3.0)
	if _aoe_fx_layer != null and is_instance_valid(_aoe_fx_layer):
		_aoe_fx_layer.add_child(n)
	else:
		n.z_index = 20
		_world_layer.add_child(n)
	var dur: float = SimConstantsScript.AOE_VISUAL_MAX_DURATION
	n.modulate = Color(1, 1, 1, 0.92)
	var tw := create_tween()
	tw.tween_property(n, "modulate:a", 0.0, dur)
	tw.tween_callback(n.queue_free)


func _spawn_melee_slash_fx(screen_pos: Vector2) -> void:
	var s := Node2D.new()
	s.position = screen_pos
	s.z_index = 5
	var line := Line2D.new()
	line.add_point(Vector2(-10, -10))
	line.add_point(Vector2(10, 10))
	line.width = 2.0
	line.default_color = Color(1.0, 0.9, 0.45, 0.85)
	s.add_child(line)
	_world_layer.add_child(s)
	var tw := create_tween()
	tw.tween_property(line, "modulate:a", 0.0, 0.1)
	tw.tween_callback(s.queue_free)


func _spawn_floating_text_screen(text: String, screen_pos: Vector2, color: Color) -> void:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", 16)
	label.add_theme_color_override("font_color", color)
	label.z_index = 200
	label.position = screen_pos
	_ui_layer.add_child(label)
	_floating_texts.append(label)
	var tween := create_tween()
	tween.parallel().tween_property(label, "position", screen_pos + Vector2(0, -40), 0.8)
	tween.parallel().tween_property(label, "modulate:a", 0.0, 0.8)
	tween.tween_callback(label.queue_free)


func _remove_floating_text(label: Label) -> void:
	_floating_texts.erase(label)


func _setup_draft_ui() -> void:
	var screen_size := get_viewport_rect().size

	var rf: Array[Node] = []
	for child in _header_panel.get_children():
		if child is Button and (child as Button).name.begins_with("RoleFilter"):
			rf.append(child)
	for n: Node in rf:
		if is_instance_valid(n) and n.get_parent() == _header_panel:
			_header_panel.remove_child(n)
			n.free()

	# Set up role filter buttons with proportional sizing (manual positioning)
	var roles: Array[StringName] = [&"tank", &"fighter", &"assassin", &"marksman", &"mage", &"support"]
	var filter_w_ratio := 0.15  # 15% of screen width
	var filter_h_ratio := 0.06  # 6% of screen height
	var filter_spacing_ratio := 0.02  # 2% of screen width
	var filter_start_x_ratio := 0.02  # 2% of screen width
	var filter_start_y_ratio := 0.33  # 33% of screen height

	for i in range(roles.size()):
		var role: StringName = roles[i]
		var filter_button := Button.new()
		filter_button.name = "RoleFilter_" + String(role)
		filter_button.text = String(role).to_upper()
		var button_size := Vector2(screen_size.x * filter_w_ratio, screen_size.y * filter_h_ratio)
		filter_button.size = button_size
		filter_button.position = Vector2(screen_size.x * (filter_start_x_ratio + float(i) * (filter_w_ratio + filter_spacing_ratio)), screen_size.y * filter_start_y_ratio)
		filter_button.pressed.connect(_on_role_filter_toggled.bind(role, filter_button))
		_header_panel.add_child(filter_button)
		# Don't add to active filters initially - all roles should be visible (see all)
		_update_role_filter_button_style(filter_button, role, false)

	# Connect action buttons
	if _random_draft_button != null:
		_random_draft_button.pressed.connect(_on_random_draft_clicked)
	if _start_match_button != null:
		_start_match_button.pressed.connect(_on_start_match_clicked)

	# Populate champion grid with proportional sizing
	_populate_champion_grid()


func _populate_champion_grid() -> void:
	var screen_size := get_viewport_rect().size
	var start_y_ratio := 0.41  # 41% of screen height
	var square_size_ratio := 0.10  # 10% of screen width
	var square_margin_ratio := 0.015  # 1.5% of screen width
	var start_x_ratio := 0.025  # 2.5% of screen width

	var square_size := screen_size.x * square_size_ratio
	var square_margin := screen_size.x * square_margin_ratio
	var cols: int = max(1, int((screen_size.x - screen_size.x * start_x_ratio * 2) / (square_size + square_margin)))

	_remove_champion_buttons_from_header()

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	var visible_heroes: Array[StringName] = []

	# Filter by role
	for champion_id in champion_ids:
		if _active_role_filters.is_empty():
			visible_heroes.append(champion_id)
		else:
			var champion: Variant = ChampionCatalogScript.get_champion(champion_id)
			if champion != null:
				var champion_dict: Dictionary = champion.to_dict()
				var stats_dict: Dictionary = champion_dict.get("stats", {})
				var role: StringName = StringName(stats_dict.get("role", ""))
				if _active_role_filters.has(role):
					visible_heroes.append(champion_id)

	# Sort by name
	visible_heroes.sort_custom(func(a, b): return String(a) < String(b))

	# Create buttons with proportional sizing (manual positioning)
	for i in range(visible_heroes.size()):
		var champion_id: StringName = visible_heroes[i]
		var champion: Variant = ChampionCatalogScript.get_champion(champion_id)
		if champion == null:
			continue
		
		var champion_dict: Dictionary = champion.to_dict()
		var stats_dict: Dictionary = champion_dict.get("stats", {})
		var role: StringName = StringName(stats_dict.get("role", ""))

		var is_taken: bool = champion_id in _player_picks or champion_id in _enemy_picks or champion_id in _banned_heroes

		var row := i / cols
		var col := i % cols
		var button := Button.new()
		button.name = "Champion_" + String(champion_id)
		button.text = String(stats_dict.get("name", String(champion_id)))
		button.custom_minimum_size = Vector2(square_size, square_size)
		button.position = Vector2(screen_size.x * (start_x_ratio + float(col) * (square_size_ratio + square_margin_ratio)), screen_size.y * (start_y_ratio + float(row) * (square_size_ratio * screen_size.x / screen_size.y + square_margin_ratio * screen_size.x / screen_size.y)))
		var role_color: Color = ROLE_COLORS.get(String(role), COLOR_BUTTON)
		_style_champion_button(button, role_color, champion_id, is_taken)
		button.tooltip_text = _champion_tooltip_lines(champion_id)
		button.pressed.connect(_on_champion_clicked.bind(champion_id))
		_header_panel.add_child(button)

	# Force redraw after creating new buttons
	_header_panel.queue_redraw()


func _on_role_filter_toggled(role: StringName, button: Button) -> void:
	# If the clicked role is already selected, deselect it (go back to see all)
	if _active_role_filters.has(role):
		_active_role_filters.clear()
	else:
		# Clear any existing filter and set only the clicked role
		_active_role_filters.clear()
		_active_role_filters.append(role)
	
	# Update all role filter button styles
	for child in _header_panel.get_children():
		if child is Button and child.name.begins_with("RoleFilter"):
			var button_role := String(child.name.replace("RoleFilter_", ""))
			var is_active := StringName(button_role) in _active_role_filters
			_update_role_filter_button_style(child, StringName(button_role), is_active)

	# Fresh pull and redraw of champion UI buttons
	_populate_champion_grid()


func _update_role_filter_button_style(button: Button, role: StringName, is_active: bool) -> void:
	var role_color: Color = ROLE_COLORS.get(String(role), COLOR_BUTTON)
	
	# Reset modulate to white to avoid affecting StyleBoxFlat
	button.modulate = Color.WHITE
	
	if is_active or _active_role_filters.is_empty():
		# Bright when this specific filter is active OR all filters are empty (see all)
		var style_box := StyleBoxFlat.new()
		style_box.bg_color = role_color
		button.add_theme_stylebox_override("normal", style_box)
		button.add_theme_stylebox_override("hover", style_box)
		button.add_theme_stylebox_override("pressed", style_box)
	else:
		# Dimmed when this specific filter is inactive (others are active)
		var dimmed_color := Color(
			max(0.0, role_color.r * 0.5),
			max(0.0, role_color.g * 0.5),
			max(0.0, role_color.b * 0.5)
		)
		var style_box := StyleBoxFlat.new()
		style_box.bg_color = dimmed_color
		button.add_theme_stylebox_override("normal", style_box)
		button.add_theme_stylebox_override("hover", style_box)
		button.add_theme_stylebox_override("pressed", style_box)


func _style_champion_button(button: Button, role_color: Color, champion_id: StringName, is_taken: bool) -> void:
	# Reset modulate to white to avoid affecting StyleBoxFlat
	button.modulate = Color.WHITE
	
	if is_taken:
		# Dim the role color for the fill
		var dimmed_color := Color(
			max(0.0, role_color.r * 0.5),
			max(0.0, role_color.g * 0.5),
			max(0.0, role_color.b * 0.5)
		)
		
		# Add border highlight based on team (independent of role color)
		var border_color := COLOR_PLAYER if champion_id in _player_picks else COLOR_ENEMY
		var style_box := StyleBoxFlat.new()
		style_box.bg_color = dimmed_color  # Use dimmed role color as fill
		style_box.border_width_left = 3
		style_box.border_width_top = 3
		style_box.border_width_right = 3
		style_box.border_width_bottom = 3
		style_box.border_color = border_color  # Team color for border
		button.add_theme_stylebox_override("normal", style_box)
		button.add_theme_stylebox_override("hover", style_box)
		button.add_theme_stylebox_override("pressed", style_box)
		button.add_theme_stylebox_override("disabled", style_box)
		button.disabled = true
	else:
		# Apply full role color for available champions (bright default)
		var style_box := StyleBoxFlat.new()
		style_box.bg_color = role_color
		button.add_theme_stylebox_override("normal", style_box)
		button.add_theme_stylebox_override("hover", style_box)
		button.add_theme_stylebox_override("pressed", style_box)
		button.add_theme_stylebox_override("disabled", style_box)
		button.modulate = Color.WHITE
		button.disabled = _draft_step_index >= DRAFT_SEQUENCE.size()


func _update_turn_display() -> void:
	if _turn_label == null:
		return

	var screen_size := get_viewport_rect().size
	var center_x := screen_size.x / 2.0

	if _draft_step_index < DRAFT_SEQUENCE.size():
		var current_turn: String = DRAFT_SEQUENCE[_draft_step_index]
		_turn_label.text = "TURN: %s" % current_turn.replace("_", " ")
		# Center the turn label horizontally
		_turn_label.position = Vector2(center_x - _turn_label.get_size().x / 2.0, 50.0)
	else:
		_turn_label.text = "TURN: Draft Complete"
		# Center the turn label horizontally
		_turn_label.position = Vector2(center_x - _turn_label.get_size().x / 2.0, 50.0)


func _update_team_rosters() -> void:
	for child in _player_team_list.get_children():
		_player_team_list.remove_child(child)
		child.free()
	for child in _enemy_team_list.get_children():
		_enemy_team_list.remove_child(child)
		child.free()
	if _banned_list != null:
		for child in _banned_list.get_children():
			_banned_list.remove_child(child)
			child.free()

	_player_team_list.add_theme_constant_override("separation", 2)
	_enemy_team_list.add_theme_constant_override("separation", 2)
	if _banned_list != null:
		_banned_list.add_theme_constant_override("separation", 2)

	# VBoxContainer lays out; do not set per-label y (was stacking with flow + races from await).
	for i in range(_player_picks.size()):
		var champion_id: StringName = _player_picks[i]
		var label := Label.new()
		label.text = "- %s" % String(champion_id).capitalize()
		label.add_theme_color_override("font_color", COLOR_TEXT)
		_player_team_list.add_child(label)

	for i in range(_enemy_picks.size()):
		var champion_id: StringName = _enemy_picks[i]
		var label2 := Label.new()
		label2.text = "- %s" % String(champion_id).capitalize()
		label2.add_theme_color_override("font_color", COLOR_TEXT)
		_enemy_team_list.add_child(label2)

	if _banned_list != null:
		var sorted_banned: Array[StringName] = _banned_heroes.duplicate()
		sorted_banned.sort_custom(func(a, b): return String(a) < String(b))
		for j in range(sorted_banned.size()):
			var b_id: StringName = sorted_banned[j]
			var bl := Label.new()
			bl.text = String(b_id)
			bl.add_theme_color_override("font_color", COLOR_SUBTLE)
			_banned_list.add_child(bl)


func _update_start_match_enabled() -> void:
	if _start_match_button == null:
		return
	var can: bool = _draft_step_index >= DRAFT_SEQUENCE.size() or _debug_mode
	_start_match_button.disabled = not can


func _power_score_for_pick(hero_id: StringName) -> float:
	var ch: Variant = ChampionCatalogScript.get_champion(hero_id)
	if ch == null:
		return 0.0
	var d: Dictionary = ch.to_dict()
	var st: Dictionary = d.get("stats", {})
	var max_hp: float = float(st.get("max_hp", 1.0))
	var ad: float = float(st.get("attack_damage", 0.0))
	var as_sp: float = maxf(0.01, float(st.get("attack_speed", 0.5)))
	return max_hp * ad * as_sp


func _try_enemy_draft_ai() -> void:
	while _draft_step_index < DRAFT_SEQUENCE.size():
		var step: String = DRAFT_SEQUENCE[_draft_step_index]
		if not step.begins_with("P2"):
			break
		if step.contains("BAN"):
			break
		var taken: Array[StringName] = _player_picks + _enemy_picks + _banned_heroes
		var ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
		var available: Array[StringName] = []
		for cid in ids:
			if cid not in taken:
				available.append(cid)
		if available.is_empty():
			break
		var chosen: StringName = _pick_p2_hero(available)
		if chosen == StringName():
			break
		if _enemy_picks.size() < MAX_TEAM_SIZE:
			_enemy_picks.append(chosen)
		_draft_step_index += 1
		_update_turn_display()
		_update_team_rosters()
		_update_champion_button_style(chosen)
	_update_start_match_enabled()


func _pick_p2_hero(available: Array[StringName]) -> StringName:
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


func _champion_tooltip_lines(hero_id: StringName) -> String:
	var ch: Variant = ChampionCatalogScript.get_champion(hero_id)
	if ch == null:
		return ""
	var d: Dictionary = ch.to_dict()
	var st: Dictionary = d.get("stats", {})
	var name: String = str(st.get("name", hero_id))
	var role: String = str(st.get("role", "")).to_upper()
	var lines: PackedStringArray = PackedStringArray()
	lines.append("%s (%s)" % [name, role])
	lines.append(str(d.get("description", "")))
	lines.append(
		"HP %.0f | AD %.1f | AS %.2f | Range %.1f"
		% [
			float(st.get("max_hp", 0.0)),
			float(st.get("attack_damage", 0.0)),
			float(st.get("attack_speed", 0.0)),
			float(st.get("attack_range", 0.0)),
		]
	)
	lines.append("Passive: %s" % str(d.get("passive_desc", "")))
	lines.append("Ability (%ss): %s" % [str(st.get("ability_cd", 0.0)), str(d.get("ability_desc", ""))])
	lines.append("Ultimate (%ss): %s" % [str(st.get("ultimate_cd", 0.0)), str(d.get("ultimate_desc", ""))])
	return "\n".join(lines)


func _on_random_draft_clicked() -> void:
	while _draft_step_index < DRAFT_SEQUENCE.size():
		var taken: Array[StringName] = _player_picks + _enemy_picks + _banned_heroes
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
	_enter_preparation()


func _setup_control_panel() -> void:
	if _control_panel == null:
		return

	var hbox := HBoxContainer.new()
	_control_panel.add_child(hbox)

	var pause_button := Button.new()
	pause_button.text = "Pause"
	pause_button.custom_minimum_size = Vector2(100, 40)
	pause_button.pressed.connect(_toggle_pause)
	_style_button(pause_button)
	hbox.add_child(pause_button)

	var restart_button := Button.new()
	restart_button.text = "Restart"
	restart_button.custom_minimum_size = Vector2(100, 40)
	restart_button.pressed.connect(_restart_match)
	_style_button(restart_button)
	hbox.add_child(restart_button)


func _style_button(button: Button) -> void:
	button.add_theme_color_override("font_color", COLOR_BUTTON_TEXT)
	button.add_theme_color_override("font_pressed_color", COLOR_BUTTON_TEXT)


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
			var archetype_id: StringName = StringName(unit_dict.get("archetype_id", ""))
			var ut: String = String(unit_dict.get("team", ""))
			var hp: float = float(unit_dict.get("hp", 0.0))
			var max_hp: float = float(unit_dict.get("max_hp", 0.0))
			var state: StringName = StringName(unit_dict.get("state", ""))
			var target_id: int = int(unit_dict.get("target_id", 0))

			label.text = "Unit: %s\nTeam: %s\nHP: %.1f/%.1f\nState: %s\nTarget: %d" % [archetype_id, ut, hp, max_hp, state, target_id]
			return

	label.text = "Unit %d not found in snapshot" % instance_id


func _on_champion_clicked(champion_id: StringName) -> void:
	if _game_state != DRAFTING:
		return

	if _draft_step_index >= DRAFT_SEQUENCE.size():
		return

	var current_turn: String = DRAFT_SEQUENCE[_draft_step_index]

	if current_turn == "P1_PICK":
		if _player_picks.size() < MAX_TEAM_SIZE:
			_player_picks.append(champion_id)
	elif current_turn == "P2_PICK":
		if _enemy_picks.size() < MAX_TEAM_SIZE:
			_enemy_picks.append(champion_id)

	_draft_step_index += 1
	_update_turn_display()
	_update_team_rosters()
	_update_champion_button_style(champion_id)
	_update_start_match_enabled()
	_try_enemy_draft_ai()



func _update_champion_button_style(champion_id: StringName) -> void:
	var button_name := "Champion_" + String(champion_id)
	var button: Button = _header_panel.get_node_or_null(button_name)
	if button == null:
		return
	
	# Get champion info for role color
	var champion: Variant = ChampionCatalogScript.get_champion(champion_id)
	if champion == null:
		return
	
	var champion_dict: Dictionary = champion.to_dict()
	var stats_dict: Dictionary = champion_dict.get("stats", {})
	var role: StringName = StringName(stats_dict.get("role", ""))
	var role_color: Color = ROLE_COLORS.get(String(role), COLOR_BUTTON)
	var is_taken: bool = champion_id in _player_picks or champion_id in _enemy_picks or champion_id in _banned_heroes
	
	# Reset modulate to white to avoid affecting StyleBoxFlat
	button.modulate = Color.WHITE
	
	if is_taken:
		# Dim the role color for the fill
		var dimmed_color := Color(
			max(0.0, role_color.r * 0.5),
			max(0.0, role_color.g * 0.5),
			max(0.0, role_color.b * 0.5)
		)
		
		# Add border highlight based on team (independent of role color)
		var border_color := COLOR_PLAYER if champion_id in _player_picks else COLOR_ENEMY
		var style_box := StyleBoxFlat.new()
		style_box.bg_color = dimmed_color  # Use dimmed role color as fill
		style_box.border_width_left = 3
		style_box.border_width_top = 3
		style_box.border_width_right = 3
		style_box.border_width_bottom = 3
		style_box.border_color = border_color  # Team color for border
		button.add_theme_stylebox_override("normal", style_box)
		button.add_theme_stylebox_override("hover", style_box)
		button.add_theme_stylebox_override("pressed", style_box)
		button.add_theme_stylebox_override("disabled", style_box)
		button.disabled = true
	else:
		# Apply role color for available champions using StyleBoxFlat
		var style_box := StyleBoxFlat.new()
		style_box.bg_color = role_color
		button.add_theme_stylebox_override("normal", style_box)
		button.add_theme_stylebox_override("hover", style_box)
		button.add_theme_stylebox_override("pressed", style_box)
		button.add_theme_stylebox_override("disabled", style_box)
		button.modulate = Color.WHITE
		button.disabled = _draft_step_index >= DRAFT_SEQUENCE.size()


func _toggle_pause() -> void:
	_combat_paused = not _combat_paused
	if _hud_pause != null:
		_hud_pause.visible = _combat_paused and _game_state == COMBAT
		if _hud_pause.visible:
			_hud_pause.text = "PAUSED (Space)  |  K: speed"


func _restart_match() -> void:
	if _match_overlay != null:
		_match_overlay.visible = false
	_reset_to_draft()


func _enter_preparation() -> void:
	if _backend == null:
		return
	if not _can_start_match():
		return
	var match_input: Object = MatchReplayInputScript.build_match_input(
		0,
		_player_picks,
		_enemy_picks,
		SimConstantsScript.DEFAULT_TICK_RATE
	)
	_backend.begin_match(match_input)
	_game_state = PREPARATION
	_sim_time_accumulator = 0.0
	_show_world_and_hud_for_battle()
	if _arena_layer != null:
		_arena_layer.show_preparation_zones = true
	if _commence_button != null:
		_commence_button.visible = true
	_lbl_combat_state.text = "PREPARATION"
	_hud_pause.visible = false
	_update_visualization_from_snapshot()


func _can_start_match() -> bool:
	if _player_picks.is_empty() or _enemy_picks.is_empty():
		return false
	if _draft_step_index < DRAFT_SEQUENCE.size() and not _debug_mode:
		return false
	return true


func _show_world_and_hud_for_battle() -> void:
	var screen_size := get_viewport_rect().size
	if _header_panel != null:
		_header_panel.visible = false
	if _role_filter_container != null:
		_role_filter_container.visible = false
	if _champion_grid != null:
		_champion_grid.visible = false
	if _random_draft_button != null:
		_random_draft_button.visible = false
	if _start_match_button != null:
		_start_match_button.visible = false
	if _world_layer != null:
		_world_layer.visible = true
	if _aoe_fx_layer != null:
		_aoe_fx_layer.visible = true
	if _control_panel != null:
		_control_panel.visible = true
		_control_panel.size = Vector2(screen_size.x, 56.0)
		_control_panel.position = Vector2(0.0, screen_size.y - 56.0)
	var top_hud: Control = _ui_layer.get_node_or_null("CombatHUD")
	if top_hud != null:
		top_hud.visible = true
	if _p1_roster_labels != null:
		_p1_roster_labels.get_parent().visible = true
	if _p2_roster_labels != null:
		_p2_roster_labels.get_parent().visible = true


func _on_commence_battle() -> void:
	if _game_state != PREPARATION:
		return
	if _commence_button != null:
		_commence_button.visible = false
	if _arena_layer != null:
		_arena_layer.show_preparation_zones = false
	_game_state = COMBAT
	_sim_time_accumulator = 0.0
	_lbl_combat_state.text = "COMBAT"


func _end_match() -> void:
	_game_state = MATCH_OVER
	var summary: Dictionary = _backend.finish_and_summarize()
	_combat_paused = false
	_show_match_results(summary)


func _show_match_results(summary: Dictionary) -> void:
	if _match_overlay == null or _match_title == null or _match_stats_container == null:
		return
	_match_overlay.visible = true
	var w: String = str(summary.get("winner_team", "draw"))
	_match_title.remove_theme_color_override("font_color")
	if w == "player":
		_match_title.text = "PLAYER 1 VICTORY"
		_match_title.add_theme_color_override("font_color", COLOR_PLAYER)
	elif w == "enemy":
		_match_title.text = "PLAYER 2 VICTORY"
		_match_title.add_theme_color_override("font_color", COLOR_ENEMY)
	else:
		_match_title.text = "MATCH DRAW"
		_match_title.add_theme_color_override("font_color", COLOR_TEXT)
	for c in _match_stats_container.get_children():
		c.queue_free()
	var pi: int = int(summary.get("player_kills", 0))
	var ei: int = int(summary.get("enemy_kills", 0))
	# If summary has no kills at root, 0-0
	var sc_line := Label.new()
	sc_line.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	sc_line.text = "Final score (kills): %d - %d" % [pi, ei]
	_match_stats_container.add_child(sc_line)
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
			String(u.get("archetype", u.get("archetype_id", "?"))),
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
	_reset_to_draft()


func _reset_to_draft() -> void:
	_clear_units()
	_game_state = DRAFTING
	_combat_paused = false
	_draft_step_index = 0
	_player_picks.clear()
	_enemy_picks.clear()
	_banned_heroes.clear()
	_sim_time_accumulator = 0.0
	_selected_unit_id = 0
	if _commence_button != null:
		_commence_button.visible = false
	if _arena_layer != null:
		_arena_layer.show_preparation_zones = false
	if _header_panel != null:
		_header_panel.visible = true
	if _role_filter_container != null:
		_role_filter_container.visible = true
	if _champion_grid != null:
		_champion_grid.visible = true
	if _random_draft_button != null:
		_random_draft_button.visible = true
	if _start_match_button != null:
		_start_match_button.visible = true
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
	_update_turn_display()
	_update_team_rosters()
	_populate_champion_grid()
	_update_start_match_enabled()
	if _inspection_panel != null:
		_inspection_panel.visible = false


func _clear_units() -> void:
	if _aoe_fx_layer != null:
		for c in _aoe_fx_layer.get_children():
			if is_instance_valid(c):
				c.queue_free()
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
