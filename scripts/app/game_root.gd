extends Node

const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"
const HeadlessRunnerScript := preload("res://scripts/simulation/headless_runner.gd")


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
		call_deferred("_open_stats_dashboard")
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


func _start_headless_run() -> void:
	HeadlessRunnerScript.run_from_cli(get_tree())
