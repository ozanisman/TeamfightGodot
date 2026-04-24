class_name SimRunner
extends RefCounted

## Thin wrapper around [TeamfightSimulationCore] for fixed-timestep stepping and optional trace export.

var _core: Object = null
var _tick_rate: float = 0.2
var _accum: float = 0.0

func _init(p_core: Object = null) -> void:
	_core = p_core

func set_core(p_core: Object) -> void:
	_core = p_core

func begin(match_input: Variant) -> void:
	if _core == null:
		push_error("SimRunner.begin: core is null.")
		return
	_accum = 0.0
	if match_input is Dictionary:
		_tick_rate = float(Dictionary(match_input).get("tick_rate", 0.2))
	else:
		_tick_rate = float(match_input.tick_rate)
	_core.call("begin_match", match_input)

func step(delta: float) -> void:
	if _core == null:
		return
	_accum += delta
	while _accum + 1e-6 >= _tick_rate:
		if bool(_core.call("match_ticks_exhausted")):
			return
		_accum -= _tick_rate
		_core.call("advance_one_tick")

func is_finished() -> bool:
	if _core == null:
		return true
	return bool(_core.call("match_ticks_exhausted"))

func finish() -> Dictionary:
	if _core == null:
		return {}
	return _core.call("finish_and_summarize")

## Runs the full match using the same tick schedule as [method TeamfightSimulationCore.run_match].
func run_to_end_with_core(core: Object, match_input: Variant) -> Dictionary:
	set_core(core)
	begin(match_input)
	while not is_finished():
		step(_tick_rate)
	return finish()

func get_trace_events() -> Array:
	if _core != null and _core.has_method("get_trace_events"):
		return _core.call("get_trace_events")
	return []
