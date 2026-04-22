extends Control
class_name DraftScreen

const ChampionCatalog = preload("res://scripts/champion_catalog.gd")
const DraftHeroButtonScript = preload("res://scripts/draft_hero_button.gd")

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
	"tank": Color(0.22, 0.50, 0.86),
	"fighter": Color(0.92, 0.48, 0.14),
	"assassin": Color(0.72, 0.24, 0.92),
	"marksman": Color(0.16, 0.74, 0.60),
	"mage": Color(0.18, 0.68, 0.96),
	"support": Color(0.82, 0.60, 0.22),
}

const TOOLTIP_SIZE := Vector2(360.0, 520.0)
const TOOLTIP_OFFSET := Vector2(18.0, 18.0)
const TOOLTIP_MARGIN := 8.0

@onready var turn_label: Label = $Panel/OuterMargin/VBox/TopRow/TurnLabel
@onready var pick_count_label: Label = $Panel/OuterMargin/VBox/TopRow/PickCountLabel
@onready var player_roster: Label = $Panel/OuterMargin/VBox/ContentRow/RosterPanel/RosterMargin/Rosters/PlayerRoster
@onready var enemy_roster: Label = $Panel/OuterMargin/VBox/ContentRow/RosterPanel/RosterMargin/Rosters/EnemyRoster
@onready var banned_roster: Label = $Panel/OuterMargin/VBox/ContentRow/RosterPanel/RosterMargin/Rosters/BannedRoster
@onready var filter_row: HBoxContainer = $Panel/OuterMargin/VBox/TopRow/Filters
@onready var hero_summary_label: Label = $Panel/OuterMargin/VBox/ContentRow/HeroPanel/HeroMargin/HeroVBox/HeroSummaryLabel
@onready var hero_grid_scroll: ScrollContainer = $Panel/OuterMargin/VBox/ContentRow/HeroPanel/HeroMargin/HeroVBox/HeroGridScroll
@onready var hero_grid: GridContainer = $Panel/OuterMargin/VBox/ContentRow/HeroPanel/HeroMargin/HeroVBox/HeroGridScroll/HeroGrid
@onready var random_button: Button = $Panel/OuterMargin/VBox/ActionsRow/RandomButton
@onready var start_button: Button = $Panel/OuterMargin/VBox/ActionsRow/StartButton
@onready var tooltip_panel: PanelContainer = $TooltipPanel
@onready var tooltip_accent: ColorRect = $TooltipPanel/TooltipMargin/TooltipVBox/TooltipAccent
@onready var tooltip_title: Label = $TooltipPanel/TooltipMargin/TooltipVBox/TooltipTitle
@onready var tooltip_role: Label = $TooltipPanel/TooltipMargin/TooltipVBox/TooltipRole
@onready var tooltip_stats: RichTextLabel = $TooltipPanel/TooltipMargin/TooltipVBox/TooltipStats
@onready var tooltip_description: RichTextLabel = $TooltipPanel/TooltipMargin/TooltipVBox/TooltipDescription
@onready var tooltip_ability: RichTextLabel = $TooltipPanel/TooltipMargin/TooltipVBox/TooltipAbility
@onready var tooltip_ultimate: RichTextLabel = $TooltipPanel/TooltipMargin/TooltipVBox/TooltipUltimate
@onready var tooltip_passive: RichTextLabel = $TooltipPanel/TooltipMargin/TooltipVBox/TooltipPassive

var player_picks: Array[String] = []
var enemy_picks: Array[String] = []
var banned_heroes: Array[String] = []
var draft_step_index: int = 0
var active_role_filters: Array[String] = []
var tooltip_base_style: StyleBoxFlat


func _ready() -> void:
	_bind_actions()
	_build_role_filters()
	_cache_tooltip_style()
	_hide_tooltip()
	_update_layout()
	_refresh()


func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED:
		_update_layout()
		if is_node_ready():
			_update_tooltip_position()


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
	pick_count_label.text = "PICKS: %d / %d" % [player_picks.size() + enemy_picks.size(), DRAFT_SEQUENCE.size()]


func _refresh_rosters() -> void:
	player_roster.text = _format_roster("PLAYER 1 TEAM", player_picks, Color(0.35, 0.60, 1.0))
	enemy_roster.text = _format_roster("PLAYER 2 TEAM", enemy_picks, Color(0.92, 0.35, 0.35))
	banned_roster.text = _format_roster("BANNED", banned_heroes, Color(0.75, 0.75, 0.75))
	_refresh_hero_summary()


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
	_hide_tooltip()
	for child in hero_grid.get_children():
		child.queue_free()

	var heroes: Array[Dictionary] = []
	for hero in ChampionCatalog.HEROES:
		var role := String(hero.get("role", ""))
		if not active_role_filters.is_empty() and not active_role_filters.has(role):
			continue
		heroes.append(ChampionCatalog.get_by_id(String(hero.get("id", ""))))
	heroes.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return String(a.get("name", "")) < String(b.get("name", ""))
	)

	for hero in heroes:
		var hero_id := String(hero.get("id", ""))
		var owner := ""
		if player_picks.has(hero_id):
			owner = "player"
		elif enemy_picks.has(hero_id):
			owner = "enemy"
		elif banned_heroes.has(hero_id):
			owner = "banned"
		var button := DraftHeroButtonScript.new()
		button.setup(hero_id, hero)
		button.disabled = not owner.is_empty()
		button.set_pick_owner(owner)
		button.custom_minimum_size = Vector2(0.0, 88.0)
		button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		button.pressed.connect(_on_hero_button_pressed.bind(hero_id))
		button.mouse_entered.connect(_on_hero_hovered.bind(hero))
		button.mouse_exited.connect(_on_hero_unhovered)
		hero_grid.add_child(button)


func _on_role_filter_pressed(role: String) -> void:
	role_filter_toggled.emit(role)


func _on_random_button_pressed() -> void:
	random_draft_requested.emit()


func _on_start_button_pressed() -> void:
	start_match_requested.emit()


func _refresh_hero_summary() -> void:
	var filtered_count := ChampionCatalog.available_heroes(player_picks, enemy_picks, banned_heroes, active_role_filters).size()
	var role_text := "all roles"
	if not active_role_filters.is_empty():
		role_text = ", ".join(active_role_filters).to_upper()
	hero_summary_label.text = "%d heroes available - filters: %s" % [filtered_count, role_text]


func _on_hero_button_pressed(hero_id: String) -> void:
	hero_selected.emit(hero_id)


func _update_layout() -> void:
	if hero_grid == null or hero_grid_scroll == null:
		return
	var available_width := maxf(0.0, hero_grid_scroll.size.x)
	if available_width <= 0.0:
		available_width = maxf(0.0, size.x - 360.0)
	var columns := int(floor((available_width + 12.0) / 180.0))
	hero_grid.columns = clampi(columns, 2, 8)


func _on_hero_hovered(hero: Dictionary) -> void:
	_show_tooltip(hero)


func _on_hero_unhovered() -> void:
	_hide_tooltip()


func _show_tooltip(hero: Dictionary) -> void:
	_apply_tooltip_theme(hero)
	tooltip_title.text = "%s" % String(hero.get("name", ""))
	tooltip_role.text = String(hero.get("role", "")).to_upper()
	var stat_line := "\n".join([
		"HEALTH: %.0f" % float(hero.get("max_hp", 0.0)),
		"ATTACK DAMAGE: %.0f" % float(hero.get("attack_damage", 0.0)),
		"ATTACK SPEED: %.2f" % float(hero.get("attack_speed", 0.0)),
		"RANGE: %.2f" % float(hero.get("attack_range", 0.0)),
		"MOVE SPEED: %.2f" % float(hero.get("move_speed", 0.0)),
		"ARMOR: %.2f" % float(hero.get("armor", 0.0)),
		"MAGIC RESIST: %.2f" % float(hero.get("magic_resist", 0.0)),
		"TENACITY: %.2f" % float(hero.get("tenacity", 0.0)),
		"LIFESTEAL: %.2f" % float(hero.get("life_steal", 0.0)),
		"MAX MANA: %.0f" % float(hero.get("max_mana", 0.0)),
		"MANA / ATK: %.0f" % float(hero.get("mana_per_attack", 0.0)),
		"ABILITY CD: %.2f" % float(hero.get("ability_cd", 0.0)),
		"ULTIMATE CD: %.2f" % float(hero.get("ultimate_cd", 0.0)),
		"PROJECTILE SPD: %.2f" % float(hero.get("projectile_speed", 0.0)),
		"PROJECTILE RAD: %.2f" % float(hero.get("projectile_radius", 0.0)),
		"RESPAWN: %.2f" % float(hero.get("respawn_time", 0.0)),
		"PASSIVE ID: %s" % String(hero.get("passive_id", "")),
		"UNIT ID: %s" % String(hero.get("id", "")),
	])
	tooltip_stats.text = "[b]STATS[/b]\n%s" % stat_line
	tooltip_description.text = "[b]DESCRIPTION[/b]\n%s" % String(hero.get("description", ""))
	tooltip_ability.text = "[b]ABILITY[/b]\n%s" % String(hero.get("ability_desc", ""))
	tooltip_ultimate.text = "[b]ULTIMATE[/b]\n%s" % String(hero.get("ultimate_desc", ""))
	tooltip_passive.text = "[b]PASSIVE[/b]\n%s" % String(hero.get("passive_desc", ""))
	tooltip_panel.visible = true
	_update_tooltip_position()


func _hide_tooltip() -> void:
	tooltip_panel.visible = false


func _update_tooltip_position() -> void:
	if tooltip_panel == null or not tooltip_panel.visible:
		return
	var mouse_pos := get_viewport().get_mouse_position()
	var viewport_size := get_viewport_rect().size
	var next_pos := mouse_pos + TOOLTIP_OFFSET
	if next_pos.x + TOOLTIP_SIZE.x > viewport_size.x:
		next_pos.x = mouse_pos.x - TOOLTIP_SIZE.x - TOOLTIP_OFFSET.x
	if next_pos.y + TOOLTIP_SIZE.y > viewport_size.y:
		next_pos.y = mouse_pos.y - TOOLTIP_SIZE.y - TOOLTIP_OFFSET.y
	next_pos.x = clampf(next_pos.x, TOOLTIP_MARGIN, maxf(TOOLTIP_MARGIN, viewport_size.x - TOOLTIP_SIZE.x - TOOLTIP_MARGIN))
	next_pos.y = clampf(next_pos.y, TOOLTIP_MARGIN, maxf(TOOLTIP_MARGIN, viewport_size.y - TOOLTIP_SIZE.y - TOOLTIP_MARGIN))
	tooltip_panel.position = next_pos


func _cache_tooltip_style() -> void:
	var panel_style := tooltip_panel.get_theme_stylebox("panel")
	if panel_style is StyleBoxFlat:
		tooltip_base_style = (panel_style as StyleBoxFlat).duplicate() as StyleBoxFlat


func _apply_tooltip_theme(hero: Dictionary) -> void:
	var role := String(hero.get("role", ""))
	var role_color: Color = ROLE_COLORS.get(role, Color(0.6, 0.6, 0.6))
	var panel_style := tooltip_base_style.duplicate() as StyleBoxFlat if tooltip_base_style != null else StyleBoxFlat.new()
	panel_style.bg_color = Color(0.09, 0.11, 0.16, 0.97).lerp(role_color, 0.22)
	panel_style.border_color = role_color.lightened(0.18)
	tooltip_panel.add_theme_stylebox_override("panel", panel_style)
	tooltip_accent.color = role_color.lightened(0.10)
	tooltip_title.add_theme_color_override("font_color", role_color.lightened(0.40))
	tooltip_role.add_theme_color_override("font_color", role_color.lightened(0.16))
