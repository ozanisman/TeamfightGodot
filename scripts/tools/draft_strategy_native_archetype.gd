## Experimental native draft AI strategy with small archetype tag scoring.
## Keeps baseline native scoring unchanged; passes an experimental archetype strategy.

extends "res://scripts/tools/draft_strategy.gd"

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

const STRATEGY_FULL: int = 4
const STRATEGY_LIGHT: int = 5
const STRATEGY_PICK_LIGHT: int = 6
const STRATEGY_BAN_LIGHT: int = 7

var _backend: RefCounted = null
var _stats_dir: String = "res://stats_output_100k"
var _weight_overrides: Dictionary = {}
var _strategy: int = STRATEGY_FULL
var _strategy_name: String = "native_archetype"


func _init(stats_dir: String = "res://stats_output_100k", strategy: int = STRATEGY_FULL, strategy_name: String = "native_archetype") -> void:
	_stats_dir = stats_dir
	_strategy = strategy
	_strategy_name = strategy_name
	_backend = NativeSimulationBackendScript.new()


func set_weight_overrides(weight_overrides: Dictionary) -> void:
	_weight_overrides = weight_overrides


func get_strategy_name() -> String:
	return _strategy_name


func recommend_next_pick(allies: Array, enemies: Array, available: Array, draft_step: int = -1) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeArchetypeStrategy: native backend unavailable")
		return StringName(available[0])

	var recommendations: Array = _backend.get_draft_ai_pick_recommendations(_stats_dir, available, allies, enemies, 1, draft_step, _strategy)
	if recommendations.is_empty():
		push_warning("NativeArchetypeStrategy: no pick recommendations returned, falling back to first available")
		return StringName(available[0])

	var top_rec: Dictionary = recommendations[0]
	return StringName(top_rec.candidate)


func recommend_next_ban(allies: Array, enemies: Array, available: Array, draft_step: int = -1, side: String = "blue", weight_overrides: Dictionary = {}) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeArchetypeStrategy: native backend unavailable")
		return StringName(available[0])

	var recommendations: Array = _backend.get_draft_ai_ban_recommendations(_stats_dir, available, allies, enemies, 1, draft_step, side, _weight_overrides, _strategy)
	if recommendations.is_empty():
		push_warning("NativeArchetypeStrategy: no ban recommendations returned, falling back to first available")
		return StringName(available[0])

	var top_rec: Dictionary = recommendations[0]
	return StringName(top_rec.candidate)
