extends SceneTree

const AppGameRootScript := preload("res://scripts/app/game_root.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const ChampionSpecScript := preload("res://scripts/simulation/champion_spec.gd")
const ChampionStatsScript := preload("res://scripts/simulation/champion_stats.gd")
const EffectSpecScript := preload("res://scripts/simulation/effect_spec.gd")
const HeadlessRunnerScript := preload("res://scripts/simulation/headless_runner.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const MatchReplaySummaryScript := preload("res://scripts/simulation/match_replay_summary.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const TeamfightSimulationCoreScript := preload("res://scripts/simulation/teamfight_simulation_core.gd")
const CheckDeterminismScript := preload("res://scripts/tools/check_determinism.gd")
const CheckBenchmarkScript := preload("res://scripts/tools/check_benchmark.gd")
const ParityToolsScript := preload("res://scripts/simulation/parity_tools.gd")
const ReplayIOScript := preload("res://scripts/simulation/replay_io.gd")
const RoleConfigSpecScript := preload("res://scripts/simulation/role_config_spec.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const SimulationBackendScript := preload("res://scripts/simulation/simulation_backend.gd")
const SimulationSchemaScript := preload("res://scripts/simulation/simulation_schema.gd")
const SpawnSpecScript := preload("res://scripts/simulation/spawn_spec.gd")
const UnitReplaySummaryScript := preload("res://scripts/simulation/unit_replay_summary.gd")
const GameRootScene := preload("res://scenes/game_root.tscn")
const StatsDashboardLoaderScript := preload("res://scripts/tools/stats_dashboard_loader.gd")
const StatsDashboardScript := preload("res://scripts/app/stats_dashboard.gd")
const StatsBarControlScript := preload("res://scripts/app/stats_bar_control.gd")
const StatsDoughnutScript := preload("res://scripts/app/stats_doughnut.gd")

func _init() -> void:
	var _compile_bar := StatsBarControlScript
	var _compile_doughnut := StatsDoughnutScript
	var _compile_stats_dashboard := StatsDashboardScript
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
