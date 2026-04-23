class_name NativeSimulationBackend
extends RefCounted

const MatchReplaySummaryScript := preload("res://scripts/simulation/match_replay_summary.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const TeamfightSimulationCoreScript := preload("res://scripts/simulation/teamfight_simulation_core.gd")
const NativeClassName := "TeamfightSimulationCore"

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
	return _backend != null or (not _validation_mode and ClassDB.can_instantiate(NativeClassName))

func clear() -> void:
	if not _ensure_native_backend():
		return
	if _backend != null and _backend.has_method("clear"):
		_backend.call("clear")

func run_match(match_input):
	if not _ensure_native_backend():
		push_error("Native simulation core is not available.")
		return MatchReplaySummaryScript.new()

	if _backend.has_method("run_match"):
		var payload: Variant = match_input.to_dict() if _native_available and match_input is Object and match_input.has_method("to_dict") else match_input
		var result: Variant = _backend.call("run_match", payload)
		if result is Dictionary:
			return MatchReplaySummaryScript.from_dict(Dictionary(result))
		if result is Object and result.has_method("to_dict"):
			return result

	push_error("Native simulation core does not expose run_match(match_input).")
	return MatchReplaySummaryScript.new()

func run_matches(match_inputs: Array):
	if not _ensure_native_backend():
		push_error("Native simulation core is not available.")
		return []
	if _backend.has_method("run_matches"):
		var payloads: Array = []
		payloads.resize(match_inputs.size())
		for index in range(match_inputs.size()):
			var item: Variant = match_inputs[index]
			payloads[index] = item.to_dict() if _native_available and item is Object and item.has_method("to_dict") else item
		var result: Variant = _backend.call("run_matches", payloads)
		if result is Array:
			var summaries: Array = []
			summaries.resize(result.size())
			for index in range(result.size()):
				var entry: Variant = result[index]
				if entry is Dictionary:
					summaries[index] = MatchReplaySummaryScript.from_dict(Dictionary(entry))
				else:
					summaries[index] = entry
			return summaries

	var fallback: Array = []
	fallback.resize(match_inputs.size())
	for index in range(match_inputs.size()):
		fallback[index] = run_match(match_inputs[index])
		clear()
	return fallback
