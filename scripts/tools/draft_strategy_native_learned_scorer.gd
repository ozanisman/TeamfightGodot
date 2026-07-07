## Validation-only strategy that ranks native recommendation candidates with the
## offline draft-state learned scorer model. Not used by gameplay or UI.

extends "res://scripts/tools/draft_strategy.gd"

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftAiConfigScript := preload("res://scripts/tools/draft_ai_config.gd")
const DraftStateScorerModelScript := preload("res://scripts/tools/draft_state_scorer_model.gd")

var _backend: RefCounted = null
var _stats_dir: String = "res://model_stats/stats_output_100k"
var _model_path: String = DraftStateScorerModelScript.DEFAULT_MODEL_PATH
var _model: RefCounted = null
var _load_error: String = ""
var _blue_picks_before: Array = []
var _red_picks_before: Array = []
var _blue_bans_before: Array = []
var _red_bans_before: Array = []
var _acting_side: String = "blue"


func _init(stats_dir: String = "res://model_stats/stats_output_100k", model_path: String = DraftStateScorerModelScript.DEFAULT_MODEL_PATH) -> void:
	_stats_dir = stats_dir
	_model_path = model_path
	_backend = NativeSimulationBackendScript.new()
	_model = DraftStateScorerModelScript.new()
	if not _model.load_from_path(_model_path):
		_load_error = _model.last_error()
		push_error("NativeLearnedScorerStrategy: %s" % _load_error)


func get_strategy_name() -> String:
	return "native_learned_scorer"


func is_ready() -> bool:
	return _model != null and _model.is_loaded()


func last_error() -> String:
	if _load_error.is_empty():
		return "model is not loaded"
	return _load_error


func set_draft_context(
	blue_picks_before: Array,
	red_picks_before: Array,
	blue_bans_before: Array,
	red_bans_before: Array,
	acting_side: String
) -> void:
	_blue_picks_before = blue_picks_before.duplicate()
	_red_picks_before = red_picks_before.duplicate()
	_blue_bans_before = blue_bans_before.duplicate()
	_red_bans_before = red_bans_before.duplicate()
	_acting_side = acting_side


func recommend_next_pick(allies: Array, enemies: Array, available: Array, draft_step: int = -1) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeLearnedScorerStrategy: native backend unavailable")
		return StringName("")
	if _model == null or not _model.is_loaded():
		push_error("NativeLearnedScorerStrategy: model unavailable: %s" % last_error())
		return StringName("")

	var recommendations: Array = _backend.get_draft_ai_pick_recommendations(
		_stats_dir,
		available,
		allies,
		enemies,
		available.size(),
		draft_step,
		0,
		DraftAiConfigScript.DEFAULT_CONFIG_PATH
	)
	return _select_best(recommendations, "PICK", available, draft_step)


func recommend_next_ban(allies: Array, enemies: Array, available: Array, draft_step: int = -1, side: String = "blue", weight_overrides: Dictionary = {}) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeLearnedScorerStrategy: native backend unavailable")
		return StringName("")
	if _model == null or not _model.is_loaded():
		push_error("NativeLearnedScorerStrategy: model unavailable: %s" % last_error())
		return StringName("")

	var recommendations: Array = _backend.get_draft_ai_ban_recommendations(
		_stats_dir,
		available,
		allies,
		enemies,
		available.size(),
		draft_step,
		side,
		weight_overrides,
		0,
		DraftAiConfigScript.DEFAULT_CONFIG_PATH
	)
	return _select_best(recommendations, "BAN", available, draft_step)


func _select_best(recommendations: Array, action: String, available: Array, draft_step: int) -> StringName:
	if recommendations.is_empty():
		push_error("NativeLearnedScorerStrategy: no %s recommendations returned" % action.to_lower())
		return StringName("")

	var best_candidate := StringName("")
	var best_learned_score: float = -INF
	var best_native_score: float = -INF
	for rec_var in recommendations:
		var rec: Dictionary = rec_var
		var candidate := StringName(rec.get("candidate", ""))
		if candidate.is_empty() or not candidate in available:
			continue
		var row: Dictionary = _model.row_from_recommendation(
			rec,
			action,
			_acting_side,
			draft_step,
			_blue_picks_before,
			_red_picks_before,
			_blue_bans_before,
			_red_bans_before,
			available
		)
		var learned_score: float = _model.score_row(row)
		var native_score: float = float(rec.get("total_score", 0.0))
		if _is_better_candidate(candidate, learned_score, native_score, best_candidate, best_learned_score, best_native_score):
			best_candidate = candidate
			best_learned_score = learned_score
			best_native_score = native_score

	if best_candidate.is_empty():
		return StringName("")
	return best_candidate


func _is_better_candidate(
	candidate: StringName,
	learned_score: float,
	native_score: float,
	best_candidate: StringName,
	best_learned_score: float,
	best_native_score: float
) -> bool:
	if best_candidate.is_empty():
		return true
	if learned_score != best_learned_score:
		return learned_score > best_learned_score
	if native_score != best_native_score:
		return native_score > best_native_score
	return String(candidate) < String(best_candidate)
