class_name SimulationBatchWorker
extends RefCounted

static var _bench_flush_core: Object = null

static func set_sim_profile_enabled(on: bool) -> void:
	if _bench_flush_core == null and ClassDB.can_instantiate(&"TeamfightSimulationCore"):
		_bench_flush_core = ClassDB.instantiate(&"TeamfightSimulationCore")
	if _bench_flush_core != null and _bench_flush_core.has_method(&"sim_profile_set_enabled"):
		_bench_flush_core.call(&"sim_profile_set_enabled", on)


static func release_benchmark_handles() -> void:
	if _bench_flush_core != null and _bench_flush_core.has_method(&"flush_stdio"):
		_bench_flush_core.call(&"flush_stdio")
	_bench_flush_core = null

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const MatchupTrackerScript := preload("res://scripts/simulation/matchup_tracker.gd")


static func _now_ns() -> int:
	return Time.get_ticks_usec() * 1000


static func _dominant_phase(phases: Dictionary) -> String:
	var best_name: String = ""
	var best_ns: int = -1
	for key in phases.keys():
		var value_ns: int = int(phases[key])
		if value_ns > best_ns:
			best_ns = value_ns
			best_name = String(key)
	return best_name


static func _string_name_array(values: Array) -> Array[StringName]:
	var out: Array[StringName] = []
	for value in values:
		out.append(StringName(String(value)))
	return out

static func _build_batch_input_for_seed(
	match_seed: int,
	team_size: int,
	archetypes: Array[StringName],
	players: Array[StringName],
	enemies: Array[StringName]
):
	var rng = RandomNumberGenerator.new()
	rng.seed = match_seed
	players.clear()
	enemies.clear()
	if archetypes.size() < team_size * 2:
		for _i in range(team_size):
			players.append(archetypes[rng.randi_range(0, archetypes.size() - 1)])
		for _i in range(team_size):
			enemies.append(archetypes[rng.randi_range(0, archetypes.size() - 1)])
	else:
		var indices: Array[int] = []
		for i in range(archetypes.size()):
			indices.append(i)
		indices.shuffle()
		for i in range(team_size):
			players.append(archetypes[indices[i]])
		for i in range(team_size, team_size * 2):
			enemies.append(archetypes[indices[i]])
	return MatchReplayInputScript.build_match_input(match_seed, players, enemies, SimConstantsScript.DEFAULT_TICK_RATE, false, false)

static func reset_benchmark_progress(total_matches: int) -> void:
	if _bench_flush_core == null and ClassDB.can_instantiate(&"TeamfightSimulationCore"):
		_bench_flush_core = ClassDB.instantiate(&"TeamfightSimulationCore")
	if _bench_flush_core != null and _bench_flush_core.has_method(&"benchmark_progress_reset"):
		_bench_flush_core.call(&"benchmark_progress_reset", maxi(0, total_matches))

static func record_benchmark_progress(delta_matches: int) -> void:
	if delta_matches <= 0:
		return
	if _bench_flush_core == null and ClassDB.can_instantiate(&"TeamfightSimulationCore"):
		_bench_flush_core = ClassDB.instantiate(&"TeamfightSimulationCore")
	if _bench_flush_core != null and _bench_flush_core.has_method(&"benchmark_progress_add"):
		_bench_flush_core.call(&"benchmark_progress_add", delta_matches)

static func benchmark_progress_read_value() -> int:
	# Same lazy init as record_benchmark_progress: read must hit the process-wide native counter
	# even if reset_benchmark_progress failed to create the handle (workers still update the atomic).
	if _bench_flush_core == null and ClassDB.can_instantiate(&"TeamfightSimulationCore"):
		_bench_flush_core = ClassDB.instantiate(&"TeamfightSimulationCore")
	if _bench_flush_core == null:
		return 0
	if _bench_flush_core.has_method(&"benchmark_progress_read"):
		return int(_bench_flush_core.call(&"benchmark_progress_read"))
	return 0


static func flush_stdio_if_available() -> void:
	if _bench_flush_core != null and _bench_flush_core.has_method(&"flush_stdio"):
		_bench_flush_core.call(&"flush_stdio")

func run_chunk(data: Dictionary) -> Array:
	# Clear thread cache to ensure fresh data for each chunk
	ChampionCatalogScript.clear_thread_cache()
	
	# Pre-initialize champion catalog in thread context
	ChampionCatalogScript.build_catalog()
	
	# Initialize matchup tracker for this chunk
	var matchup_tracker = MatchupTrackerScript.new()
	
	var start_index: int = int(data.get("start_index", 0))
	var end_index: int = int(data.get("end_index", 0))
	var team_size: int = int(data.get("team_size", 1))
	var base_seed: int = int(data.get("base_seed", 0))
	var bench_skip_summaries: bool = bool(data.get("bench_skip_summaries", false))
	var allow_native_batch: bool = bool(data.get("allow_native_batch", false))
	var profile_stats: bool = bool(data.get("profile_stats", false))
	var chunk_start_ns: int = _now_ns()
	var chunk_profile: Dictionary = {}
	
	# Match summaries come from native only (telemetry lives under unit_stats[].telemetry).
	var backend: Object = NativeSimulationBackendScript.new()
	
	if not backend.is_available():
		return []

	var chunk_len: int = maxi(0, end_index - start_index)
	var archetypes: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	var players: Array[StringName] = []
	var enemies: Array[StringName] = []
	var results: Array = []
	results.resize(chunk_len)
	var use_compact_stats: bool = backend.has_method("run_match_stats")

	if bench_skip_summaries and allow_native_batch and backend.has_method("run_generated_matches_simulation_only"):
		backend.run_generated_matches_simulation_only(base_seed + start_index, chunk_len, team_size)
		for i in range(chunk_len):
			results[i] = true
		backend.clear()
		return results

	if bench_skip_summaries and allow_native_batch and backend.has_method("run_matches_simulation_only"):
		var inputs: Array = []
		inputs.resize(chunk_len)
		var input_index: int = 0
		for match_index in range(start_index, end_index):
			inputs[input_index] = _build_batch_input_for_seed(base_seed + match_index, team_size, archetypes, players, enemies)
			input_index += 1
		backend.run_matches_simulation_only(inputs)
		for i in range(chunk_len):
			results[i] = true
		backend.clear()
		return results

	var assembly_ns: int = 0
	var native_run_ns: int = 0
	var matchup_ns: int = 0
	var clear_ns: int = 0
	var result_index: int = 0
	for match_index in range(start_index, end_index):
		var t_assembly_ns: int = _now_ns()
		var batch_match_input = _build_batch_input_for_seed(base_seed + match_index, team_size, archetypes, players, enemies)
		assembly_ns += _now_ns() - t_assembly_ns
		if bench_skip_summaries and backend.has_method("run_match_simulation_only"):
			backend.run_match_simulation_only(batch_match_input)
			results[result_index] = true
		else:
			var t_native_run_ns: int = _now_ns()
			var summary = backend.run_match_stats(batch_match_input) if use_compact_stats else backend.run_match(batch_match_input)
			native_run_ns += _now_ns() - t_native_run_ns
			results[result_index] = summary
			
			# Process matchup data from this match result
			if summary is Dictionary:
				var t_matchup_ns: int = _now_ns()
				var winners: Array[StringName] = []
				var losers: Array[StringName] = []
				var summary_dict: Dictionary = Dictionary(summary)
				
				# Determine winners and losers based on winner_team
				var winner_team: StringName = StringName(String(summary_dict.get("winner_team", "")))
				if winner_team == &"player":
					winners = _string_name_array(Array(summary_dict.get("player_comp", [])))
					losers = _string_name_array(Array(summary_dict.get("enemy_comp", [])))
				elif winner_team == &"enemy":
					winners = _string_name_array(Array(summary_dict.get("enemy_comp", [])))
					losers = _string_name_array(Array(summary_dict.get("player_comp", [])))
				
				# Only process if we have valid winners and losers
				if not winners.is_empty() and not losers.is_empty():
					matchup_tracker.process_match_result(winners, losers)
				matchup_ns += _now_ns() - t_matchup_ns
		
		result_index += 1
		if result_index % 1000 == 0:
			record_benchmark_progress(1000)
		var t_clear_ns: int = _now_ns()
		if backend.has_method("clear"):
			backend.call("clear")
		clear_ns += _now_ns() - t_clear_ns

	var tail: int = chunk_len % 1000
	if tail != 0:
		record_benchmark_progress(tail)

	# Include matchup data in results if not in benchmark mode
	if not bench_skip_summaries:
		var chunk_result = {
			"match_results": results,
			"matchup_data": matchup_tracker.get_matchup_data()
		}
		if profile_stats:
			var total_ns: int = assembly_ns + native_run_ns + matchup_ns + clear_ns
			chunk_profile = {
				"start_index": start_index,
				"end_index": end_index,
				"team_size": team_size,
				"base_seed": base_seed,
				"match_count": chunk_len,
				"assembly_ns": assembly_ns,
				"assembly_pct": 100.0 * float(assembly_ns) / float(total_ns) if total_ns > 0 else 0.0,
				"native_run_ns": native_run_ns,
				"native_run_pct": 100.0 * float(native_run_ns) / float(total_ns) if total_ns > 0 else 0.0,
				"matchup_ns": matchup_ns,
				"matchup_pct": 100.0 * float(matchup_ns) / float(total_ns) if total_ns > 0 else 0.0,
				"clear_ns": clear_ns,
				"clear_pct": 100.0 * float(clear_ns) / float(total_ns) if total_ns > 0 else 0.0,
				"dominant_phase": _dominant_phase({
					"assembly_ns": assembly_ns,
					"native_run_ns": native_run_ns,
					"matchup_ns": matchup_ns,
					"clear_ns": clear_ns,
				}),
				"avg_ns_per_match": float(total_ns) / float(chunk_len) if chunk_len > 0 else 0.0,
				"wall_ns": _now_ns() - chunk_start_ns,
				"wall_pct": 100.0,
			}
			chunk_result["profile_stats"] = chunk_profile
		return [chunk_result]

	return results
