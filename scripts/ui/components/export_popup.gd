class_name ExportPopup
extends Window

const UiStylesScript := preload("res://scripts/ui/ui_styles.gd")
const UiTokensScript := preload("res://scripts/ui/ui_tokens.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

signal closed

@export var _native_required_notice_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/NativeRequiredNotice")
@export var _modes_row_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/ModesRow")
@export var _sample_edit_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/SampleBlock/SampleEdit")
@export var _worker_edit_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/WorkerBlock/WorkerEdit")
@export var _output_dir_edit_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/OutputDirBlock/OutputDirEdit")
@export var _eval_predictions_check_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/PredictionBlock/EvalPredictionsCheck")
@export var _prediction_dir_edit_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/PredictionBlock/PredictionDirEdit")
@export var _generate_button_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/GenerateButton")
@export var _progress_bar_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/ProgressBar")
@export var _status_label_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/StatusLabel")

@onready var _native_required_notice: Label = get_node_or_null(_native_required_notice_path)
@onready var _modes_row: HFlowContainer = get_node_or_null(_modes_row_path)
@onready var _sample_edit: LineEdit = get_node_or_null(_sample_edit_path)
@onready var _worker_edit: LineEdit = get_node_or_null(_worker_edit_path)
@onready var _output_dir_edit: LineEdit = get_node_or_null(_output_dir_edit_path)
@onready var _eval_predictions_check: CheckBox = get_node_or_null(_eval_predictions_check_path)
@onready var _prediction_dir_edit: LineEdit = get_node_or_null(_prediction_dir_edit_path)
@onready var _generate_button: Button = get_node_or_null(_generate_button_path)
@onready var _progress_bar: ProgressBar = get_node_or_null(_progress_bar_path)
@onready var _status_label: Label = get_node_or_null(_status_label_path)

var _regen_checks: Dictionary[int, CheckBox] = {}

var regen_checks: Dictionary:
	get:
		return _regen_checks

var sample_edit: LineEdit:
	get:
		return _sample_edit

var worker_edit: LineEdit:
	get:
		return _worker_edit

var output_dir_edit: LineEdit:
	get:
		return _output_dir_edit

var eval_predictions_check: CheckBox:
	get:
		return _eval_predictions_check

var prediction_dir_edit: LineEdit:
	get:
		return _prediction_dir_edit

var generate_button: Button:
	get:
		return _generate_button

var progress_bar: ProgressBar:
	get:
		return _progress_bar

var status_label: Label:
	get:
		return _status_label

func _ready() -> void:
	title = "Generate New Data"
	min_size = Vector2i(1000, 900)
	unresizable = false
	close_requested.connect(_on_close_requested)
	
	if _output_dir_edit != null:
		_output_dir_edit.text = "res://stats_output"
	if _eval_predictions_check != null:
		_clear_checkbox_border(_eval_predictions_check)
	if _prediction_dir_edit != null:
		_prediction_dir_edit.text = "res://stats_output_baseline"
	
	# Build mode checkboxes
	if _modes_row != null:
		var sizes: Array = Array(SimConstantsScript.SIMULATION_TEAM_SIZES)
		for sz in sizes:
			var cb := CheckBox.new()
			cb.text = "%dv%d" % [sz, sz]
			cb.custom_minimum_size.y = 56
			cb.button_pressed = true
			cb.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
			_clear_checkbox_border(cb)
			_regen_checks[sz] = cb
			_modes_row.add_child(cb)

func _clear_checkbox_border(cb: CheckBox) -> void:
	var bg := UiStylesScript.solid(UiTokensScript.COLOR_PANEL)
	bg.content_margin_left = 16.0
	bg.content_margin_top = 8.0
	bg.content_margin_right = 16.0
	bg.content_margin_bottom = 8.0
	cb.add_theme_stylebox_override("normal", bg)
	cb.add_theme_stylebox_override("hover", bg)
	cb.add_theme_stylebox_override("pressed", bg)
	cb.add_theme_stylebox_override("hover_pressed", bg)
	cb.add_theme_stylebox_override("disabled", bg)
	cb.add_theme_stylebox_override("focus", StyleBoxEmpty.new())
	cb.add_theme_color_override("font_color", UiTokensScript.COLOR_TEXT)
	cb.add_theme_color_override("font_hover_color", UiTokensScript.COLOR_TEXT)
	cb.add_theme_color_override("font_pressed_color", UiTokensScript.COLOR_TEXT)
	cb.add_theme_color_override("font_hover_pressed_color", UiTokensScript.COLOR_TEXT)
	cb.add_theme_color_override("font_focus_color", UiTokensScript.COLOR_TEXT)
	cb.add_theme_color_override("font_disabled_color", UiTokensScript.COLOR_TEXT)

func show_popup() -> void:
	reset_size()
	popup_centered(Vector2i(1000, 900))

func set_native_ready(ready: bool) -> void:
	if _native_required_notice != null:
		_native_required_notice.visible = not ready

func _on_close_requested() -> void:
	hide()
	closed.emit()
