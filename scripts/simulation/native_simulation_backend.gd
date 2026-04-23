class_name NativeSimulationBackend
extends RefCounted

const MatchReplaySummaryScript := preload("res://scripts/simulation/match_replay_summary.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const TeamfightSimulationCoreScript := preload("res://scripts/simulation/teamfight_simulation_core.gd")

var _backend: Object = null
var _native_available: bool = false

func _init() -> void:
	if ClassDB.class_exists("TeamfightSimulationCore"):
		var native_backend: Object = ClassDB.instantiate("TeamfightSimulationCore")
		if native_backend != null and native_backend.has_method("is_ready") and bool(native_backend.call("is_ready")):
			_backend = native_backend
			_native_available = true
			return
	_backend = TeamfightSimulationCoreScript.new()
	_native_available = false

func is_available() -> bool:
	return _backend != null

func clear() -> void:
	if _backend != null and _backend.has_method("clear"):
		_backend.call("clear")

func run_match(match_input):
	if _backend == null:
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
	if _backend == null:
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
