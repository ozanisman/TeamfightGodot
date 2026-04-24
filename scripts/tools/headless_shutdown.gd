extends RefCounted
## Shared teardown for headless tools: release static native handles, pump the main loop so RefCounted/native
## refs can drop, then quit. Do not call GDExtensionManager.unload_extension() here (SIGSEGV with live ClassDB refs).

const SimulationBatchWorkerScript := preload("res://scripts/simulation/simulation_batch_worker.gd")
const TEARDOWN_FRAMES_AFTER_RELEASE := 12


static func release_static_native_refs() -> void:
	SimulationBatchWorkerScript.release_benchmark_handles()


static func teardown_extension_then_quit(tree: SceneTree, exit_code: int) -> void:
	await tree.process_frame
	release_static_native_refs()
	for _i in range(TEARDOWN_FRAMES_AFTER_RELEASE):
		await tree.process_frame
	tree.quit(exit_code)
