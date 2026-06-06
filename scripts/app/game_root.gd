extends Node

const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"
const HeadlessRunnerScript := preload("res://scripts/simulation/headless_runner.gd")
const SimulationSchemaScript := preload("res://scripts/simulation/simulation_schema.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")


## True if `flag` was passed after `--` (user args) or anywhere on the command line.
## Without `--`, Godot does not put flags in get_cmdline_user_args(); many people run
## `godot --path proj --stats-dashboard` and expect it to work.
func _argv_has_flag(flag: String) -> bool:
	if OS.get_cmdline_user_args().has(flag):
		return true
	for a in OS.get_cmdline_args():
		if a == flag:
			return true
	return false


func _ready() -> void:
	if _argv_has_flag("--check-only"):
		get_tree().quit()
		return
	if _argv_has_flag("--stats-dashboard"):
		_export_champion_schema()
		call_deferred("_open_stats_dashboard")
		return
	if _argv_has_flag("--simulation-viewer"):
		_export_champion_schema()
		if not NativeSimulationBackendScript.ensure_gdextension_loaded():
			push_error("Failed to load native simulation extension: %s" % NativeExtensionPath)
			return
		call_deferred("_open_simulation_viewer")
		return
	if _argv_has_flag("--headless-run"):
		if not NativeSimulationBackendScript.ensure_gdextension_loaded():
			push_error("Failed to load native simulation extension: %s" % NativeExtensionPath)
			return
		await get_tree().process_frame
		_start_headless_run()
		return

	# Default to main menu
	_export_champion_schema()
	call_deferred("_open_main_menu")


func _open_main_menu() -> void:
	# Create main menu programmatically
	var main_menu := Control.new()
	main_menu.name = "MainMenu"
	
	# Background
	var background := ColorRect.new()
	background.color = Color(0.078, 0.078, 0.102, 1.0)
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	main_menu.add_child(background)
	
	# Container for UI elements
	var vbox := VBoxContainer.new()
	vbox.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	vbox.add_theme_constant_override("separation", 20)
	main_menu.add_child(vbox)
	
	# Title
	var title := Label.new()
	title.text = "Teamfight"
	title.add_theme_color_override("font_color", Color.WHITE)
	title.add_theme_font_size_override("font_size", 48)
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vbox.add_child(title)
	
	# Spacer
	var spacer := Control.new()
	spacer.custom_minimum_size = Vector2(0, 100)
	vbox.add_child(spacer)
	
	# Button container
	var button_container := HBoxContainer.new()
	button_container.add_theme_constant_override("separation", 30)
	vbox.add_child(button_container)
	
	# Simulation Viewer button
	var sim_button := Button.new()
	sim_button.text = "Simulation Viewer"
	sim_button.custom_minimum_size = Vector2(200, 80)
	sim_button.add_theme_color_override("font_color", Color.WHITE)
	sim_button.add_theme_font_size_override("font_size", 18)
	sim_button.pressed.connect(_open_simulation_viewer)
	button_container.add_child(sim_button)
	
	# Stats Dashboard button
	var stats_button := Button.new()
	stats_button.text = "Stats Dashboard"
	stats_button.custom_minimum_size = Vector2(200, 80)
	stats_button.add_theme_color_override("font_color", Color.WHITE)
	stats_button.add_theme_font_size_override("font_size", 18)
	stats_button.pressed.connect(_open_stats_dashboard)
	button_container.add_child(stats_button)

	# Draft Testing button
	var draft_button := Button.new()
	draft_button.text = "Draft Testing"
	draft_button.custom_minimum_size = Vector2(200, 80)
	draft_button.add_theme_color_override("font_color", Color.WHITE)
	draft_button.add_theme_font_size_override("font_size", 18)
	draft_button.pressed.connect(_open_draft_testing)
	button_container.add_child(draft_button)

	# Add to tree
	get_tree().root.add_child(main_menu)
	get_tree().current_scene = main_menu


func _open_stats_dashboard() -> void:
	get_tree().change_scene_to_file("res://scenes/stats_dashboard.tscn")


func _open_simulation_viewer() -> void:
	get_tree().change_scene_to_file("res://scenes/simulation_viewer.tscn")


func _open_draft_testing() -> void:
	get_tree().change_scene_to_file("res://scenes/draft_testing.tscn")


func _start_headless_run() -> void:
	HeadlessRunnerScript.run_from_cli(get_tree())


func _export_champion_schema() -> void:
	var success := SimulationSchemaScript.write_champion_schema_to_file()
	if success:
		print("Champion schema exported successfully")
	else:
		push_error("Failed to export champion schema")
	
	# Test minimal kit generation
	const MinimalKitTest := preload("res://scripts/tools/minimal_kit_test.gd")
	var kit_success := MinimalKitTest.write_minimal_kit_json()
	print("Minimal kit test success: %s" % kit_success)
