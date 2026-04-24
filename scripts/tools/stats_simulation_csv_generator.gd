class_name StatsSimulationCsvGenerator
extends RefCounted

## Runs native batch matches and writes stats CSVs via [StatsCsvAggregator].

const SimulationBatchWorkerScript := preload("res://scripts/simulation/simulation_batch_worker.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const StatsCsvAggregatorScript := preload("res://scripts/tools/stats_csv_aggregator.gd")

const CHUNK_SIZE: int = 256


func run_packed(p: Dictionary) -> int:
	var raw_sizes: Array = Array(p.get("team_sizes", []))
	var arr: Array[int] = []
	for x in raw_sizes:
		arr.append(int(x))
	return run(str(p.get("output_dir", "")), arr, int(p.get("matches_per_size", 0)), int(p.get("base_seed", 0)))


func run(output_dir: String, team_sizes: Array[int], matches_per_size: int, base_seed: int) -> Error:
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
	var worker := SimulationBatchWorkerScript.new()
	for sz in team_sizes:
		var per_size_seed: int = base_seed + int(sz) * 1_009_033
		var chunk_start: int = 0
		while chunk_start < matches_per_size:
			var chunk_end: int = mini(matches_per_size, chunk_start + CHUNK_SIZE)
			var thread_data := {
				"start_index": chunk_start,
				"end_index": chunk_end,
				"team_size": int(sz),
				"base_seed": per_size_seed,
				"bench_skip_summaries": false,
				"allow_native_batch": false,
			}
			var results: Array = worker.run_chunk(thread_data)
			if results.is_empty():
				return ERR_SCRIPT_FAILED
			for entry in results:
				aggregator.consume_summary(int(sz), entry)
			chunk_start = chunk_end
	return aggregator.write_to_dir(output_dir)
