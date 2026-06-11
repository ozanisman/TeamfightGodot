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
@export var _eval_predictions_check_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/PredictionBlock/EvalPredictionsCheck")
@export var _prediction_dir_edit_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/PredictionBlock/PredictionDirEdit")
@export var _generate_button_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/GenerateButton")
@export var _progress_bar_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/ProgressBar")
@export var _status_label_path: NodePath = NodePath("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/StatusLabel")

var _native_required_notice: Label
var _modes_row: HFlowContainer
var _sample_edit: LineEdit
var _worker_edit: LineEdit
var _eval_predictions_check: CheckBox
var _prediction_dir_edit: LineEdit
var _generate_button: Button
var _progress_bar: ProgressBar
var _status_label: Label

var _regen_checks: Dictionary = {}

var regen_checks: Dictionary:
	get:
		return _regen_checks

var sample_edit: LineEdit:
	get:
		return _sample_edit

var worker_edit: LineEdit:
	get:
		return _worker_edit

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
	
	# Initialize node references
	if has_node(_native_required_notice_path):
		_native_required_notice = get_node(_native_required_notice_path)
	if has_node(_modes_row_path):
		_modes_row = get_node(_modes_row_path)
	if has_node(_sample_edit_path):
		_sample_edit = get_node(_sample_edit_path)
	if has_node(_worker_edit_path):
		_worker_edit = get_node(_worker_edit_path)
	if has_node(_eval_predictions_check_path):
		_eval_predictions_check = get_node(_eval_predictions_check_path)
		_clear_checkbox_border(_eval_predictions_check)
	if has_node(_prediction_dir_edit_path):
		_prediction_dir_edit = get_node(_prediction_dir_edit_path)
		_prediction_dir_edit.text = "res://stats_output_baseline"
	if has_node(_generate_button_path):
		_generate_button = get_node(_generate_button_path)
	if has_node(_progress_bar_path):
		_progress_bar = get_node(_progress_bar_path)
	if has_node(_status_label_path):
		_status_label = get_node(_status_label_path)
	
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
