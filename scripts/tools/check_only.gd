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

func _init() -> void:
	# quit() from _init runs before the main loop is ready and can leave a headless process stuck.
	call_deferred("quit", 0)
