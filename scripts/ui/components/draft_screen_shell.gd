class_name DraftScreenShell
extends Control

signal back_pressed
signal start_battle_pressed
signal auto_fill_pressed

const Tokens := preload("res://scripts/ui/ui_tokens.gd")
const Styles := preload("res://scripts/ui/ui_styles.gd")
const DraftLayoutScript := preload("res://scripts/ui/draft_layout.gd")
const UiTokensScript := preload("res://scripts/ui/ui_tokens.gd")

@onready var draft_panel: Panel = $DraftPanel
@onready var back_button: Button = $DraftPanel/BackButton
@onready var title_label: Label = $DraftPanel/TitleLabel
@onready var turn_label: Label = $DraftPanel/TurnLabel
@onready var debug_label: Label = $DraftPanel/DebugLabel
@onready var player_team_panel: Panel = $DraftPanel/PlayerTeamPanel
@onready var player_team_label: Label = $DraftPanel/PlayerTeamPanel/PlayerTeamLabel
@onready var player_team_list: VBoxContainer = $DraftPanel/PlayerTeamPanel/PlayerTeamList
@onready var player_bans_panel: Panel = $DraftPanel/PlayerBansPanel
@onready var player_bans_label: Label = $DraftPanel/PlayerBansPanel/PlayerBansLabel
@onready var player_bans_list: VBoxContainer = $DraftPanel/PlayerBansPanel/PlayerBansList
@onready var enemy_team_panel: Panel = $DraftPanel/EnemyTeamPanel
@onready var enemy_team_label: Label = $DraftPanel/EnemyTeamPanel/EnemyTeamLabel
@onready var enemy_team_list: VBoxContainer = $DraftPanel/EnemyTeamPanel/EnemyTeamList
@onready var enemy_bans_panel: Panel = $DraftPanel/EnemyBansPanel
@onready var enemy_bans_label: Label = $DraftPanel/EnemyBansPanel/EnemyBansLabel
@onready var enemy_bans_list: VBoxContainer = $DraftPanel/EnemyBansPanel/EnemyBansList
@onready var role_filter_container: HBoxContainer = $DraftPanel/RoleFilterContainer
@onready var draft_action_box: VBoxContainer = $DraftPanel/DraftActionBox
@onready var auto_fill_button: Button = $DraftPanel/DraftActionBox/RandomDraftButton
@onready var start_battle_button: Button = $DraftPanel/DraftActionBox/StartMatchButton
@onready var champion_scroll: ScrollContainer = $DraftPanel/ChampionScroll
@onready var champion_grid: GridContainer = $DraftPanel/ChampionScroll/ChampionGrid
@onready var recommendation_panel: Panel = $RecommendationPanel
@onready var recommendation_title: Label = $RecommendationPanel/RecommendationTitle
@onready var recommendation_list: VBoxContainer = $RecommendationPanel/RecommendationList


func _ready() -> void:
	_connect_signals()
	_apply_static_styles()
	apply_layout(get_viewport_rect().size)


func apply_layout(screen_size: Vector2, has_recommendation_panel: bool = false) -> void:
	DraftLayoutScript.apply_full_rect(self)
	DraftLayoutScript.apply_full_rect(draft_panel)
	DraftLayoutScript.apply_back_button_layout(back_button)
	DraftLayoutScript.apply_title_layout(title_label)
	DraftLayoutScript.apply_turn_layout(turn_label)
	DraftLayoutScript.apply_debug_label_layout(debug_label)
	DraftLayoutScript.apply_section_panel_layout(player_team_panel, &"left", 0)
	DraftLayoutScript.apply_section_panel_layout(player_bans_panel, &"left", 1)
	DraftLayoutScript.apply_section_panel_layout(enemy_team_panel, &"right", 0)
	DraftLayoutScript.apply_section_panel_layout(enemy_bans_panel, &"right", 1)
	for label in [player_team_label, player_bans_label, enemy_team_label, enemy_bans_label]:
		DraftLayoutScript.apply_section_header_layout(label as Label)
	for list in [player_team_list, player_bans_list, enemy_team_list, enemy_bans_list]:
		DraftLayoutScript.apply_section_list_layout(list as Control)
	DraftLayoutScript.apply_role_filter_layout(role_filter_container)
	DraftLayoutScript.apply_action_box_layout(draft_action_box, screen_size)
	DraftLayoutScript.apply_champion_scroll_layout(champion_scroll, screen_size, has_recommendation_panel)
	if has_recommendation_panel:
		DraftLayoutScript.apply_recommendation_panel_layout(recommendation_panel, screen_size)
	champion_grid.columns = DraftLayoutScript.calculate_grid_columns(screen_size.x, UiTokensScript.DRAFT_CHAMPION_TILE_PX)


func set_draft_visible(is_visible: bool) -> void:
	visible = is_visible
	draft_panel.visible = is_visible
	role_filter_container.visible = is_visible
	champion_scroll.visible = is_visible
	if not is_visible:
		recommendation_panel.visible = false


func set_battle_actions_visible(is_visible: bool) -> void:
	draft_action_box.visible = is_visible


func get_champion_grid() -> GridContainer:
	return champion_grid


func center_champion_grid() -> void:
	await get_tree().process_frame
	var grid_width: float = champion_grid.size.x
	var scroll_width: float = champion_scroll.size.x
	var horizontal_offset: float = (scroll_width - grid_width) / 2.0
	champion_grid.position.x = horizontal_offset


func get_role_filter_container() -> HBoxContainer:
	return role_filter_container


func _connect_signals() -> void:
	if not back_button.pressed.is_connected(_on_back_pressed):
		back_button.pressed.connect(_on_back_pressed)
	if not auto_fill_button.pressed.is_connected(_on_auto_fill_pressed):
		auto_fill_button.pressed.connect(_on_auto_fill_pressed)
	if not start_battle_button.pressed.is_connected(_on_start_battle_pressed):
		start_battle_button.pressed.connect(_on_start_battle_pressed)


func _apply_static_styles() -> void:
	draft_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE

	turn_label.add_theme_color_override("font_color", Tokens.COLOR_WARNING)
	debug_label.add_theme_color_override("font_color", Tokens.COLOR_WARNING)
	player_bans_label.add_theme_color_override("font_color", Tokens.COLOR_SUBTLE)
	enemy_bans_label.add_theme_color_override("font_color", Tokens.COLOR_SUBTLE)

	draft_action_box.add_theme_constant_override("separation", Tokens.DRAFT_ACTION_GAP_PX)
	auto_fill_button.custom_minimum_size = Vector2(Tokens.DRAFT_ACTION_W_PX, Tokens.DRAFT_ACTION_H_PX)
	start_battle_button.custom_minimum_size = Vector2(Tokens.DRAFT_ACTION_W_PX, Tokens.DRAFT_ACTION_H_PX)
	champion_grid.add_theme_constant_override("h_separation", Tokens.DRAFT_CHAMPION_GAP_PX)
	champion_grid.add_theme_constant_override("v_separation", Tokens.DRAFT_CHAMPION_GAP_PX)

	recommendation_title.add_theme_color_override("font_color", Tokens.COLOR_HIGHLIGHT)
	recommendation_title.add_theme_font_size_override("font_size", 24)
	recommendation_list.add_theme_constant_override("separation", 8)


func _on_back_pressed() -> void:
	back_pressed.emit()


func _on_auto_fill_pressed() -> void:
	auto_fill_pressed.emit()


func _on_start_battle_pressed() -> void:
	start_battle_pressed.emit()
