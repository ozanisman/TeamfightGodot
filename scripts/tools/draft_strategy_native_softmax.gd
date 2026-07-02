## Native strategy that mirrors the exact stochastic policy shipped in gameplay
## (top-5 candidates + softmax, temperature=0.5, scale=100) instead of deterministic top-1.
## Used to validate the policy players actually face (Section 3.5 mismatch fix).

extends "res://scripts/tools/draft_strategy_native.gd"

const DraftPolicyScript := preload("res://scripts/tools/draft_policy.gd")

const TOP_K: int = 5
const TEMPERATURE: float = 0.5
const SCALE: float = 100.0


func get_strategy_name() -> String:
	return "native_softmax"


func recommend_next_pick(allies: Array, enemies: Array, available: Array, draft_step: int = -1) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeSoftmaxStrategy: native backend unavailable")
		return StringName(available[0])

	var recommendations: Array = _backend.get_draft_ai_pick_recommendations(_stats_dir, available, allies, enemies, TOP_K, draft_step)
	if recommendations.is_empty():
		push_warning("NativeSoftmaxStrategy: no pick recommendations returned, falling back to first available")
		return StringName(available[0])

	var selected: StringName = DraftPolicyScript.softmax_select(recommendations, TEMPERATURE, SCALE)
	if selected.is_empty():
		push_warning("NativeSoftmaxStrategy: softmax selection failed, falling back to first available")
		return StringName(available[0])
	return selected


func recommend_next_ban(allies: Array, enemies: Array, available: Array, draft_step: int = -1, side: String = "blue", weight_overrides: Dictionary = {}) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeSoftmaxStrategy: native backend unavailable")
		return StringName(available[0])

	var recommendations: Array = _backend.get_draft_ai_ban_recommendations(_stats_dir, available, allies, enemies, TOP_K, draft_step, side, weight_overrides)
	if recommendations.is_empty():
		push_warning("NativeSoftmaxStrategy: no ban recommendations returned, falling back to first available")
		return StringName(available[0])

	var selected: StringName = DraftPolicyScript.softmax_select(recommendations, TEMPERATURE, SCALE)
	if selected.is_empty():
		push_warning("NativeSoftmaxStrategy: softmax selection failed, falling back to first available")
		return StringName(available[0])
	return selected
