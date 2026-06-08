extends SceneTree

## Standalone script to run signal variance analysis on existing stats
## Usage: godot --script scripts/tools/run_signal_variance_analysis.gd --stats-dir=res://stats_output_baseline --output=res://stats_output_baseline/signal_variance.csv

const AnalyzeSignalVarianceScript := preload("res://scripts/tools/analyze_signal_variance.gd")

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	print("Signal Variance Analysis Runner")
	print("==============================")

	# Use hardcoded defaults for testing
	var stats_dir := "res://stats_output_baseline"
	var output_path := "res://stats_output_baseline/signal_variance.csv"
	var team_sizes: Array = [5]
	var min_samples := 20

	print("Stats dir: %s" % stats_dir)
	print("Output path: %s" % output_path)
	print("Team sizes: %s" % str(team_sizes))
	print("Min samples: %d" % min_samples)

	print("Loading analyzer script...")
	var analyzer_script := load("res://scripts/tools/analyze_signal_variance.gd")
	if analyzer_script == null:
		push_error("Failed to load analyze_signal_variance.gd")
		quit(1)
		return

	print("Creating analyzer instance...")
	var analyzer: RefCounted = analyzer_script.new()
	print("Running analysis...")
	var result: Dictionary = analyzer.analyze(stats_dir, team_sizes, min_samples, output_path)
	print("Result: %s" % str(result))

	print("Analysis complete")
	quit(0)
