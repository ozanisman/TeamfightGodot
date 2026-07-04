## Native lookahead strategy with softmax selection (validation-only).
## Uses policy-aware opponent modeling (softmax expectation) in C++ and
## DraftPolicy.softmax_select for final pick/ban choice.

extends "res://scripts/tools/draft_strategy_native_softmax.gd"

const STRATEGY_LOOKAHEAD: int = 1  # DraftStrategy::NATIVE_LOOKAHEAD


func get_strategy_name() -> String:
	return "native_lookahead_softmax"


func recommend_next_pick(allies: Array, enemies: Array, available: Array, draft_step: int = -1) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeLookaheadSoftmaxStrategy: native backend unavailable")
		return StringName("")

	var recommendations: Array = _backend.get_draft_ai_pick_recommendations(
		_stats_dir, available, allies, enemies, TOP_K, draft_step,
		STRATEGY_LOOKAHEAD, DraftAiConfigScript.LOOKAHEAD_SOFTMAX_CONFIG_PATH
	)
	if recommendations.is_empty():
		push_error("NativeLookaheadSoftmaxStrategy: no pick recommendations returned")
		return StringName("")

	var selected: StringName = DraftPolicyScript.softmax_select(recommendations, TEMPERATURE, SCALE)
	if selected.is_empty():
		push_warning("NativeLookaheadSoftmaxStrategy: softmax selection failed, falling back to first available")
		return StringName(available[0])
	return selected


func recommend_next_ban(allies: Array, enemies: Array, available: Array, draft_step: int = -1, side: String = "blue", weight_overrides: Dictionary = {}) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeLookaheadSoftmaxStrategy: native backend unavailable")
		return StringName("")

	var recommendations: Array = _backend.get_draft_ai_ban_recommendations(
		_stats_dir, available, allies, enemies, TOP_K, draft_step, side,
		_weight_overrides, STRATEGY_LOOKAHEAD, DraftAiConfigScript.LOOKAHEAD_SOFTMAX_CONFIG_PATH
	)
	if recommendations.is_empty():
		push_error("NativeLookaheadSoftmaxStrategy: no ban recommendations returned")
		return StringName("")

	var selected: StringName = DraftPolicyScript.softmax_select(recommendations, TEMPERATURE, SCALE)
	if selected.is_empty():
		push_warning("NativeLookaheadSoftmaxStrategy: softmax selection failed, falling back to first available")
		return StringName(available[0])
	return selected
