## LOGIT-based draft recommender strategy (wraps existing native backend).

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

var _backend: RefCounted = null
var _stats_dir: String = "res://stats_output"


func _init(stats_dir: String = "res://stats_output") -> void:
	_stats_dir = stats_dir
	_backend = NativeSimulationBackendScript.new()


func get_strategy_name() -> String:
	return "logit"


func recommend_next_pick(allies: Array, enemies: Array, available: Array) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("LogitStrategy: native backend unavailable")
		return StringName(available[0])

	var rows: Array = _backend.get_draft_recommendations_with_breakdowns(
		allies, enemies, available, available.size(), _stats_dir,
		0.50, 0.25, 0.25
	)
	if rows.is_empty():
		return StringName(available[0])

	var top := Dictionary(rows[0])
	return StringName(top.get("champion", available[0]))
