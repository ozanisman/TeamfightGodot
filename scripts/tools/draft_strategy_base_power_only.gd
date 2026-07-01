## Base-power-only draft strategy.
## Picks the available champion with the highest native base_power score.
## Bans the available champion with the highest native enemy_pick_value.

extends "res://scripts/tools/draft_strategy.gd"

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

var _backend: RefCounted = null
var _stats_dir: String = "res://model_stats/stats_output_100k"


func _init(stats_dir: String = "res://model_stats/stats_output_100k") -> void:
	_stats_dir = stats_dir
	_backend = NativeSimulationBackendScript.new()


func get_strategy_name() -> String:
	return "base_power_only"


func recommend_next_pick(allies: Array, enemies: Array, available: Array, draft_step: int = -1) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("BasePowerOnlyStrategy: native backend unavailable")
		return StringName(available[0])

	var recommendations: Array = _backend.get_draft_ai_pick_recommendations(
		_stats_dir, available, allies, enemies, available.size(), draft_step
	)
	if recommendations.is_empty():
		push_warning("BasePowerOnlyStrategy: no pick recommendations returned, falling back to first available")
		return StringName(available[0])

	var best: StringName = StringName(recommendations[0].get("candidate", ""))
	var best_score: float = float(recommendations[0].get("base_power", 0.0))
	for rec in recommendations:
		var score: float = float(rec.get("base_power", 0.0))
		if score > best_score:
			best_score = score
			best = StringName(rec.get("candidate", ""))
	if best.is_empty():
		return StringName(available[0])
	return best


func recommend_next_ban(allies: Array, enemies: Array, available: Array, draft_step: int = -1, side: String = "blue", weight_overrides: Dictionary = {}) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("BasePowerOnlyStrategy: native backend unavailable")
		return StringName(available[0])

	var recommendations: Array = _backend.get_draft_ai_ban_recommendations(
		_stats_dir, available, allies, enemies, available.size(), draft_step, side, weight_overrides
	)
	if recommendations.is_empty():
		push_warning("BasePowerOnlyStrategy: no ban recommendations returned, falling back to first available")
		return StringName(available[0])

	var best: StringName = StringName(recommendations[0].get("candidate", ""))
	var best_score: float = float(recommendations[0].get("enemy_pick_value", 0.0))
	for rec in recommendations:
		var score: float = float(rec.get("enemy_pick_value", 0.0))
		if score > best_score:
			best_score = score
			best = StringName(rec.get("candidate", ""))
	if best.is_empty():
		return StringName(available[0])
	return best
