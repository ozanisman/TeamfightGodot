class_name NativeSimulationBackend
extends RefCounted

const TeamfightSimulationCoreScript := preload("res://scripts/simulation/teamfight_simulation_core.gd")
const NativeClassName := "TeamfightSimulationCore"
const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"

var _backend: Object = null
var _native_available: bool = false
var _validation_mode: bool = false

func _init(validation_mode: bool = false) -> void:
	_validation_mode = validation_mode
	if validation_mode:
		push_warning("Native simulation core unavailable; using reference-only GDScript backend for validation.")
		_backend = TeamfightSimulationCoreScript.new()
		_native_available = false
	else:
		if not ClassDB.can_instantiate(NativeClassName):
			var load_status: int = GDExtensionManager.load_extension(NativeExtensionPath)
			if load_status != OK and load_status != ERR_ALREADY_EXISTS:
				push_error("Failed to load native simulation extension: %s" % NativeExtensionPath)
		if not ClassDB.can_instantiate(NativeClassName):
			push_error("Native simulation core unavailable; native is required for production runtime.")
			_backend = null
			_native_available = false
		else:
			_backend = ClassDB.instantiate(NativeClassName)
			_native_available = _backend != null
			if not _native_available:
				push_error("Native simulation core failed to initialize.")

func _ensure_native_backend() -> bool:
	if _backend != null:
		return true
	if _validation_mode:
		return false
	if not ClassDB.can_instantiate(NativeClassName):
		var load_status: int = GDExtensionManager.load_extension(NativeExtensionPath)
		if load_status != OK and load_status != ERR_ALREADY_EXISTS:
			return false
		if not ClassDB.can_instantiate(NativeClassName):
			return false
	var native_backend: Object = ClassDB.instantiate(NativeClassName)
	if native_backend == null:
		return false
	if native_backend.has_method("is_ready") and not bool(native_backend.call("is_ready")):
		return false
	_backend = native_backend
	_native_available = true
	return true

func is_available() -> bool:
	if _backend != null:
		return true
	if _validation_mode:
		return false
	if ClassDB.can_instantiate(NativeClassName):
		return true
	var load_status: int = GDExtensionManager.load_extension(NativeExtensionPath)
	if load_status != OK and load_status != ERR_ALREADY_EXISTS:
		return false
	return ClassDB.can_instantiate(NativeClassName)

func clear() -> void:
	if not _ensure_native_backend():
		return
	if _backend != null and _backend.has_method("clear"):
		_backend.call("clear")

func run_match(match_input):
	if not _ensure_native_backend():
		push_error("Native simulation core is not available.")
		return {}

	if _backend.has_method("run_match"):
		return _backend.call("run_match", match_input)

	push_error("Native simulation core does not expose run_match(match_input).")
	return {}

func run_matches(match_inputs: Array):
	if not _ensure_native_backend():
		push_error("Native simulation core is not available.")
		return []
	if _backend.has_method("run_matches"):
		return _backend.call("run_matches", match_inputs)

	var fallback: Array = []
	fallback.resize(match_inputs.size())
	for index in range(match_inputs.size()):
		fallback[index] = run_match(match_inputs[index])
		clear()
	return fallback
