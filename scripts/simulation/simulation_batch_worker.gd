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
const StatsCsvAggregatorScript := preload("res://scripts/tools/stats_csv_aggregator.gd")


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


static func _matchup_wall_ns_from_summary(summary: Variant, matchup_tracker: Object) -> int:
	var t_matchup_ns: int = _now_ns()
	if summary is Dictionary and matchup_tracker != null:
		var summary_dict: Dictionary = Dictionary(summary)
		var winners: Array[StringName] = []
		var losers: Array[StringName] = []
		var winner_team: StringName = StringName(String(summary_dict.get("winner_team", "")))
		if winner_team == &"player":
			winners = _string_name_array(Array(summary_dict.get("player_comp", [])))
			losers = _string_name_array(Array(summary_dict.get("enemy_comp", [])))
		elif winner_team == &"enemy":
			winners = _string_name_array(Array(summary_dict.get("enemy_comp", [])))
			losers = _string_name_array(Array(summary_dict.get("player_comp", [])))
		if not winners.is_empty() and not losers.is_empty():
			matchup_tracker.process_match_result(winners, losers)
	return _now_ns() - t_matchup_ns


func run_chunk(data: Dictionary) -> Array:
	var chunk_start_ns: int = _now_ns()
	var start_index: int = int(data.get("start_index", 0))
	var end_index: int = int(data.get("end_index", 0))
	var team_size: int = int(data.get("team_size", 1))
	var base_seed: int = int(data.get("base_seed", 0))
	var bench_skip_summaries: bool = bool(data.get("bench_skip_summaries", false))
	var allow_native_batch: bool = bool(data.get("allow_native_batch", false))
	var profile_stats: bool = bool(data.get("profile_stats", false))
	var aggregate_stats_in_worker: bool = bool(data.get("aggregate_stats_in_worker", false))
	var write_match_log: bool = bool(data.get("write_match_log", true))
	var skip_catalog_thread_clear: bool = bool(data.get("skip_catalog_thread_clear", false))
	var chunk_profile: Dictionary = {}

	var cache_clear_ns: int = 0
	var catalog_build_ns: int = 0
	var matchup_init_ns: int = 0
	var archetypes_ns: int = 0
	var results_init_ns: int = 0
	var stats_setup_ns: int = 0
	var matchup_tracker = null
	var archetypes: Array[StringName] = []
	
	var t_phase_ns: int = _now_ns()
	var backend: Object = NativeSimulationBackendScript.new()
	var backend_create_ns: int = _now_ns() - t_phase_ns
	
	t_phase_ns = _now_ns()
	if not backend.is_available():
		return []
	var backend_available_ns: int = _now_ns() - t_phase_ns

	var chunk_len: int = maxi(0, end_index - start_index)
	var results: Array = []
	results.resize(chunk_len)
	var use_compact_stats: bool = backend.has_method("run_match_stats")

	if bench_skip_summaries and allow_native_batch and backend.has_method("run_generated_matches_simulation_only"):
		# Benchmark-only fast path: no catalog build, no matchups, no per-match input assembly.
		var t_native_batch_ns: int = _now_ns()
		backend.run_generated_matches_simulation_only(base_seed + start_index, chunk_len, team_size)
		var native_batch_ns: int = _now_ns() - t_native_batch_ns
		for i in range(chunk_len):
			results[i] = true
		var t_native_clear_ns: int = _now_ns()
		backend.clear()
		var native_clear_ns: int = _now_ns() - t_native_clear_ns
		if profile_stats:
			return [{
				"match_results": results,
				"profile_stats": {
					"path": "generated_native_batch",
					"start_index": start_index,
					"end_index": end_index,
					"team_size": team_size,
					"base_seed": base_seed,
					"match_count": chunk_len,
					"cache_clear_ns": cache_clear_ns,
					"catalog_build_ns": catalog_build_ns,
					"matchup_init_ns": matchup_init_ns,
					"backend_create_ns": backend_create_ns,
					"backend_available_ns": backend_available_ns,
					"archetypes_ns": archetypes_ns,
					"results_init_ns": results_init_ns,
					"stats_setup_ns": stats_setup_ns,
					"assembly_ns": 0,
					"native_run_ns": native_batch_ns,
					"matchup_ns": 0,
					"clear_ns": native_clear_ns,
					"wall_ns": _now_ns() - chunk_start_ns,
				},
			}]
		return results

	t_phase_ns = _now_ns()
	if not skip_catalog_thread_clear:
		ChampionCatalogScript.clear_thread_cache()
	cache_clear_ns = _now_ns() - t_phase_ns

	t_phase_ns = _now_ns()
	ChampionCatalogScript.build_catalog()
	catalog_build_ns = _now_ns() - t_phase_ns
	
	t_phase_ns = _now_ns()
	matchup_tracker = MatchupTrackerScript.new()
	matchup_init_ns = _now_ns() - t_phase_ns
	
	t_phase_ns = _now_ns()
	archetypes = ChampionCatalogScript.get_champion_ids()
	archetypes_ns = _now_ns() - t_phase_ns
	var players: Array[StringName] = []
	var enemies: Array[StringName] = []
	
	t_phase_ns = _now_ns()
	results_init_ns = _now_ns() - t_phase_ns
	
	t_phase_ns = _now_ns()
	var stats_aggregator = null
	if aggregate_stats_in_worker and not bench_skip_summaries:
		stats_aggregator = StatsCsvAggregatorScript.new()
		stats_aggregator.set_write_match_log(write_match_log)
		stats_aggregator.reset()
		var preload_roles_variant: Variant = data.get("role_by_hero_map", null)
		if preload_roles_variant is Dictionary and not (preload_roles_variant as Dictionary).is_empty():
			stats_aggregator.preload_roles(preload_roles_variant as Dictionary)
	stats_setup_ns = _now_ns() - t_phase_ns

	if bench_skip_summaries and allow_native_batch and backend.has_method("run_matches_simulation_only"):
		var inputs: Array = []
		inputs.resize(chunk_len)
		var input_index: int = 0
		var input_build_ns: int = 0
		for match_index in range(start_index, end_index):
			var t_input_build_ns: int = _now_ns()
			inputs[input_index] = _build_batch_input_for_seed(base_seed + match_index, team_size, archetypes, players, enemies)
			input_build_ns += _now_ns() - t_input_build_ns
			input_index += 1
		var t_native_inputs_ns: int = _now_ns()
		backend.run_matches_simulation_only(inputs)
		var native_inputs_ns: int = _now_ns() - t_native_inputs_ns
		for i in range(chunk_len):
			results[i] = true
		var t_clear_inputs_ns: int = _now_ns()
		backend.clear()
		var clear_inputs_ns: int = _now_ns() - t_clear_inputs_ns
		if profile_stats:
			return [{
				"match_results": results,
				"profile_stats": {
					"path": "input_native_batch",
					"start_index": start_index,
					"end_index": end_index,
					"team_size": team_size,
					"base_seed": base_seed,
					"match_count": chunk_len,
					"cache_clear_ns": cache_clear_ns,
					"catalog_build_ns": catalog_build_ns,
					"matchup_init_ns": matchup_init_ns,
					"backend_create_ns": backend_create_ns,
					"backend_available_ns": backend_available_ns,
					"archetypes_ns": archetypes_ns,
					"results_init_ns": results_init_ns,
					"stats_setup_ns": stats_setup_ns,
					"assembly_ns": input_build_ns,
					"native_run_ns": native_inputs_ns,
					"matchup_ns": 0,
					"clear_ns": clear_inputs_ns,
					"wall_ns": _now_ns() - chunk_start_ns,
				},
			}]
		return results

	var assembly_ns: int = 0
	var native_run_ns: int = 0
	var matchup_ns: int = 0
	var clear_ns: int = 0
	var result_index: int = 0
	var batch_stats_via_native: bool = (
		use_compact_stats
		and (not bench_skip_summaries)
		and backend.has_method("run_matches_stats")
	)

	if batch_stats_via_native:
		var batch_inputs: Array = []
		batch_inputs.resize(chunk_len)
		var slot: int = 0
		for match_index in range(start_index, end_index):
			var t_asm: int = _now_ns()
			batch_inputs[slot] = _build_batch_input_for_seed(
				base_seed + match_index, team_size, archetypes, players, enemies
			)
			assembly_ns += _now_ns() - t_asm
			slot += 1
		var t_native_total: int = _now_ns()
		var summaries_var: Variant = backend.run_matches_stats(batch_inputs)
		native_run_ns += _now_ns() - t_native_total
		if typeof(summaries_var) != TYPE_ARRAY:
			return []
		var summaries_arr: Array = summaries_var
		if summaries_arr.size() != chunk_len:
			return []
		result_index = 0
		for summary in summaries_arr:
			if aggregate_stats_in_worker:
				stats_aggregator.consume_summary(team_size, summary)
			else:
				results[result_index] = summary
			matchup_ns += _matchup_wall_ns_from_summary(summary, matchup_tracker)
			result_index += 1
			if result_index % 1000 == 0:
				record_benchmark_progress(1000)
	else:
		for match_index in range(start_index, end_index):
			var t_assembly_ns: int = _now_ns()
			var batch_match_input = _build_batch_input_for_seed(
				base_seed + match_index, team_size, archetypes, players, enemies
			)
			assembly_ns += _now_ns() - t_assembly_ns
			if bench_skip_summaries and backend.has_method("run_match_simulation_only"):
				backend.run_match_simulation_only(batch_match_input)
				results[result_index] = true
			else:
				var t_native_run_ns: int = _now_ns()
				var summary = (
					backend.run_match_stats(batch_match_input)
					if use_compact_stats
					else backend.run_match(batch_match_input)
				)
				native_run_ns += _now_ns() - t_native_run_ns
				if aggregate_stats_in_worker:
					stats_aggregator.consume_summary(team_size, summary)
				else:
					results[result_index] = summary
				matchup_ns += _matchup_wall_ns_from_summary(summary, matchup_tracker)

			result_index += 1
			if result_index % 1000 == 0:
				record_benchmark_progress(1000)
			var t_clear_ns_loop: int = _now_ns()
			if backend.has_method("clear"):
				backend.call("clear")
			clear_ns += _now_ns() - t_clear_ns_loop

	var tail: int = chunk_len % 1000
	if tail != 0:
		record_benchmark_progress(tail)

	# Include matchup data in results if not in benchmark mode
	if not bench_skip_summaries:
		var chunk_result = {"matchup_data": matchup_tracker.get_matchup_data()}
		if aggregate_stats_in_worker:
			chunk_result["stats_partial"] = stats_aggregator.to_partial_dict(write_match_log)
		else:
			chunk_result["match_results"] = results
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
				"cache_clear_ns": cache_clear_ns,
				"catalog_build_ns": catalog_build_ns,
				"matchup_init_ns": matchup_init_ns,
				"backend_create_ns": backend_create_ns,
				"backend_available_ns": backend_available_ns,
				"archetypes_ns": archetypes_ns,
				"results_init_ns": results_init_ns,
				"stats_setup_ns": stats_setup_ns,
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
