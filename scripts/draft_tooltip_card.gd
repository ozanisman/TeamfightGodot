extends PanelContainer
class_name DraftTooltipCard

const ROLE_COLORS := {
	"tank": Color(0.27, 0.51, 0.71),
	"fighter": Color(0.82, 0.41, 0.11),
	"assassin": Color(0.60, 0.20, 0.82),
	"marksman": Color(0.22, 0.55, 0.22),
	"mage": Color(0.10, 0.70, 0.95),
	"support": Color(0.86, 0.67, 0.18),
}

const TOOLTIP_BASE_PANEL_COLOR := Color(0.09, 0.11, 0.16, 0.97)

var hero_data: Dictionary = {}
var base_panel_style: StyleBoxFlat

@onready var accent_strip: ColorRect = $CardMargin/CardVBox/AccentStrip
@onready var title_label: Label = $CardMargin/CardVBox/TitleLabel
@onready var role_label: Label = $CardMargin/CardVBox/RoleLabel
@onready var stats_label: Label = $CardMargin/CardVBox/StatsLabel
@onready var description_label: Label = $CardMargin/CardVBox/DescriptionLabel
@onready var ability_label: Label = $CardMargin/CardVBox/AbilityLabel
@onready var ultimate_label: Label = $CardMargin/CardVBox/UltimateLabel
@onready var passive_label: Label = $CardMargin/CardVBox/PassiveLabel


func setup(hero: Dictionary) -> void:
	hero_data = hero.duplicate(true)
	if is_node_ready():
		_apply_hero_data()


func _ready() -> void:
	base_panel_style = get_theme_stylebox("panel").duplicate() as StyleBoxFlat
	_apply_hero_data()


func _apply_hero_data() -> void:
	if hero_data.is_empty() or base_panel_style == null:
		return

	var role := String(hero_data.get("role", ""))
	var role_color: Color = ROLE_COLORS.get(role, Color(0.6, 0.6, 0.6))
	var panel_style := base_panel_style.duplicate() as StyleBoxFlat
	panel_style.bg_color = TOOLTIP_BASE_PANEL_COLOR.lerp(role_color, 0.22)
	panel_style.border_color = role_color.lightened(0.18)
	add_theme_stylebox_override("panel", panel_style)

	accent_strip.color = role_color.lightened(0.06)
	title_label.text = String(hero_data.get("name", ""))
	title_label.add_theme_color_override("font_color", role_color.lightened(0.40))
	role_label.add_theme_color_override("font_color", role_color.lightened(0.16))
	stats_label.text = "HP %.0f | ATK %.0f | AS %.2f | RNG %.2f | MOVE %.2f" % [
		float(hero_data.get("max_hp", 0.0)),
		float(hero_data.get("attack_damage", 0.0)),
		float(hero_data.get("attack_speed", 0.0)),
		float(hero_data.get("attack_range", 0.0)),
		float(hero_data.get("move_speed", 0.0)),
	]
	description_label.text = String(hero_data.get("description", ""))
	ability_label.text = String(hero_data.get("ability_desc", ""))
	ultimate_label.text = String(hero_data.get("ultimate_desc", ""))
	passive_label.text = String(hero_data.get("passive_desc", ""))
