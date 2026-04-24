extends SceneTree

const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"
const SimulationBatchWorkerScript := preload("res://scripts/simulation/simulation_batch_worker.gd")

func _init() -> void:
	call_deferred("_run_benchmark")

func _parse_int(prefix: String, fallback: int) -> int:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with(prefix):
			return int(arg.substr(prefix.length()))
	return fallback

func _flag_enabled(prefix: String) -> bool:
	for arg in OS.get_cmdline_user_args():
		if arg == prefix:
			return true
		if arg.begins_with(prefix + "="):
			var tail: String = arg.substr(prefix.length() + 1)
			return tail != "0" and tail.to_lower() != "false"
	return false

func _run_benchmark() -> void:
	var extension_path: String = ProjectSettings.globalize_path(NativeExtensionPath)
	var load_status: int = GDExtensionManager.load_extension(extension_path)
	if load_status != GDExtensionManager.LOAD_STATUS_OK and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED:
		push_error("Failed to load %s (status %d)" % [NativeExtensionPath, load_status])
		quit(1)
		return

	await process_frame
	var batch_count: int = maxi(1, _parse_int("--batch-count=", 100000))
	var team_size: int = maxi(1, _parse_int("--team-size=", 1))
	var bench_skip_summaries: bool = _flag_enabled("--bench-skip-summaries")
	var cpu_count: int = maxi(1, OS.get_processor_count())
	var worker_cap: int = _parse_int("--workers=", _parse_int("--max-workers=", 0))
	var worker_count: int = mini(batch_count, cpu_count)
	if worker_cap > 0:
		worker_count = mini(worker_count, worker_cap)
	worker_count = maxi(1, worker_count)
	var chunk_size: int = int(ceil(float(batch_count) / float(worker_count)))
	var worker_runner = SimulationBatchWorkerScript.new()
	var threads: Array[Thread] = []

	var start_static_memory: int = int(OS.get_static_memory_usage())
	var start_peak_memory: int = int(OS.get_static_memory_peak_usage())
	var start_usec: int = Time.get_ticks_usec()

	for worker_index in range(worker_count):
		var start_index: int = worker_index * chunk_size
		var end_index: int = mini(batch_count, start_index + chunk_size)
		if start_index >= end_index:
			break

		var thread := Thread.new()
		threads.append(thread)
		var thread_data := {
			"start_index": start_index,
			"end_index": end_index,
			"team_size": team_size,
			"base_seed": 0,
			"bench_skip_summaries": bench_skip_summaries,
			# Batched native sim from multiple threads faults on Windows; single worker is safe and fastest for bench-skip.
			"allow_native_batch": bench_skip_summaries and worker_count == 1,
		}
		var start_error: int = thread.start(Callable(worker_runner, "run_chunk").bind(thread_data))
		if start_error != OK:
			push_error("Failed to start benchmark worker thread.")
			for started_thread in threads:
				if started_thread is Thread:
					(started_thread as Thread).wait_to_finish()
			quit(1)
			return

	var completed_matches: int = 0
	for thread in threads:
		var chunk_results: Array = thread.wait_to_finish()
		completed_matches += chunk_results.size()

	var elapsed_usec: int = max(1, Time.get_ticks_usec() - start_usec)
	var end_static_memory: int = int(OS.get_static_memory_usage())
	var end_peak_memory: int = int(OS.get_static_memory_peak_usage())
	var duration_sec: float = float(elapsed_usec) / 1000000.0
	var matches_per_sec: float = float(completed_matches) / maxf(0.000001, duration_sec)
	var live_memory_delta: int = end_static_memory - start_static_memory
	var peak_memory_growth: int = end_peak_memory - start_peak_memory
	var churn_estimate: int = end_peak_memory - end_static_memory

	print(JSON.stringify({
		"batch_count": completed_matches,
		"team_size": team_size,
		"bench_skip_summaries": bench_skip_summaries,
		"workers": threads.size(),
		"duration_sec": duration_sec,
		"matches_per_sec": matches_per_sec,
		"static_memory_start": start_static_memory,
		"static_memory_end": end_static_memory,
		"static_memory_delta": live_memory_delta,
		"peak_memory_start": start_peak_memory,
		"peak_memory_end": end_peak_memory,
		"peak_memory_growth": peak_memory_growth,
		"allocation_churn_estimate": churn_estimate,
	}, "\t", true, true))

	threads.clear()
	worker_runner = null
	await process_frame
	await process_frame
	quit(0)
