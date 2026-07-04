## Native draft AI strategy with 1-ply lookahead for bans only (Phase 34)
## Uses DraftStatsDatabase, DraftEvaluator, DraftRecommender with lookahead for bans
## Picks use baseline native scoring
## GDScript only orchestrates - C++ does all scoring

extends "res://scripts/tools/draft_strategy.gd"

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftAiConfigScript := preload("res://scripts/tools/draft_ai_config.gd")

var _backend: RefCounted = null
var _stats_dir: String = "res://model_stats/stats_output_100k"
var _weight_overrides: Dictionary = {}
const STRATEGY: int = 3  # DraftStrategy::NATIVE_LOOKAHEAD_BAN


func _init(stats_dir: String = "res://model_stats/stats_output_100k") -> void:
	_stats_dir = stats_dir
	_backend = NativeSimulationBackendScript.new()


func set_weight_overrides(weight_overrides: Dictionary) -> void:
	_weight_overrides = weight_overrides


func get_strategy_name() -> String:
	return "native_lookahead_ban_only"


func recommend_next_pick(allies: Array, enemies: Array, available: Array, draft_step: int = -1) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeLookaheadBanStrategy: native backend unavailable")
		return StringName(available[0])

	# Use baseline native for picks (strategy 0)
	var recommendations: Array = _backend.get_draft_ai_pick_recommendations(_stats_dir, available, allies, enemies, 1, draft_step, 0)
	if recommendations.is_empty():
		push_warning("NativeLookaheadBanStrategy: no pick recommendations returned, falling back to first available")
		return StringName(available[0])

	var top_rec: Dictionary = recommendations[0]
	var champion: StringName = StringName(top_rec.candidate)
	return champion


func recommend_next_ban(allies: Array, enemies: Array, available: Array, draft_step: int = -1, side: String = "blue", weight_overrides: Dictionary = {}) -> StringName:
	if available.is_empty():
		return StringName("")
	if _backend == null or not _backend.is_available():
		push_error("NativeLookaheadBanStrategy: native backend unavailable")
		return StringName(available[0])

	var recommendations: Array = _backend.get_draft_ai_ban_recommendations(
		_stats_dir, available, allies, enemies, 1, draft_step, side, _weight_overrides, STRATEGY,
		DraftAiConfigScript.LEGACY_LOOKAHEAD_CONFIG_PATH
	)
	if recommendations.is_empty():
		push_warning("NativeLookaheadBanStrategy: no ban recommendations returned, falling back to first available")
		return StringName(available[0])

	var top_rec: Dictionary = recommendations[0]
	var champion: StringName = StringName(top_rec.candidate)
	return champion
