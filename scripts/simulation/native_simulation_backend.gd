class_name NativeSimulationBackend
extends RefCounted

const TeamfightSimulationCoreScript := preload("res://scripts/simulation/teamfight_simulation_core.gd")
const SimRunnerScript := preload("res://scripts/simulation/sim_runner.gd")
const NativeClassName := "TeamfightSimulationCore"
const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"
## Must match [libraries] windows.* entry in teamfight_simulation_core.gdextension.
const NativeWindowsDllResPath := "res://native/bin/teamfight_simulation_core.dll"

var _backend: Object = null
var _native_available: bool = false
var _validation_mode: bool = false
static var _logged_extension_load_failure: bool = false


## Windows-only path probe; do not use alone to decide if native sim is active — use is_native_runtime().
static func is_windows_native_dll_file_present() -> bool:
	if OS.get_name() != "Windows":
		return true
	return FileAccess.file_exists(NativeWindowsDllResPath)


func _try_load_native_extension() -> void:
	var load_status: int = GDExtensionManager.load_extension(NativeExtensionPath)
	# load_extension returns GDExtensionManager.LoadStatus, not Error. 2 == LOAD_STATUS_ALREADY_LOADED.
	if (
		load_status != GDExtensionManager.LOAD_STATUS_OK
		and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED
	):
		if not _logged_extension_load_failure:
			_logged_extension_load_failure = true
			push_warning(
				"GDExtension load failed (status %s) for %s (e.g. missing DLL or bad binary)."
				% [load_status, NativeExtensionPath]
			)


func _attach_native_or_gdscript() -> void:
	_try_load_native_extension()
	if ClassDB.class_exists(NativeClassName) and ClassDB.can_instantiate(NativeClassName):
		_backend = ClassDB.instantiate(NativeClassName)
		if _backend != null:
			_native_available = true
			return
		push_warning("Native simulation core failed to instantiate; using GDScript backend.")
	else:
		push_warning(
			"Native class %s not registered; using GDScript teamfight_simulation_core.gd (slower; build native/bin DLL for release)."
			% NativeClassName
		)
	_backend = TeamfightSimulationCoreScript.new()
	_native_available = false


func _init(validation_mode: bool = false) -> void:
	_validation_mode = validation_mode
	if validation_mode:
		push_warning("Native simulation core unavailable; using reference-only GDScript backend for validation.")
		_backend = TeamfightSimulationCoreScript.new()
		_native_available = false
	else:
		_attach_native_or_gdscript()

func _ensure_native_backend() -> bool:
	if _backend != null:
		return true
	if _validation_mode:
		return false
	_try_load_native_extension()
	if ClassDB.class_exists(NativeClassName) and ClassDB.can_instantiate(NativeClassName):
		var native_backend: Object = ClassDB.instantiate(NativeClassName)
		if native_backend != null:
			if native_backend.has_method("is_ready") and not bool(native_backend.call("is_ready")):
				native_backend = null
			else:
				_backend = native_backend
				_native_available = true
				return true
	_backend = TeamfightSimulationCoreScript.new()
	_native_available = false
	return true

func is_available() -> bool:
	if _backend != null:
		return true
	if _validation_mode:
		return false
	_try_load_native_extension()
	return ClassDB.class_exists(NativeClassName) and ClassDB.can_instantiate(NativeClassName)


## True when GDExtension TeamfightSimulationCore is active (not GDScript fallback).
func is_native_runtime() -> bool:
	return _native_available

func clear() -> void:
	if not _ensure_native_backend():
		return
	if _backend != null and _backend.has_method("clear"):
		_backend.call("clear")

func run_match(match_input):
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return {}

	var runner := SimRunnerScript.new()
	return runner.run_to_end_with_core(_backend, match_input)

func run_matches(match_inputs: Array):
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return []
	if _backend.has_method("run_matches"):
		return _backend.call("run_matches", match_inputs)

	var fallback: Array = []
	fallback.resize(match_inputs.size())
	for index in range(match_inputs.size()):
		fallback[index] = run_match(match_inputs[index])
		clear()
	return fallback

func run_match_simulation_only(match_input: Variant) -> void:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return
	if _backend.has_method("run_match_simulation_only"):
		_backend.call("run_match_simulation_only", match_input)
		return
	run_match(match_input)
	if _backend != null and _backend.has_method("clear"):
		_backend.call("clear")

## Runs N full simulations without building summaries. Call from one thread at a time (e.g. bench with --workers=1).
func run_matches_simulation_only(match_inputs: Array) -> void:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return
	if _backend.has_method("run_matches_simulation_only"):
		_backend.call("run_matches_simulation_only", match_inputs)
		return
	for index in range(match_inputs.size()):
		run_match_simulation_only(match_inputs[index])
