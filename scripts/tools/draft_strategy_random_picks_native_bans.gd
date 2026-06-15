## Random picks + native bans wrapper strategy

const DraftStrategyNativePath := preload("res://scripts/tools/draft_strategy_native.gd")
const DraftStrategyRandomPath := preload("res://scripts/tools/draft_strategy_random.gd")

var _native_strategy: RefCounted = null
var _random_strategy: RefCounted = null
var _stats_dir: String = "res://model_stats/stats_output_100k"


func _init(stats_dir: String = "res://model_stats/stats_output_100k") -> void:
	_stats_dir = stats_dir
	_native_strategy = DraftStrategyNativePath.new(_stats_dir)
	_random_strategy = DraftStrategyRandomPath.new()


func set_weight_overrides(weight_overrides: Dictionary) -> void:
	_native_strategy.set_weight_overrides(weight_overrides)


func get_strategy_name() -> String:
	return "random_picks_native_bans"


func recommend_next_pick(allies: Array, enemies: Array, available: Array, draft_step: int = -1) -> StringName:
	return _random_strategy.recommend_next_pick(allies, enemies, available, draft_step)


func recommend_next_ban(allies: Array, enemies: Array, available: Array, draft_step: int = -1, side: String = "blue", weight_overrides: Dictionary = {}) -> StringName:
	return _native_strategy.recommend_next_ban(allies, enemies, available, draft_step, side, weight_overrides)
