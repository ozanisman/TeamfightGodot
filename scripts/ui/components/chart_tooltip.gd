class_name ChartTooltip
extends PanelContainer

const UiTokensScript := preload("res://scripts/ui/ui_tokens.gd")
const UiStylesScript := preload("res://scripts/ui/ui_styles.gd")

const UI_TOOLTIP_MOUSE_OFF := Vector2(16, 20)
const UI_TOOLTIP_FONT_SIZE := 22
const UI_TOOLTIP_MIN_WIDTH := 520
const UI_TOOLTIP_CONTENT_MARGIN := 10

@export var _label_path: NodePath = NodePath("RichTextLabel")

@onready var _label: RichTextLabel = get_node_or_null(_label_path)
var _style: StyleBoxFlat

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	z_index = UiTokensScript.Z_CHART_TOOLTIP
	_style = UiStylesScript.panel(UiTokensScript.COLOR_PANEL, UiTokensScript.COLOR_SUBTLE, 2)
	_style.set_corner_radius_all(4)
	_style.set_content_margin_all(UI_TOOLTIP_CONTENT_MARGIN)
	add_theme_stylebox_override("panel", _style)
	_label.bbcode_enabled = true
	_label.scroll_active = false
	_label.fit_content = true
	_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_label.add_theme_font_size_override("normal_font_size", UI_TOOLTIP_FONT_SIZE)
	_label.add_theme_font_size_override("bold_font_size", UI_TOOLTIP_FONT_SIZE)
	_label.add_theme_font_size_override("italics_font_size", UI_TOOLTIP_FONT_SIZE)
	_label.add_theme_font_size_override("bold_italics_font_size", UI_TOOLTIP_FONT_SIZE)
	_label.add_theme_font_size_override("mono_font_size", UI_TOOLTIP_FONT_SIZE)
	_label.custom_minimum_size.x = UI_TOOLTIP_MIN_WIDTH
	visible = false

func show_tooltip(text: String, border_color: Color) -> void:
	_label.text = text
	_style.border_color = border_color
	visible = true
	reset_size()
	update_position()

func hide_tooltip() -> void:
	visible = false

func update_position() -> void:
	if not visible:
		return
	var vp: Rect2 = get_viewport().get_visible_rect()
	var sz: Vector2 = get_combined_minimum_size()
	var g: Vector2 = get_global_mouse_position() + UI_TOOLTIP_MOUSE_OFF
	g.x = clampf(g.x, vp.position.x + 4.0, vp.end.x - sz.x - 4.0)
	g.y = clampf(g.y, vp.position.y + 4.0, vp.end.y - sz.y - 4.0)
	global_position = g
