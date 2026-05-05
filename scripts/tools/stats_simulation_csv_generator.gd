class_name StatsSimulationCsvGenerator
extends RefCounted

## Runs native batch matches and writes stats CSVs via [StatsCsvAggregator].
## [param max_worker_threads]: 0 = auto (min of matches, CPU count, [constant DEFAULT_EXPORT_MAX_WORKER_THREADS]); else cap parallel workers.

const SimulationBatchWorkerScript := preload("res://scripts/simulation/simulation_batch_worker.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const StatsCsvAggregatorScript := preload("res://scripts/tools/stats_csv_aggregator.gd")

## Upper bound on parallel export workers (avoids huge thread counts on high-core CPUs).
const DEFAULT_EXPORT_MAX_WORKER_THREADS: int = 16


static func _now_ns() -> int:
	return Time.get_ticks_usec() * 1000


static func _emit_profile_line(payload: Dictionary) -> void:
	printerr(JSON.stringify(payload))


static func _profile_percent(part_ns: int, total_ns: int) -> float:
	if total_ns <= 0:
		return 0.0
	return 100.0 * float(part_ns) / float(total_ns)


static func _rank_ns_fields(source: Dictionary, fields: Array[String]) -> Array:
	var ranked: Array = []
	var total_ns: int = 0
	for field in fields:
		total_ns += int(source.get(field, 0))
	for field in fields:
		var value_ns: int = int(source.get(field, 0))
		if value_ns <= 0:
			continue
		ranked.append({
			"phase": field,
			"ns": value_ns,
			"pct": _profile_percent(value_ns, total_ns),
		})
	ranked.sort_custom(func(left: Dictionary, right: Dictionary) -> bool:
		return int(left.get("ns", 0)) > int(right.get("ns", 0))
	)
	return ranked


static func _team_size_rankings(team_size_ns: Dictionary, total_ns: int) -> Array:
	var ranked: Array = []
	for key in team_size_ns.keys():
		var value_ns: int = int(team_size_ns[key])
		if value_ns <= 0:
			continue
		ranked.append({
			"team_size": int(key),
			"ns": value_ns,
			"pct": _profile_percent(value_ns, total_ns),
		})
	ranked.sort_custom(func(left: Dictionary, right: Dictionary) -> bool:
		return int(left.get("ns", 0)) > int(right.get("ns", 0))
	)
	return ranked


static func _build_profile_summary(profile_state: Dictionary) -> Dictionary:
	var wall_ns: int = int(profile_state.get("wall_ns", 0))
	var chunk_total_ns: int = int(profile_state.get("chunk_total_ns", 0))
	var chunk_count: int = maxi(0, int(profile_state.get("chunk_count", 0)))
	var match_count: int = maxi(0, int(profile_state.get("match_count", 0)))
	var total_match_path_ns: int = int(profile_state.get("assembly_ns", 0)) + int(profile_state.get("native_run_ns", 0)) + int(profile_state.get("summary_to_dict_ns", 0)) + int(profile_state.get("matchup_ns", 0)) + int(profile_state.get("clear_ns", 0))
	var setup_total_ns: int = int(profile_state.get("probe_ns", 0)) + int(profile_state.get("progress_reset_ns", 0)) + int(profile_state.get("worker_startup_ns", 0)) + int(profile_state.get("worker_join_ns", 0))
	var bookkeeping_total_ns: int = int(profile_state.get("aggregation_ns", 0)) + int(profile_state.get("csv_write_ns", 0)) + int(profile_state.get("matchup_write_ns", 0))
	var per_match_ns: float = float(chunk_total_ns) / float(match_count) if match_count > 0 else 0.0
	var per_chunk_ns: float = float(chunk_total_ns) / float(chunk_count) if chunk_count > 0 else 0.0
	var profile_breakdown: Dictionary = {
		"setup_total_ns": setup_total_ns,
		"bookkeeping_total_ns": bookkeeping_total_ns,
		"chunk_total_ns": chunk_total_ns,
		"match_path_total_ns": total_match_path_ns,
	}
	var setup_rankings: Array = _rank_ns_fields(profile_state, ["probe_ns", "progress_reset_ns", "worker_startup_ns", "worker_join_ns"])
	var bookkeeping_rankings: Array = _rank_ns_fields(profile_state, ["aggregation_ns", "csv_write_ns", "matchup_write_ns"])
	var chunk_rankings: Array = _rank_ns_fields(profile_state, ["assembly_ns", "native_run_ns", "summary_to_dict_ns", "matchup_ns", "clear_ns"])
	var top_level_rankings: Array = _rank_ns_fields(profile_breakdown, ["setup_total_ns", "bookkeeping_total_ns", "chunk_total_ns", "match_path_total_ns"])
	return {
		"wall_ns": wall_ns,
		"match_count": match_count,
		"chunk_count": chunk_count,
		"avg_ns_per_match": per_match_ns,
		"avg_ns_per_chunk": per_chunk_ns,
		"setup_phase_rankings": setup_rankings,
		"bookkeeping_phase_rankings": bookkeeping_rankings,
		"chunk_phase_rankings": chunk_rankings,
		"top_level_rankings": top_level_rankings,
		"dominant_top_level_phase": top_level_rankings[0]["phase"] if not top_level_rankings.is_empty() else "",
		"dominant_chunk_phase": chunk_rankings[0]["phase"] if not chunk_rankings.is_empty() else "",
		"team_size_rankings": _team_size_rankings(Dictionary(profile_state.get("team_size_ns", {})), wall_ns),
		"top_level_percentages": {
			"setup_pct": _profile_percent(setup_total_ns, wall_ns),
			"bookkeeping_pct": _profile_percent(bookkeeping_total_ns, wall_ns),
			"chunk_pct": _profile_percent(chunk_total_ns, wall_ns),
			"match_path_pct": _profile_percent(total_match_path_ns, wall_ns),
		},
		"profile_breakdown_ns": profile_breakdown,
	}


func run_packed(p: Dictionary) -> int:
	var raw_sizes: Array = Array(p.get("team_sizes", []))
	var arr: Array[int] = []
	for x in raw_sizes:
		arr.append(int(x))
	return run(
		str(p.get("output_dir", "")),
		arr,
		int(p.get("matches_per_size", 0)),
		int(p.get("base_seed", 0)),
		int(p.get("max_worker_threads", 0)),
		bool(p.get("profile_stats", false))
	)


func run(
	output_dir: String,
	team_sizes: Array[int],
	matches_per_size: int,
	base_seed: int,
	max_worker_threads: int = 0,
	profile_stats: bool = false
) -> Error:
	if output_dir.is_empty():
		return ERR_INVALID_PARAMETER
	if team_sizes.is_empty():
		return ERR_INVALID_PARAMETER
	if matches_per_size < 1:
		return ERR_INVALID_PARAMETER
	var run_start_ns: int = _now_ns()
	var profile_state: Dictionary = {}
	if profile_stats:
		profile_state = {
			"enabled": true,
			"output_dir": output_dir,
			"team_sizes": team_sizes.duplicate(),
			"matches_per_size": matches_per_size,
			"base_seed": base_seed,
			"max_worker_threads": max_worker_threads,
			"probe_ns": 0,
			"progress_reset_ns": 0,
			"team_size_ns": {},
			"worker_startup_ns": 0,
			"worker_join_ns": 0,
			"aggregation_ns": 0,
			"csv_write_ns": 0,
			"matchup_write_ns": 0,
			"chunk_count": 0,
			"match_count": 0,
			"chunk_total_ns": 0,
			"assembly_ns": 0,
			"native_run_ns": 0,
			"summary_to_dict_ns": 0,
			"matchup_ns": 0,
			"clear_ns": 0,
		}
	var probe_start_ns: int = _now_ns()
	var probe := NativeSimulationBackendScript.new()
	if not probe.is_available():
		return ERR_UNAVAILABLE
	if profile_stats:
		profile_state["probe_ns"] = _now_ns() - probe_start_ns
	var total_matches: int = team_sizes.size() * matches_per_size
	var reset_start_ns: int = _now_ns()
	SimulationBatchWorkerScript.reset_benchmark_progress(total_matches)
	if profile_stats:
		profile_state["progress_reset_ns"] = _now_ns() - reset_start_ns
	var aggregator := StatsCsvAggregator.new()
	aggregator.reset()
	for sz in team_sizes:
		var size_start_ns: int = _now_ns()
		var per_size_seed: int = base_seed + int(sz) * 1_009_033
		var err_sz: Error = _run_matches_for_team_size(
			aggregator, int(sz), per_size_seed, matches_per_size, max_worker_threads, profile_state if profile_stats else null
		)
		if err_sz != OK:
			return err_sz
		if profile_stats:
			var team_ns: Dictionary = Dictionary(profile_state.get("team_size_ns", {}))
			team_ns[int(sz)] = _now_ns() - size_start_ns
			profile_state["team_size_ns"] = team_ns
	
	# Write regular CSV files
	var csv_start_ns: int = _now_ns()
	var csv_err: Error = aggregator.write_to_dir(output_dir)
	if csv_err != OK:
		return csv_err
	if profile_stats:
		profile_state["csv_write_ns"] = _now_ns() - csv_start_ns
	
	# Write matchup data file
	var matchup_start_ns: int = _now_ns()
	var matchup_success: bool = aggregator.write_matchup_file(output_dir)
	if not matchup_success:
		push_error("Failed to write matchup data file")
		return ERR_FILE_CANT_WRITE
	if profile_stats:
		profile_state["matchup_write_ns"] = _now_ns() - matchup_start_ns
		profile_state["wall_ns"] = _now_ns() - run_start_ns
		_emit_profile_line({
			"stats_profile": profile_state,
			"stats_profile_summary": _build_profile_summary(profile_state),
		})
	
	return OK


func _worker_count_for_export(matches_per_size: int, max_worker_threads: int) -> int:
	if max_worker_threads > 0:
		return maxi(1, mini(matches_per_size, max_worker_threads))
	var cpu: int = maxi(1, OS.get_processor_count())
	return maxi(1, mini(mini(matches_per_size, cpu), DEFAULT_EXPORT_MAX_WORKER_THREADS))


func _run_matches_for_team_size(
	aggregator: RefCounted,
	team_size: int,
	per_size_seed: int,
	matches_per_size: int,
	max_worker_threads: int,
	profile_state: Variant = null
) -> Error:
	var worker_count: int = _worker_count_for_export(matches_per_size, max_worker_threads)
	var slice: int = (matches_per_size + worker_count - 1) / worker_count
	var threads: Array = []
	var worker_runners: Array = []
	var do_profile: bool = profile_state is Dictionary
	var startup_start_ns: int = _now_ns()
	for worker_index in range(worker_count):
		var start_index: int = worker_index * slice
		var end_index: int = mini(matches_per_size, start_index + slice)
		if start_index >= end_index:
			break
		var thread_data := {
			"start_index": start_index,
			"end_index": end_index,
			"team_size": team_size,
			"base_seed": per_size_seed,
			"bench_skip_summaries": false,
			"allow_native_batch": false,
			"profile_stats": do_profile,
		}
		var runner := SimulationBatchWorkerScript.new()
		worker_runners.append(runner)
		var thread := Thread.new()
		threads.append(thread)
		var start_err: Error = thread.start(Callable(runner, "run_chunk").bind(thread_data))
		if start_err != OK:
			for started_thread_obj in threads:
				var started_thread: Thread = started_thread_obj as Thread
				if started_thread != null and started_thread.is_started():
					started_thread.wait_to_finish()
			return start_err
	if do_profile:
		var ps: Dictionary = profile_state
		ps["worker_startup_ns"] = int(ps.get("worker_startup_ns", 0)) + (_now_ns() - startup_start_ns)
		ps["worker_count"] = int(ps.get("worker_count", 0)) + threads.size()

	var join_start_ns: int = _now_ns()
	var batch_results: Array = []
	batch_results.resize(threads.size())
	for i in range(threads.size()):
		var t: Thread = threads[i] as Thread
		var results: Variant = t.wait_to_finish()
		if not results is Array:
			return ERR_SCRIPT_FAILED
		var arr: Array = results as Array
		if arr.is_empty():
			return ERR_SCRIPT_FAILED
		batch_results[i] = arr
	if do_profile:
		var ps_join: Dictionary = profile_state
		ps_join["worker_join_ns"] = int(ps_join.get("worker_join_ns", 0)) + (_now_ns() - join_start_ns)
	for batch in batch_results:
		for entry in (batch as Array):
			if do_profile and entry is Dictionary:
				var chunk_dict: Dictionary = Dictionary(entry)
				if chunk_dict.has("profile_stats"):
					var chunk_profile: Dictionary = Dictionary(chunk_dict.get("profile_stats", {}))
					if not chunk_profile.is_empty():
						_emit_profile_line({"stats_profile_chunk": chunk_profile})
						var ps_chunk: Dictionary = profile_state
						ps_chunk["chunk_count"] = int(ps_chunk.get("chunk_count", 0)) + 1
						ps_chunk["chunk_total_ns"] = int(ps_chunk.get("chunk_total_ns", 0)) + int(chunk_profile.get("wall_ns", 0))
						ps_chunk["assembly_ns"] = int(ps_chunk.get("assembly_ns", 0)) + int(chunk_profile.get("assembly_ns", 0))
						ps_chunk["native_run_ns"] = int(ps_chunk.get("native_run_ns", 0)) + int(chunk_profile.get("native_run_ns", 0))
						ps_chunk["summary_to_dict_ns"] = int(ps_chunk.get("summary_to_dict_ns", 0)) + int(chunk_profile.get("summary_to_dict_ns", 0))
						ps_chunk["matchup_ns"] = int(ps_chunk.get("matchup_ns", 0)) + int(chunk_profile.get("matchup_ns", 0))
						ps_chunk["clear_ns"] = int(ps_chunk.get("clear_ns", 0)) + int(chunk_profile.get("clear_ns", 0))
						ps_chunk["match_count"] = int(ps_chunk.get("match_count", 0)) + int(chunk_profile.get("match_count", 0))
			var agg_start_ns: int = _now_ns()
			aggregator.consume_summary(team_size, entry)
			if do_profile:
				var ps_agg: Dictionary = profile_state
				ps_agg["aggregation_ns"] = int(ps_agg.get("aggregation_ns", 0)) + (_now_ns() - agg_start_ns)
	return OK
