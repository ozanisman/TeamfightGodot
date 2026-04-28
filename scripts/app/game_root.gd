extends Node

const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"
const HeadlessRunnerScript := preload("res://scripts/simulation/headless_runner.gd")
const SimulationSchemaScript := preload("res://scripts/simulation/simulation_schema.gd")


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
		var load_status: int = GDExtensionManager.load_extension(NativeExtensionPath)
		if (
			load_status != GDExtensionManager.LOAD_STATUS_OK
			and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED
		):
			push_error("Failed to load native simulation extension: %s" % NativeExtensionPath)
		call_deferred("_open_simulation_viewer")
		return
	if _argv_has_flag("--headless-run"):
		var load_status: int = GDExtensionManager.load_extension(NativeExtensionPath)
		if (
			load_status != GDExtensionManager.LOAD_STATUS_OK
			and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED
		):
			push_error("Failed to load native simulation extension: %s" % NativeExtensionPath)
		await get_tree().process_frame
		_start_headless_run()
		return

	# Presentation layer will attach here once the compiled simulation core exists.


func _open_stats_dashboard() -> void:
	get_tree().change_scene_to_file("res://scenes/stats_dashboard.tscn")


func _open_simulation_viewer() -> void:
	get_tree().change_scene_to_file("res://scenes/simulation_viewer.tscn")


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
