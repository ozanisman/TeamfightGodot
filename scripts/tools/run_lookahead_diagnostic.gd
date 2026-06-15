extends SceneTree

## Run lookahead turn logic diagnostic

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

var _backend: RefCounted = null

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	_backend = NativeSimulationBackendScript.new()
	if not _backend.is_available():
		push_error("Lookahead diagnostic: native backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	
	print("Running lookahead turn logic diagnostic...")
	print("Writing to lookahead_turn_logic_report.txt...")
	
	_backend.debug_lookahead_turn_diagnostic()
	
	print("Diagnostic complete. Output above.")
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)
