extends RefCounted

func run_match(_match_input):
	push_error("SimulationBackend.run_match() is abstract.")
	return null

func clear() -> void:
	pass

func run_matches(_match_inputs: Array):
	push_error("SimulationBackend.run_matches() is abstract.")
	return []
