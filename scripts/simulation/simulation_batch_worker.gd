class_name SimulationBatchWorker
extends RefCounted

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const MatchReplaySummaryScript := preload("res://scripts/simulation/match_replay_summary.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

static func _summary_object(summary_value: Variant):
	if summary_value is Object:
		return summary_value
	if summary_value is Dictionary:
		return MatchReplaySummaryScript.from_dict(Dictionary(summary_value))
	return MatchReplaySummaryScript.new()

static func _build_batch_input_for_seed(match_seed: int, team_size: int):
	var rng = RandomNumberGenerator.new()
	rng.seed = match_seed
	var archetypes = ChampionCatalogScript.get_champion_ids()
	var players: Array[StringName] = []
	var enemies: Array[StringName] = []
	if archetypes.size() < team_size * 2:
		for _i in range(team_size):
			players.append(archetypes[rng.randi_range(0, archetypes.size() - 1)])
		for _i in range(team_size):
			enemies.append(archetypes[rng.randi_range(0, archetypes.size() - 1)])
	else:
		var pool := archetypes.duplicate()
		pool.shuffle()
		for i in range(team_size):
			players.append(pool[i])
		for i in range(team_size, team_size * 2):
			enemies.append(pool[i])
	return MatchReplayInputScript.build_match_input(match_seed, players, enemies, SimConstantsScript.SIMULATION_TICK_RATE)

func run_chunk(data: Dictionary) -> Array:
	var start_index: int = int(data.get("start_index", 0))
	var end_index: int = int(data.get("end_index", 0))
	var team_size: int = int(data.get("team_size", 1))
	var base_seed: int = int(data.get("base_seed", 0))
	var backend: Object = NativeSimulationBackendScript.new()
	if not backend.is_available():
		return []

	var results: Array = []
	results.resize(maxi(0, end_index - start_index))
	var result_index: int = 0
	for match_index in range(start_index, end_index):
		var batch_match_input = _build_batch_input_for_seed(base_seed + match_index, team_size)
		var summary = _summary_object(backend.run_match(batch_match_input))
		results[result_index] = summary.to_dict() if summary is Object and summary.has_method("to_dict") else summary
		result_index += 1
		if backend.has_method("clear"):
			backend.call("clear")

	return results
