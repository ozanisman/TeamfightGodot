## Native strategy that mirrors the exact stochastic policy shipped in gameplay
## (top-k candidates + softmax) instead of deterministic top-1.
## Used to validate the policy players actually face (Section 3.5 mismatch fix).

extends "res://scripts/tools/draft_strategy_native.gd"

const DraftPolicyScript := preload("res://scripts/tools/draft_policy.gd")

var _tier_id: String = DraftAiConfigScript.TIER_NORMAL
var TOP_K: int = 5
var TEMPERATURE: float = 0.5
var SCALE: float = 100.0


func _init(stats_dir: String = "res://model_stats/stats_output_100k", tier_id: String = "normal") -> void:
	super._init(stats_dir)
	_tier_id = tier_id if DraftAiConfigScript.is_valid_tier_id(tier_id) else DraftAiConfigScript.TIER_NORMAL
	var preset: Dictionary = DraftAiConfigScript.get_tier_preset(_tier_id)
	TOP_K = int(preset["top_k"])
	TEMPERATURE = float(preset["temperature"])
	SCALE = DraftAiConfigScript.get_softmax_scale()


func get_strategy_name() -> String:
	if _tier_id == DraftAiConfigScript.TIER_EASY:
		return "native_softmax_easy"
	if _tier_id == DraftAiConfigScript.TIER_HARD:
		return "native_softmax_hard"
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
