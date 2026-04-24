class_name StatsSimulationCsvGenerator
extends RefCounted

## Runs native batch matches and writes stats CSVs via [StatsCsvAggregator].
## [param max_worker_threads]: 0 = auto (min of matches, CPU count, [constant DEFAULT_EXPORT_MAX_WORKER_THREADS]); else cap parallel workers.

const SimulationBatchWorkerScript := preload("res://scripts/simulation/simulation_batch_worker.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const StatsCsvAggregatorScript := preload("res://scripts/tools/stats_csv_aggregator.gd")

## Upper bound on parallel export workers (avoids huge thread counts on high-core CPUs).
const DEFAULT_EXPORT_MAX_WORKER_THREADS: int = 16


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
		int(p.get("max_worker_threads", 0))
	)


func run(
	output_dir: String,
	team_sizes: Array[int],
	matches_per_size: int,
	base_seed: int,
	max_worker_threads: int = 0
) -> Error:
	if output_dir.is_empty():
		return ERR_INVALID_PARAMETER
	if team_sizes.is_empty():
		return ERR_INVALID_PARAMETER
	if matches_per_size < 1:
		return ERR_INVALID_PARAMETER
	var probe := NativeSimulationBackendScript.new()
	if not probe.is_available():
		return ERR_UNAVAILABLE
	var total_matches: int = team_sizes.size() * matches_per_size
	SimulationBatchWorkerScript.reset_benchmark_progress(total_matches)
	var aggregator := StatsCsvAggregatorScript.new()
	aggregator.reset()
	for sz in team_sizes:
		var per_size_seed: int = base_seed + int(sz) * 1_009_033
		var err_sz: Error = _run_matches_for_team_size(
			aggregator, int(sz), per_size_seed, matches_per_size, max_worker_threads
		)
		if err_sz != OK:
			return err_sz
	return aggregator.write_to_dir(output_dir)


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
	max_worker_threads: int
) -> Error:
	var worker_count: int = _worker_count_for_export(matches_per_size, max_worker_threads)
	var slice: int = (matches_per_size + worker_count - 1) / worker_count
	var threads: Array = []
	var worker_runners: Array = []
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
	for batch in batch_results:
		for entry in (batch as Array):
			aggregator.consume_summary(team_size, entry)
	return OK
