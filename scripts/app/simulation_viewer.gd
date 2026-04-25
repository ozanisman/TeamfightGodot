extends Control

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

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

const ROLE_COLORS: Dictionary = {
	"tank": Color(0.275, 0.51, 0.706),
	"fighter": Color(0.824, 0.412, 0.118),
	"assassin": Color(0.6, 0.196, 0.8),
	"marksman": Color(0.133, 0.545, 0.133),
	"mage": Color(0.0, 0.749, 1.0),
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

	print("Simulation Viewer ready. Game state: ", _game_state)


func _create_ui_structure() -> void:
	var screen_size := get_viewport_rect().size

	# Create WorldLayer for combat visualization (behind UI)
	_world_layer = Node2D.new()
	_world_layer.name = "WorldLayer"
	_world_layer.position = Vector2(0.0, 0.0)
	_world_layer.visible = false
	add_child(_world_layer)

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
	_control_panel.position = Vector2(0.0, screen_size.y - 60.0)
	_control_panel.custom_minimum_size = Vector2(800.0, 60.0)
	_control_panel.visible = false
	_header_panel.add_child(_control_panel)

	# InspectionPanel
	_inspection_panel = Panel.new()
	_inspection_panel.name = "InspectionPanel"
	_inspection_panel.position = Vector2(10.0, 10.0)
	_inspection_panel.custom_minimum_size = Vector2(240.0, 290.0)
	_inspection_panel.visible = false
	_header_panel.add_child(_inspection_panel)


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


func _update_champion_buttons_in_place(screen_size: Vector2) -> void:
	var start_y_ratio := 0.41  # 41% of screen height
	var square_size_ratio := 0.10  # 10% of screen width
	var square_margin_ratio := 0.015  # 1.5% of screen width
	var start_x_ratio := 0.025  # 2.5% of screen width

	var square_size := screen_size.x * square_size_ratio
	var square_margin := screen_size.x * square_margin_ratio
	var cols: int = max(1, int((screen_size.x - screen_size.x * start_x_ratio * 2) / (square_size + square_margin)))

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

	# Update existing buttons or create new ones
	var existing_buttons: Dictionary = {}
	for child in _header_panel.get_children():
		if child is Button and child.name.begins_with("Champion"):
			existing_buttons[child.name] = child

	# Update or create buttons
	for i in range(visible_heroes.size()):
		var champion_id: StringName = visible_heroes[i]
		var button_name := "Champion_" + String(champion_id)
		var button: Button = existing_buttons.get(button_name)
		
		var champion: Variant = ChampionCatalogScript.get_champion(champion_id)
		if champion == null:
			continue
		
		var champion_dict: Dictionary = champion.to_dict()
		var stats_dict: Dictionary = champion_dict.get("stats", {})
		var role: StringName = StringName(stats_dict.get("role", ""))
		var is_taken: bool = champion_id in _player_picks or champion_id in _enemy_picks or champion_id in _banned_heroes

		var row := i / cols
		var col := i % cols
		var new_size := Vector2(square_size, square_size)
		var new_position := Vector2(screen_size.x * (start_x_ratio + float(col) * (square_size_ratio + square_margin_ratio)), screen_size.y * (start_y_ratio + float(row) * (square_size_ratio * screen_size.x / screen_size.y + square_margin_ratio * screen_size.x / screen_size.y)))

		if button != null:
			# Update existing button
			button.size = new_size
			button.position = new_position
			existing_buttons.erase(button_name)
		else:
			# Create new button
			button = Button.new()
			button.name = button_name
			button.text = String(champion_id)
			button.size = new_size
			button.position = new_position
			var role_color: Color = ROLE_COLORS.get(String(role), COLOR_BUTTON)
			_style_champion_button(button, role_color, champion_id, is_taken)
			button.pressed.connect(_on_champion_clicked.bind(champion_id))
			_header_panel.add_child(button)

	# Remove buttons that are no longer needed
	for button_name in existing_buttons:
		existing_buttons[button_name].queue_free()


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


func _update_visualization_from_snapshot() -> void:
	var snapshot: Dictionary = _backend.get_tick_snapshot()
	if snapshot.is_empty():
		return

	var units: Array = snapshot.get("units", [])
	for unit_data in units:
		var unit_dict: Dictionary = Dictionary(unit_data)
		var instance_id: int = int(unit_dict.get("instance_id", 0))
		var pos_x: float = float(unit_dict.get("pos_x", 0.0))
		var pos_y: float = float(unit_dict.get("pos_y", 0.0))
		var hp: float = float(unit_dict.get("hp", 0.0))
		var max_hp: float = float(unit_dict.get("max_hp", 0.0))
		var state: StringName = StringName(unit_dict.get("state", ""))
		var target_id: int = int(unit_dict.get("target_id", 0))
		var team: StringName = StringName(unit_dict.get("team", ""))

		_update_unit_node(instance_id, pos_x, pos_y, hp, max_hp, state, target_id, team)

	# Update projectiles
	var projectiles: Array = snapshot.get("projectiles", [])
	_update_projectiles(projectiles)


func _update_unit_node(instance_id: int, pos_x: float, pos_y: float, hp: float, max_hp: float, state: StringName, target_id: int, team: StringName) -> void:
	var unit_node: Node2D = _unit_nodes.get(instance_id)

	if unit_node == null:
		unit_node = _create_unit_node(instance_id, team)
		_unit_nodes[instance_id] = unit_node
		_world_layer.add_child(unit_node)

	# Update position (scale world to screen, centered on screen)
	var screen_size := get_viewport_rect().size
	var screen_center := screen_size / 2.0
	unit_node.position = screen_center + Vector2(pos_x * 50, pos_y * 50)  # Reduced scale from 100 to 50

	print("Updated unit node instance_id: ", instance_id, " pos: ", unit_node.position, " hp: ", hp)

	# Update HP bar
	var hp_bar: ProgressBar = unit_node.get_node("HPBar")
	if hp_bar != null:
		hp_bar.value = (hp / max_hp) * 100.0 if max_hp > 0 else 0.0

	# Update state text
	var state_label: Label = unit_node.get_node("StateLabel")
	if state_label != null:
		state_label.text = String(state)

	# Update target line
	var target_line: Line2D = unit_node.get_node("TargetLine")
	if target_line != null:
		var target_node: Node2D = _unit_nodes.get(target_id)
		if target_node != null and target_id != 0:
			target_line.points = PackedVector2Array([Vector2.ZERO, target_node.position - unit_node.position])
			target_line.visible = true
		else:
			target_line.visible = false


func _create_unit_node(instance_id: int, team: StringName) -> Node2D:
	var unit_node := Node2D.new()
	unit_node.name = "Unit_%d" % instance_id

	# Unit sprite with click area
	var sprite := ColorRect.new()
	sprite.name = "Sprite"
	sprite.size = Vector2(40, 40)  # Increased from 20x20 for better visibility
	sprite.position = Vector2(-20, -20)
	sprite.color = COLOR_PLAYER if team == &"player" else COLOR_ENEMY
	sprite.mouse_filter = Control.MOUSE_FILTER_STOP
	sprite.gui_input.connect(_on_unit_sprite_input.bind(instance_id, team))
	unit_node.add_child(sprite)

	# HP bar
	var hp_bar := ProgressBar.new()
	hp_bar.name = "HPBar"
	hp_bar.size = Vector2(40, 5)
	hp_bar.position = Vector2(-20, 25)
	hp_bar.value = 100.0
	hp_bar.min_value = 0.0
	hp_bar.max_value = 100.0
	unit_node.add_child(hp_bar)

	# State label
	var state_label := Label.new()
	state_label.name = "StateLabel"
	state_label.text = "IDLE"
	state_label.position = Vector2(-20, -30)
	state_label.add_theme_font_size_override("font_size", 10)
	state_label.add_theme_color_override("font_color", COLOR_TEXT)
	unit_node.add_child(state_label)

	# Target line
	var target_line := Line2D.new()
	target_line.name = "TargetLine"
	target_line.width = 2.0
	target_line.default_color = COLOR_TARGET_LINE
	target_line.visible = false
	unit_node.add_child(target_line)

	print("Created unit node for instance_id: ", instance_id, " team: ", team)
	return unit_node


func _on_unit_sprite_input(instance_id: int, team: StringName, event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		_selected_unit_id = instance_id
		_update_inspection_panel(instance_id, team)


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
		var source_id: int = int(proj_dict.get("source_id", 0))
		var target_id: int = int(proj_dict.get("target_id", 0))

		var proj_node := _create_projectile_node(proj_id, pos_x, pos_y, source_id, target_id)
		_projectile_nodes[proj_id] = proj_node
		_world_layer.add_child(proj_node)


func _create_projectile_node(proj_id: int, pos_x: float, pos_y: float, source_id: int, target_id: int) -> Node2D:
	var proj_node := Node2D.new()
	proj_node.name = "Projectile_%d" % proj_id
	proj_node.position = Vector2(pos_x * 100, pos_y * 100)

	# Projectile sprite
	var sprite := ColorRect.new()
	sprite.name = "Sprite"
	sprite.size = Vector2(8, 8)
	sprite.position = Vector2(-4, -4)
	sprite.color = Color.YELLOW
	proj_node.add_child(sprite)

	return proj_node


func _spawn_floating_text(text: String, pos: Vector2, color: Color = COLOR_TEXT) -> void:
	var label := Label.new()
	label.text = text
	label.position = pos
	label.add_theme_font_size_override("font_size", 16)
	label.add_theme_color_override("font_color", color)
	label.z_index = 100
	_world_layer.add_child(label)
	_floating_texts.append(label)

	# Animate floating text upward and fade out
	var tween := create_tween()
	tween.parallel().tween_property(label, "position", pos + Vector2(0, -50), 1.0)
	tween.parallel().tween_property(label, "modulate:a", 0.0, 1.0)
	tween.tween_callback(func(): _remove_floating_text(label))


func _remove_floating_text(label: Label) -> void:
	_floating_texts.erase(label)


func _setup_draft_ui() -> void:
	var screen_size := get_viewport_rect().size

	# Clear existing role filter buttons
	for child in _header_panel.get_children():
		if child is Button and child.name.begins_with("RoleFilter"):
			child.queue_free()
	await get_tree().process_frame

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

	# Hide existing champion buttons before clearing to prevent visual artifacts
	for child in _header_panel.get_children():
		if child is Button and child.name.begins_with("Champion"):
			child.visible = false
	
	# Force redraw to ensure hiding takes effect
	_header_panel.queue_redraw()
	await get_tree().process_frame
	
	# Clear existing champion buttons
	for child in _header_panel.get_children():
		if child is Button and child.name.begins_with("Champion"):
			child.queue_free()
	
	# Wait for frame to ensure clearing completes
	await get_tree().process_frame
	
	# Force redraw again to ensure clearing is visible
	_header_panel.queue_redraw()
	await get_tree().process_frame

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
		button.text = String(champion_id)
		button.custom_minimum_size = Vector2(square_size, square_size)
		button.position = Vector2(screen_size.x * (start_x_ratio + float(col) * (square_size_ratio + square_margin_ratio)), screen_size.y * (start_y_ratio + float(row) * (square_size_ratio * screen_size.x / screen_size.y + square_margin_ratio * screen_size.x / screen_size.y)))
		var role_color: Color = ROLE_COLORS.get(String(role), COLOR_BUTTON)
		_style_champion_button(button, role_color, champion_id, is_taken)
		button.pressed.connect(_on_champion_clicked.bind(champion_id))
		_header_panel.add_child(button)

	# Force redraw after creating new buttons
	_header_panel.queue_redraw()


func _on_role_filter_toggled(role: StringName, button: Button) -> void:
	print("Role filter toggled: ", role, " Current filters: ", _active_role_filters)
	
	# If all roles are enabled (empty filter list), focus on only the clicked role
	if _active_role_filters.is_empty():
		_active_role_filters.clear()
		_active_role_filters.append(role)
		print("Filter was empty, now focusing on: ", role)
	elif _active_role_filters.has(role):
		# If role is already selected, remove it
		_active_role_filters.erase(role)
		print("Removed role from filter: ", role, " New filters: ", _active_role_filters)
	else:
		# Add the role to the filter (additive)
		_active_role_filters.append(role)
		print("Added role to filter: ", role, " New filters: ", _active_role_filters)
	
	# Update all role filter button styles
	for child in _header_panel.get_children():
		if child is Button and child.name.begins_with("RoleFilter"):
			var button_role := String(child.name.replace("RoleFilter_", ""))
			var is_active := StringName(button_role) in _active_role_filters
			_update_role_filter_button_style(child, StringName(button_role), is_active)

	print("About to repopulate champion grid with filters: ", _active_role_filters)
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
	var screen_size := get_viewport_rect().size
	var center_x := screen_size.x / 2.0

	# Clear existing labels
	for child in _player_team_list.get_children():
		child.queue_free()
	for child in _enemy_team_list.get_children():
		child.queue_free()
	if _banned_list != null:
		for child in _banned_list.get_children():
			child.queue_free()
	await get_tree().process_frame

	# Add player team labels with exact Python spacing (22px vertical)
	for i in range(_player_picks.size()):
		var champion_id: StringName = _player_picks[i]
		var label := Label.new()
		label.text = "- %s" % String(champion_id).capitalize()
		label.add_theme_color_override("font_color", COLOR_TEXT)
		label.position = Vector2(0.0, float(i) * 22.0)
		_player_team_list.add_child(label)

	# Add enemy team labels with exact Python spacing (22px vertical)
	for i in range(_enemy_picks.size()):
		var champion_id: StringName = _enemy_picks[i]
		var label := Label.new()
		label.text = "- %s" % String(champion_id).capitalize()
		label.add_theme_color_override("font_color", COLOR_TEXT)
		label.position = Vector2(0.0, float(i) * 22.0)
		_enemy_team_list.add_child(label)

	# Add banned heroes with exact Python spacing (20px vertical)
	if _banned_list != null:
		var sorted_banned: Array[StringName] = _banned_heroes.duplicate()
		sorted_banned.sort_custom(func(a, b): return String(a) < String(b))
		for i in range(sorted_banned.size()):
			var champion_id: StringName = sorted_banned[i]
			var label := Label.new()
			label.text = String(champion_id)
			label.add_theme_color_override("font_color", COLOR_SUBTLE)
			label.position = Vector2(0.0, float(i) * 20.0)
			_banned_list.add_child(label)


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
	_start_combat()


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


func _update_inspection_panel(instance_id: int, team: StringName) -> void:
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
			var hp: float = float(unit_dict.get("hp", 0.0))
			var max_hp: float = float(unit_dict.get("max_hp", 0.0))
			var state: StringName = StringName(unit_dict.get("state", ""))
			var target_id: int = int(unit_dict.get("target_id", 0))

			label.text = "Unit: %s\nTeam: %s\nHP: %.1f/%.1f\nState: %s\nTarget: %d" % [archetype_id, team, hp, max_hp, state, target_id]
			return

	label.text = "Unit %d not found in snapshot" % instance_id


func _on_champion_clicked(champion_id: StringName) -> void:
	print("Champion clicked: ", champion_id, ", Game state: ", _game_state, ", Draft step: ", _draft_step_index)

	if _game_state != DRAFTING:
		print("Not in drafting state, ignoring click")
		return

	if _draft_step_index >= DRAFT_SEQUENCE.size():
		print("Draft sequence complete, ignoring click")
		return

	var current_turn: String = DRAFT_SEQUENCE[_draft_step_index]
	print("Current turn: ", current_turn)

	if current_turn == "P1_PICK":
		if _player_picks.size() < MAX_TEAM_SIZE:
			_player_picks.append(champion_id)
			print("Added to player team: ", champion_id)
	elif current_turn == "P2_PICK":
		if _enemy_picks.size() < MAX_TEAM_SIZE:
			_enemy_picks.append(champion_id)
			print("Added to enemy team: ", champion_id)

	_draft_step_index += 1
	_update_turn_display()
	_update_team_rosters()
	_update_champion_button_style(champion_id)

	if _draft_step_index >= DRAFT_SEQUENCE.size():
		_game_state = PREPARATION
		print("Draft complete, state changed to PREPARATION")




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


func _restart_match() -> void:
	_clear_units()
	_player_picks.clear()
	_enemy_picks.clear()
	_banned_heroes.clear()
	_draft_step_index = 0
	_game_state = DRAFTING
	_sim_time_accumulator = 0.0
	_update_turn_display()
	_update_team_rosters()
	_populate_champion_grid()


func _start_combat() -> void:
	print("Start combat clicked. Game state: ", _game_state, ", Player picks: ", _player_picks, ", Enemy picks: ", _enemy_picks)

	if _game_state != PREPARATION:
		print("Not in PREPARATION state")
		return

	if _player_picks.is_empty() or _enemy_picks.is_empty():
		print("Teams are empty")
		return

	_show_combat_ui()
	print("Starting combat with teams - Player: ", _player_picks, ", Enemy: ", _enemy_picks)

	var match_input: Object = MatchReplayInputScript.build_match_input(
		0,
		_player_picks,
		_enemy_picks,
		SimConstantsScript.DEFAULT_TICK_RATE
	)

	_backend.begin_match(match_input)
	_game_state = COMBAT
	_sim_time_accumulator = 0.0
	print("Combat started, game state changed to COMBAT")


func _show_combat_ui() -> void:
	var screen_size := get_viewport_rect().size

	# Hide drafting UI
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

	# Show world layer for combat visualization
	if _world_layer != null:
		_world_layer.visible = true

	# Show combat HUD with full screen
	if _control_panel != null:
		_control_panel.visible = true
		_control_panel.size = screen_size
		_control_panel.position = Vector2(0.0, 0.0)


func _end_match() -> void:
	_game_state = MATCH_OVER
	var summary: Dictionary = _backend.finish_and_summarize()
	print("Match ended. Winner: ", summary.get("winner_team", "draw"))


func _clear_units() -> void:
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
