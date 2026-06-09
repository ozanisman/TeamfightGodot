## Hybrid-model-based draft recommender strategy.
## Ranks candidates by how much predict_draft_winner_hybrid favors
## the draft state when that candidate is added to the allies team.

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

var _backend: RefCounted = null
var _stats_dir: String = "res://stats_output"


func _init(stats_dir: String = "res://stats_output") -> void:
	_stats_dir = stats_dir
	_backend = NativeSimulationBackendScript.new()


func get_strategy_name() -> String:
	return "hybrid"


func recommend_next_pick(allies: Array, enemies: Array, available: Array) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("HybridStrategy: native backend unavailable")
		return StringName(available[0])

	var best_champion := StringName(available[0])
	var best_score := -1.0

	for candidate in available:
		var hypothetical_allies := allies.duplicate()
		hypothetical_allies.append(candidate)
		var result: Dictionary = _backend.predict_draft_winner_hybrid(
			hypothetical_allies, enemies, _stats_dir
		)
		var score := float(result.get("team1_prob", 0.5))
		if score > best_score:
			best_score = score
			best_champion = StringName(candidate)

	return best_champion
