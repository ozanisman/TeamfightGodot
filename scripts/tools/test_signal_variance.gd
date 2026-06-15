extends SceneTree

## Quick test script for signal variance analysis
## Run via: godot --headless --script scripts/tools/test_signal_variance.gd

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	var log_path := "res://logs/test_log.txt"
	var f := FileAccess.open(ProjectSettings.globalize_path(log_path), FileAccess.WRITE)
	f.store_string("Starting test...\n")
	f.flush()

	f.store_string("Loading script...\n")
	f.flush()

	var analyzer_script := load("res://scripts/tools/analyze_signal_variance.gd")
	if analyzer_script == null:
		f.store_string("ERROR: Failed to load script\n")
		f.flush()
		f.close()
		quit(1)
		return

	f.store_string("Script loaded successfully\n")
	f.flush()

	f.store_string("Creating instance...\n")
	f.flush()

	var analyzer: RefCounted = analyzer_script.new()
	f.store_string("Instance created\n")
	f.flush()

	f.store_string("Calling analyze...\n")
	f.flush()

	var result: Dictionary = analyzer.analyze("res://stats_output_baseline", [5], 20, "res://stats_output_baseline/signal_variance.csv")

	f.store_string("Analysis complete\n")
	f.flush()

	f.store_string("Profiles: %d\n" % result["profiles"].size())
	f.store_string("Flat profiles: %d\n" % result["flat_profiles"].size())
	f.flush()
	f.close()

	quit(0)
