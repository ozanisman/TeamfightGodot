## Native ablation strategy for validation only.
## Calls the native backend, then re-ranks recommendations with selected components zeroed out.
## Component values remain visible in the output; only the weighted contribution is removed for ranking.

extends "res://scripts/tools/draft_strategy.gd"

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

const PICK_COMPONENTS: Dictionary = {
	"no_ally_synergy": ["ally_synergy"],
	"no_enemy_counter": ["enemy_counter_value"],
	"no_counter_risk": ["counter_risk"],
	"no_role_fit": ["role_fit"],
	"no_comp_fit": ["comp_fit"],
	"base_context_only": ["ally_synergy", "enemy_counter_value", "counter_risk"]
}

# Bans have no counter_risk analog, so no_counter_risk is a pick-only ablation.
const BAN_COMPONENTS: Dictionary = {
	"no_ally_synergy": ["enemy_synergy"],
	"no_enemy_counter": ["counters_my_team"],
	"no_counter_risk": [],
	"no_role_fit": ["fills_enemy_role_need"],
	"no_comp_fit": ["enemy_comp_fit"],
	"base_context_only": ["enemy_synergy", "counters_my_team"]
}

var _backend: RefCounted = null
var _stats_dir: String = "res://model_stats/stats_output_100k"
var _variant: String = ""
var _pick_zero: Array = []
var _ban_zero: Array = []


func _init(stats_dir: String = "res://model_stats/stats_output_100k", variant: String = "no_ally_synergy") -> void:
	_stats_dir = stats_dir
	_variant = variant
	_backend = NativeSimulationBackendScript.new()
	if not PICK_COMPONENTS.has(variant):
		push_error("NativeAblationStrategy: unknown variant '%s', no components will be zeroed" % variant)
	_pick_zero = PICK_COMPONENTS.get(variant, [])
	_ban_zero = BAN_COMPONENTS.get(variant, [])


func get_strategy_name() -> String:
	return "native_ablation_" + _variant


func recommend_next_pick(allies: Array, enemies: Array, available: Array, draft_step: int = -1) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeAblationStrategy: backend unavailable")
		return StringName(available[0])

	var recommendations: Array = _backend.get_draft_ai_pick_recommendations(
		_stats_dir, available, allies, enemies, available.size(), draft_step
	)
	return _pick_by_ablated_score(recommendations, _pick_zero, available)


func recommend_next_ban(allies: Array, enemies: Array, available: Array, draft_step: int = -1, side: String = "blue", weight_overrides: Dictionary = {}) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeAblationStrategy: backend unavailable")
		return StringName(available[0])

	var recommendations: Array = _backend.get_draft_ai_ban_recommendations(
		_stats_dir, available, allies, enemies, available.size(), draft_step, side, weight_overrides
	)
	return _pick_by_ablated_score(recommendations, _ban_zero, available)


func _pick_by_ablated_score(recommendations: Array, zero_components: Array, available: Array) -> StringName:
	if recommendations.is_empty():
		return StringName(available[0])

	var scored: Array[Dictionary] = []
	for rec in recommendations:
		var candidate: StringName = StringName(rec.get("candidate", ""))
		if candidate.is_empty():
			continue

		var ablated_score: float = float(rec.get("total_score", 0.0))
		for comp in zero_components:
			var value: float = float(rec.get(comp, 0.0))
			var weight: float = float(rec.get(comp + "_weight", 0.0))
			ablated_score -= value * weight

		scored.append({"candidate": candidate, "ablated_score": ablated_score})

	if scored.is_empty():
		return StringName(available[0])

	scored.sort_custom(_compare_ablated)
	return scored[0]["candidate"]


func _compare_ablated(a: Dictionary, b: Dictionary) -> bool:
	if a["ablated_score"] != b["ablated_score"]:
		return a["ablated_score"] > b["ablated_score"]
	return String(a["candidate"]) < String(b["candidate"])
