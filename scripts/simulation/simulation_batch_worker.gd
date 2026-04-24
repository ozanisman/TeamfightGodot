class_name SimulationBatchWorker
extends RefCounted

static var _bench_mutex: Mutex = Mutex.new()
static var _bench_done: int = 0
static var _bench_target: int = 0
static var _bench_next_milestone: int = 1000
static var _bench_flush_core: Object = null

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

static func reset_benchmark_progress(total_matches: int) -> void:
	if _bench_flush_core == null and ClassDB.can_instantiate(&"TeamfightSimulationCore"):
		_bench_flush_core = ClassDB.instantiate(&"TeamfightSimulationCore")
	_bench_mutex.lock()
	_bench_done = 0
	_bench_target = maxi(0, total_matches)
	_bench_next_milestone = 1000
	_bench_mutex.unlock()

static func record_benchmark_progress(delta_matches: int) -> void:
	if delta_matches <= 0:
		return
	var milestones: Array = []
	var target_snapshot: int = 0
	_bench_mutex.lock()
	_bench_done += delta_matches
	target_snapshot = _bench_target
	while _bench_next_milestone <= _bench_done and _bench_next_milestone <= _bench_target:
		milestones.append(_bench_next_milestone)
		_bench_next_milestone += 1000
	_bench_mutex.unlock()
	for m in milestones:
		if _bench_flush_core != null and _bench_flush_core.has_method(&"benchmark_console_progress"):
			_bench_flush_core.call(&"benchmark_console_progress", m, target_snapshot, false)

func run_chunk(data: Dictionary) -> Array:
	var start_index: int = int(data.get("start_index", 0))
	var end_index: int = int(data.get("end_index", 0))
	var team_size: int = int(data.get("team_size", 1))
	var base_seed: int = int(data.get("base_seed", 0))
	var bench_skip_summaries: bool = bool(data.get("bench_skip_summaries", false))
	var allow_native_batch: bool = bool(data.get("allow_native_batch", false))
	var backend: Object = NativeSimulationBackendScript.new()
	if not backend.is_available():
		return []

	var chunk_len: int = maxi(0, end_index - start_index)
	var results: Array = []
	results.resize(chunk_len)

	if bench_skip_summaries and allow_native_batch and backend.has_method("run_matches_simulation_only"):
		var inputs: Array = []
		inputs.resize(chunk_len)
		var input_index: int = 0
		for match_index in range(start_index, end_index):
			inputs[input_index] = _build_batch_input_for_seed(base_seed + match_index, team_size)
			input_index += 1
		backend.run_matches_simulation_only(inputs)
		for i in range(chunk_len):
			results[i] = true
		if backend.has_method("clear"):
			backend.call("clear")
		return results

	var result_index: int = 0
	for match_index in range(start_index, end_index):
		var batch_match_input = _build_batch_input_for_seed(base_seed + match_index, team_size)
		if bench_skip_summaries and backend.has_method("run_match_simulation_only"):
			backend.run_match_simulation_only(batch_match_input)
			results[result_index] = true
		else:
			var summary = _summary_object(backend.run_match(batch_match_input))
			results[result_index] = summary.to_dict() if summary is Object and summary.has_method("to_dict") else summary
		result_index += 1
		if result_index % 1000 == 0:
			record_benchmark_progress(1000)
		if backend.has_method("clear"):
			backend.call("clear")

	var tail: int = chunk_len % 1000
	if tail != 0:
		record_benchmark_progress(tail)

	return results
