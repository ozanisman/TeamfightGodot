class_name DraftScreenShell
extends Control

signal back_pressed
signal start_battle_pressed
signal auto_fill_pressed
signal reset_pressed

const Tokens := preload("res://scripts/ui/ui_tokens.gd")
const Styles := preload("res://scripts/ui/ui_styles.gd")
const DraftLayoutScript := preload("res://scripts/ui/draft_layout.gd")

var draft_panel: Panel
var title_label: Label
var turn_label: Label
var debug_label: Label
var player_team_panel: Panel
var player_team_label: Label
var player_team_list: VBoxContainer
var player_bans_panel: Panel
var player_bans_label: Label
var player_bans_list: VBoxContainer
var enemy_team_panel: Panel
var enemy_team_label: Label
var enemy_team_list: VBoxContainer
var enemy_bans_panel: Panel
var enemy_bans_label: Label
var enemy_bans_list: VBoxContainer
var role_filter_container: HBoxContainer
var draft_action_box: VBoxContainer
var auto_fill_button: Button
var start_battle_button: Button
var champion_scroll: ScrollContainer
var champion_grid: GridContainer
var recommendation_panel: Panel
var recommendation_title: Label
var recommendation_list: VBoxContainer


func _ready() -> void:
	if draft_panel == null:
		_build()
	apply_layout(get_viewport_rect().size)


func apply_layout(screen_size: Vector2) -> void:
	DraftLayoutScript.apply_full_rect(self)
	if draft_panel != null:
		DraftLayoutScript.apply_full_rect(draft_panel)
	if title_label != null:
		DraftLayoutScript.apply_title_layout(title_label)
	if turn_label != null:
		DraftLayoutScript.apply_turn_layout(turn_label)
	if debug_label != null:
		DraftLayoutScript.apply_debug_label_layout(debug_label)
	if player_team_panel != null:
		DraftLayoutScript.apply_section_panel_layout(player_team_panel, &"left", 0)
	if player_bans_panel != null:
		DraftLayoutScript.apply_section_panel_layout(player_bans_panel, &"left", 1)
	if enemy_team_panel != null:
		DraftLayoutScript.apply_section_panel_layout(enemy_team_panel, &"right", 0)
	if enemy_bans_panel != null:
		DraftLayoutScript.apply_section_panel_layout(enemy_bans_panel, &"right", 1)
	if role_filter_container != null:
		DraftLayoutScript.apply_role_filter_layout(role_filter_container)
	if draft_action_box != null:
		DraftLayoutScript.apply_action_box_layout(draft_action_box, screen_size)
	if champion_scroll != null:
		DraftLayoutScript.apply_champion_scroll_layout(champion_scroll, screen_size)
	if champion_grid != null:
		champion_grid.columns = DraftLayoutScript.calculate_grid_columns(screen_size.x, Tokens.DRAFT_CHAMPION_TILE_PX)
	if recommendation_panel != null:
		DraftLayoutScript.apply_recommendation_panel_layout(recommendation_panel, screen_size)


func set_draft_visible(is_visible: bool) -> void:
	visible = is_visible
	if draft_panel != null:
		draft_panel.visible = is_visible
	if role_filter_container != null:
		role_filter_container.visible = is_visible
	if champion_scroll != null:
		champion_scroll.visible = is_visible
	if recommendation_panel != null and not is_visible:
		recommendation_panel.visible = false


func set_battle_actions_visible(is_visible: bool) -> void:
	if draft_action_box != null:
		draft_action_box.visible = is_visible


func get_champion_grid() -> GridContainer:
	return champion_grid


func get_role_filter_container() -> HBoxContainer:
	return role_filter_container


func _build() -> void:
	name = "DraftScreenShell"
	DraftLayoutScript.apply_full_rect(self)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	draft_panel = Panel.new()
	draft_panel.name = "DraftPanel"
	draft_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	draft_panel.add_theme_stylebox_override("panel", Styles.solid(Tokens.COLOR_PANEL))
	add_child(draft_panel)

	var back_button := Button.new()
	back_button.name = "BackButton"
	back_button.text = "Back to Menu"
	DraftLayoutScript.apply_back_button_layout(back_button)
	back_button.pressed.connect(func() -> void: back_pressed.emit())
	draft_panel.add_child(back_button)

	title_label = Label.new()
	title_label.name = "TitleLabel"
	title_label.text = "MANUAL TEAM SELECTION"
	title_label.add_theme_color_override("font_color", Tokens.COLOR_TEXT)
	draft_panel.add_child(title_label)

	turn_label = Label.new()
	turn_label.name = "TurnLabel"
	turn_label.text = "TURN: P1_PICK"
	turn_label.add_theme_color_override("font_color", Tokens.COLOR_WARNING)
	draft_panel.add_child(turn_label)

	debug_label = Label.new()
	debug_label.name = "DebugLabel"
	debug_label.visible = true
	debug_label.add_theme_color_override("font_color", Tokens.COLOR_WARNING)
	draft_panel.add_child(debug_label)

	player_team_panel = _build_section_panel("PlayerTeamPanel", &"left", 0)
	player_team_label = _build_section_label(player_team_panel, "PlayerTeamLabel", "PLAYER 1", Tokens.COLOR_TEXT)
	player_team_list = _build_section_list(player_team_panel, "PlayerTeamList")

	player_bans_panel = _build_section_panel("PlayerBansPanel", &"left", 1)
	player_bans_label = _build_section_label(player_bans_panel, "PlayerBansLabel", "BANS", Tokens.COLOR_SUBTLE)
	player_bans_list = _build_section_list(player_bans_panel, "PlayerBansList")

	enemy_team_panel = _build_section_panel("EnemyTeamPanel", &"right", 0)
	enemy_team_label = _build_section_label(enemy_team_panel, "EnemyTeamLabel", "PLAYER 2", Tokens.COLOR_TEXT)
	enemy_team_list = _build_section_list(enemy_team_panel, "EnemyTeamList")

	enemy_bans_panel = _build_section_panel("EnemyBansPanel", &"right", 1)
	enemy_bans_label = _build_section_label(enemy_bans_panel, "EnemyBansLabel", "BANS", Tokens.COLOR_SUBTLE)
	enemy_bans_list = _build_section_list(enemy_bans_panel, "EnemyBansList")

	role_filter_container = HBoxContainer.new()
	role_filter_container.name = "RoleFilterContainer"
	role_filter_container.visible = false
	draft_panel.add_child(role_filter_container)

	draft_action_box = VBoxContainer.new()
	draft_action_box.name = "DraftActionBox"
	draft_panel.add_child(draft_action_box)

	auto_fill_button = Button.new()
	auto_fill_button.name = "RandomDraftButton"
	auto_fill_button.text = "RANDOM DRAFT"
	auto_fill_button.custom_minimum_size = Vector2(Tokens.DRAFT_ACTION_W_PX, Tokens.DRAFT_ACTION_H_PX)
	auto_fill_button.pressed.connect(func() -> void: auto_fill_pressed.emit())
	draft_action_box.add_child(auto_fill_button)

	start_battle_button = Button.new()
	start_battle_button.name = "StartMatchButton"
	start_battle_button.text = "START MATCH"
	start_battle_button.custom_minimum_size = Vector2(Tokens.DRAFT_ACTION_W_PX, Tokens.DRAFT_ACTION_H_PX)
	start_battle_button.pressed.connect(func() -> void: start_battle_pressed.emit())
	draft_action_box.add_child(start_battle_button)

	champion_scroll = ScrollContainer.new()
	champion_scroll.name = "ChampionScroll"
	champion_scroll.visible = false
	draft_panel.add_child(champion_scroll)

	champion_grid = GridContainer.new()
	champion_grid.name = "ChampionGrid"
	champion_grid.add_theme_constant_override("h_separation", Tokens.DRAFT_CHAMPION_GAP_PX)
	champion_grid.add_theme_constant_override("v_separation", Tokens.DRAFT_CHAMPION_GAP_PX)
	champion_scroll.add_child(champion_grid)

	recommendation_panel = Panel.new()
	recommendation_panel.name = "RecommendationPanel"
	recommendation_panel.visible = false
	recommendation_panel.add_theme_stylebox_override("panel", Styles.solid(Tokens.COLOR_PANEL))
	add_child(recommendation_panel)

	recommendation_title = Label.new()
	recommendation_title.name = "RecommendationTitle"
	recommendation_title.text = "RECOMMENDATIONS"
	recommendation_title.position = Vector2(20.0, 20.0)
	recommendation_title.add_theme_color_override("font_color", Tokens.COLOR_HIGHLIGHT)
	recommendation_title.add_theme_font_size_override("font_size", 24)
	recommendation_panel.add_child(recommendation_title)

	recommendation_list = VBoxContainer.new()
	recommendation_list.name = "RecommendationList"
	recommendation_list.set_anchors_preset(Control.PRESET_TOP_WIDE)
	recommendation_list.offset_left = 20.0
	recommendation_list.offset_top = 60.0
	recommendation_list.offset_right = -20.0
	recommendation_list.offset_bottom = -20.0
	recommendation_list.add_theme_constant_override("separation", 8)
	recommendation_panel.add_child(recommendation_list)


func _build_section_panel(panel_name: String, side: StringName, slot: int) -> Panel:
	var panel := Panel.new()
	panel.name = panel_name
	DraftLayoutScript.apply_section_panel_layout(panel, side, slot)
	panel.add_theme_stylebox_override("panel", Styles.panel(Tokens.COLOR_SECTION_BG, Tokens.COLOR_SECTION_BORDER))
	draft_panel.add_child(panel)
	return panel


func _build_section_label(parent: Control, label_name: String, text: String, color: Color) -> Label:
	var label := Label.new()
	label.name = label_name
	label.text = text
	DraftLayoutScript.apply_section_header_layout(label)
	label.add_theme_color_override("font_color", color)
	label.add_theme_font_size_override("font_size", 16)
	parent.add_child(label)
	return label


func _build_section_list(parent: Control, list_name: String) -> VBoxContainer:
	var list := VBoxContainer.new()
	list.name = list_name
	DraftLayoutScript.apply_section_list_layout(list)
	parent.add_child(list)
	return list
