class_name ExportPopup
extends Window

const UiStylesScript := preload("res://scripts/ui/ui_styles.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

signal closed

@onready var _native_required_notice: Label = get_node_or_null("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/NativeRequiredNotice")
@onready var _modes_row: HFlowContainer = get_node_or_null("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/ModesRow")
@onready var _sample_edit: LineEdit = get_node_or_null("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/SampleBlock/SampleEdit")
@onready var _worker_edit: LineEdit = get_node_or_null("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/WorkerBlock/WorkerEdit")
@onready var _eval_predictions_check: CheckBox = get_node_or_null("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/PredictionBlock/EvalPredictionsCheck")
@onready var _prediction_dir_edit: LineEdit = get_node_or_null("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/PredictionBlock/PredictionDirEdit")
@onready var _generate_button: Button = get_node_or_null("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/GenerateButton")
@onready var _progress_bar: ProgressBar = get_node_or_null("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/ProgressBar")
@onready var _status_label: Label = get_node_or_null("ScrollContainer/VBoxContainer/MarginContainer/PanelContainer/VBoxContainer/StatusLabel")

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
	
	# Build mode checkboxes
	if _modes_row != null:
		var sizes: Array = Array(SimConstantsScript.SIMULATION_TEAM_SIZES)
		for sz in sizes:
			var cb := CheckBox.new()
			cb.text = "%dv%d" % [sz, sz]
			cb.custom_minimum_size.y = 56
			cb.button_pressed = true
			cb.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
			_regen_checks[sz] = cb
			_modes_row.add_child(cb)

func show_popup() -> void:
	reset_size()
	popup_centered(Vector2i(1000, 900))

func set_native_ready(ready: bool) -> void:
	if _native_required_notice != null:
		_native_required_notice.visible = not ready

func _on_close_requested() -> void:
	hide()
	closed.emit()
