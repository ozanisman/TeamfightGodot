## Native strategy that mirrors the exact stochastic policy shipped in gameplay
## (top-5 candidates + softmax, temperature=0.5, scale=100) instead of deterministic top-1.
## Used to validate the policy players actually face (Section 3.5 mismatch fix).

extends "res://scripts/tools/draft_strategy_native.gd"

const DraftPolicyScript := preload("res://scripts/tools/draft_policy.gd")
const DraftAiConfigScript := preload("res://scripts/tools/draft_ai_config.gd")

# Centralized in draft_ai_config.gd (Workstream 0.2); falls back to hardcoded defaults
# (top_k=5, temperature=0.5, scale=100.0) reproducing prior behavior exactly.
var TOP_K: int = DraftAiConfigScript.get_softmax_top_k()
var TEMPERATURE: float = DraftAiConfigScript.get_softmax_temperature()
var SCALE: float = DraftAiConfigScript.get_softmax_scale()


func get_strategy_name() -> String:
	return "native_softmax"


func recommend_next_pick(allies: Array, enemies: Array, available: Array, draft_step: int = -1) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeSoftmaxStrategy: native backend unavailable")
		return StringName("")

	var recommendations: Array = _backend.get_draft_ai_pick_recommendations(_stats_dir, available, allies, enemies, TOP_K, draft_step, 0, DraftAiConfigScript.DEFAULT_CONFIG_PATH)
	if recommendations.is_empty():
		push_error("NativeSoftmaxStrategy: no pick recommendations returned")
		return StringName("")

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
		return StringName("")

	var recommendations: Array = _backend.get_draft_ai_ban_recommendations(_stats_dir, available, allies, enemies, TOP_K, draft_step, side, _weight_overrides, 0, DraftAiConfigScript.DEFAULT_CONFIG_PATH)
	if recommendations.is_empty():
		push_error("NativeSoftmaxStrategy: no ban recommendations returned")
		return StringName("")

	var selected: StringName = DraftPolicyScript.softmax_select(recommendations, TEMPERATURE, SCALE)
	if selected.is_empty():
		push_warning("NativeSoftmaxStrategy: softmax selection failed, falling back to first available")
		return StringName(available[0])
	return selected
