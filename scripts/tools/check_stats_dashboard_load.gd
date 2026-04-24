extends SceneTree

## Focused gate: CSV loader + stats_dashboard scene parses, instantiates, runs _ready in tree.
## Run: .\run_godot.ps1 -- --check-stats-dashboard

const StatsDashboardLoaderScript := preload("res://scripts/tools/stats_dashboard_loader.gd")
const StatsDashboardScript := preload("res://scripts/app/stats_dashboard.gd")
const StatsBarControlScript := preload("res://scripts/app/stats_bar_control.gd")
const StatsDoughnutScript := preload("res://scripts/app/stats_doughnut.gd")

func _init() -> void:
	var _compile_bar := StatsBarControlScript
	var _compile_doughnut := StatsDoughnutScript
	var _compile_panel := StatsDashboardScript
	var loader := StatsDashboardLoaderScript.new()
	if loader.load_from_dir("res://fixtures/stats_dashboard") != OK:
		push_error("StatsDashboardLoader fixture load failed")
		call_deferred("quit", 1)
		return
	call_deferred("_stats_dashboard_enter_tree_smoke")


func _stats_dashboard_enter_tree_smoke() -> void:
	var sc: PackedScene = load("res://scenes/stats_dashboard.tscn") as PackedScene
	if sc == null:
		push_error("stats_dashboard: PackedScene load failed")
		quit(1)
		return
	var inst: Node = sc.instantiate()
	if inst == null:
		push_error("stats_dashboard: instantiate failed")
		quit(1)
		return
	root.add_child(inst)
	await process_frame
	await process_frame
	if not is_instance_valid(inst):
		push_error("stats_dashboard: node invalid after frames")
		quit(1)
		return
	inst.queue_free()
	quit(0)
