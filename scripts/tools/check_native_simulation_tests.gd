extends SceneTree

const TEST_EXTENSION_PATH := "res://native/tests/teamfight_simulation_tests.gdextension"
const TEST_RUNNER_CLASS := &"TeamfightSimulationTestRunner"

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	var load_status: int = GDExtensionManager.load_extension(TEST_EXTENSION_PATH)
	if (
		load_status != GDExtensionManager.LOAD_STATUS_OK
		and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED
	):
		push_error("native_simulation_tests: extension load failed with status %d" % load_status)
		quit(1)
		return
	if not ClassDB.class_exists(TEST_RUNNER_CLASS) or not ClassDB.can_instantiate(TEST_RUNNER_CLASS):
		push_error("native_simulation_tests: test runner class unavailable")
		quit(1)
		return

	var runner: RefCounted = ClassDB.instantiate(TEST_RUNNER_CLASS) as RefCounted
	var result: Dictionary = runner.call("run_all")
	var failures: Array = result.get("failures", []) as Array
	for failure: Variant in failures:
		push_error("native_simulation_tests: %s" % String(failure))
	var total: int = int(result.get("total", 0))
	var passed: int = int(result.get("passed", 0))
	runner = null
	await process_frame
	GDExtensionManager.unload_extension(TEST_EXTENSION_PATH)

	if not failures.is_empty() or total == 0 or passed != total:
		push_error("native_simulation_tests: FAILED (%d/%d passed)" % [passed, total])
		quit(1)
		return

	print("check_native_simulation_tests: OK (%d/%d passed)" % [passed, total])
	quit(0)
