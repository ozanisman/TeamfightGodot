extends Control
class_name DraftScreen

const ChampionCatalog = preload("res://scripts/champion_catalog.gd")

signal hero_selected(hero_id: String)
signal random_draft_requested
signal start_match_requested
signal role_filter_toggled(role: String)

const DRAFT_SEQUENCE: Array[String] = [
	"P1_PICK",
	"P2_PICK",
	"P1_PICK",
	"P2_PICK",
	"P1_PICK",
	"P2_PICK",
	"P1_PICK",
	"P2_PICK",
	"P1_PICK",
	"P2_PICK",
]

const ROLE_COLORS := {
	"tank": Color(0.27, 0.51, 0.71),
	"fighter": Color(0.82, 0.41, 0.11),
	"assassin": Color(0.60, 0.20, 0.82),
	"marksman": Color(0.22, 0.55, 0.22),
	"mage": Color(0.10, 0.70, 0.95),
	"support": Color(0.86, 0.67, 0.18),
}

@onready var turn_label: Label = $Panel/VBox/TopRow/TurnLabel
@onready var player_roster: Label = $Panel/VBox/ContentRow/Rosters/PlayerRoster
@onready var enemy_roster: Label = $Panel/VBox/ContentRow/Rosters/EnemyRoster
@onready var banned_roster: Label = $Panel/VBox/ContentRow/Rosters/BannedRoster
@onready var filter_row: HBoxContainer = $Panel/VBox/TopRow/Filters
@onready var hero_grid: GridContainer = $Panel/VBox/ContentRow/HeroGridScroll/HeroGrid
@onready var random_button: Button = $Panel/VBox/ActionsRow/RandomButton
@onready var start_button: Button = $Panel/VBox/ActionsRow/StartButton

var player_picks: Array[String] = []
var enemy_picks: Array[String] = []
var banned_heroes: Array[String] = []
var draft_step_index: int = 0
var active_role_filters: Array[String] = []


func _ready() -> void:
	_bind_actions()
	_build_role_filters()
	_refresh()


func _bind_actions() -> void:
	random_button.pressed.connect(_on_random_button_pressed)
	start_button.pressed.connect(_on_start_button_pressed)


func set_draft_state(
		new_player_picks: Array[String],
		new_enemy_picks: Array[String],
		new_banned_heroes: Array[String],
		new_draft_step_index: int,
		new_active_role_filters: Array[String]
	) -> void:
	player_picks = new_player_picks.duplicate()
	enemy_picks = new_enemy_picks.duplicate()
	banned_heroes = new_banned_heroes.duplicate()
	draft_step_index = new_draft_step_index
	active_role_filters = new_active_role_filters.duplicate()
	_refresh()


func get_active_role_filters() -> Array[String]:
	return active_role_filters.duplicate()


func _build_role_filters() -> void:
	for child in filter_row.get_children():
		child.queue_free()

	for role in ChampionCatalog.ROLE_ORDER:
		var button := Button.new()
		button.text = role.to_upper()
		button.toggle_mode = true
		button.button_pressed = false
		button.custom_minimum_size = Vector2(96.0, 32.0)
		button.pressed.connect(_on_role_filter_pressed.bind(role))
		filter_row.add_child(button)


func _refresh() -> void:
	_refresh_role_filter_buttons()
	_refresh_turn_label()
	_refresh_rosters()
	_refresh_hero_grid()
	random_button.disabled = draft_step_index >= DRAFT_SEQUENCE.size()
	start_button.disabled = draft_step_index < DRAFT_SEQUENCE.size()


func _refresh_role_filter_buttons() -> void:
	for child in filter_row.get_children():
		if child is Button:
			var role := String(child.text).to_lower()
			child.button_pressed = active_role_filters.has(role)
			var hero_color: Color = ROLE_COLORS.get(role, Color(0.5, 0.5, 0.5))
			child.modulate = hero_color if child.button_pressed or active_role_filters.is_empty() else hero_color.darkened(0.55)


func _refresh_turn_label() -> void:
	var turn_text := "Draft Complete"
	if draft_step_index < DRAFT_SEQUENCE.size():
		turn_text = DRAFT_SEQUENCE[draft_step_index].replace("_", " ")
	turn_label.text = "TURN: %s" % turn_text


func _refresh_rosters() -> void:
	player_roster.text = _format_roster("PLAYER 1 TEAM", player_picks, Color(0.35, 0.60, 1.0))
	enemy_roster.text = _format_roster("PLAYER 2 TEAM", enemy_picks, Color(0.92, 0.35, 0.35))
	banned_roster.text = _format_roster("BANNED", banned_heroes, Color(0.75, 0.75, 0.75))


func _format_roster(title: String, roster: Array[String], _title_color: Color) -> String:
	var lines: Array[String] = []
	lines.append("%s" % title)
	if roster.is_empty():
		lines.append("- none")
	else:
		for hero_id in roster:
			var hero := ChampionCatalog.get_by_id(hero_id)
			var name := String(hero.get("name", hero_id))
			var role := String(hero.get("role", ""))
			lines.append("- %s (%s)" % [name, role])
	return "\n".join(lines)


func _refresh_hero_grid() -> void:
	for child in hero_grid.get_children():
		child.queue_free()

	var heroes := ChampionCatalog.available_heroes(player_picks, enemy_picks, banned_heroes, active_role_filters)
	heroes.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return String(a.get("name", "")) < String(b.get("name", ""))
	)

	for hero in heroes:
		var button := Button.new()
		button.text = "%s\n[%s]" % [String(hero.get("name", "")), String(hero.get("role", "")).to_upper()]
		button.custom_minimum_size = Vector2(140.0, 72.0)
		button.tooltip_text = "%s" % String(hero.get("id", ""))
		button.pressed.connect(_on_hero_button_pressed.bind(String(hero.get("id", ""))))
		var hero_color: Color = ROLE_COLORS.get(String(hero.get("role", "")), Color(0.4, 0.4, 0.4))
		button.modulate = hero_color
		hero_grid.add_child(button)


func _on_role_filter_pressed(role: String) -> void:
	if active_role_filters.has(role):
		active_role_filters.erase(role)
	else:
		active_role_filters.append(role)
	role_filter_toggled.emit(role)
	_refresh()


func _on_hero_button_pressed(hero_id: String) -> void:
	hero_selected.emit(hero_id)


func _on_random_button_pressed() -> void:
	random_draft_requested.emit()


func _on_start_button_pressed() -> void:
	start_match_requested.emit()
