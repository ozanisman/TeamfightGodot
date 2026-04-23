class_name SimulationBackend
extends RefCounted

func run_match(_match_input):
	push_error("SimulationBackend.run_match() is abstract.")
	return null

func clear() -> void:
	pass

func run_matches(match_inputs: Array):
	var results: Array = []
	results.resize(match_inputs.size())
	for index in range(match_inputs.size()):
		results[index] = run_match(match_inputs[index])
	return results
