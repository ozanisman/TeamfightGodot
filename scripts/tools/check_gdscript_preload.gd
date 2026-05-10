extends SceneTree

## Minimal GDScript compile/preload gate for CI (--check-only).
## Dashboard loader + stats_dashboard scene smoke: --check-stats-dashboard ([check_stats_dashboard_load.gd]).

const AppGameRootScript := preload("res://scripts/app/game_root.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const ChampionSpecScript := preload("res://scripts/simulation/champion_spec.gd")
const ChampionStatsScript := preload("res://scripts/simulation/champion_stats.gd")
const EffectSpecScript := preload("res://scripts/simulation/effect_spec.gd")
const HeadlessRunnerScript := preload("res://scripts/simulation/headless_runner.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const MatchReplaySummaryScript := preload("res://scripts/simulation/match_replay_summary.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
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
const StatsDashboardScript := preload("res://scripts/app/stats_dashboard.gd")
const StatsBarControlScript := preload("res://scripts/app/stats_bar_control.gd")
const StatsChartAxisGuidesScript := preload("res://scripts/app/stats_chart_axis_guides.gd")
const StatsBalanceBarScript := preload("res://scripts/app/stats_balance_bar.gd")
const StatsCsvAggregatorScript := preload("res://scripts/tools/stats_csv_aggregator.gd")
const StatsSimulationCsvGeneratorScript := preload("res://scripts/tools/stats_simulation_csv_generator.gd")
const GenerateSimulationStatsScript := preload("res://scripts/tools/generate_simulation_stats.gd")
const CheckStatsAggregatorRoundtripScript := preload("res://scripts/tools/check_stats_aggregator_roundtrip.gd")
const SimulationBatchWorkerScript := preload("res://scripts/simulation/simulation_batch_worker.gd")
const CheckMatchTelemetryScript := preload("res://scripts/tools/check_match_telemetry.gd")
const CheckStatsCsvDeterminismScript := preload("res://scripts/tools/check_stats_csv_determinism.gd")


func _init() -> void:
	var champion_tooltip: Script = load("res://scripts/app/champion_catalog_tooltip.gd") as Script
	if champion_tooltip == null:
		push_error("champion_catalog_tooltip: load failed (parse or missing res://scripts/app/champion_catalog_tooltip.gd)")
		call_deferred("quit", 1)
		return
	call_deferred("quit", 0)
