## Base class for draft pick strategies.
## Each strategy must implement recommend_next_pick() to select a champion
## from the available pool given the current partial draft state.
##
## Use preload("res://scripts/tools/draft_strategy.gd") to extend this class.

var _name: String = "base"

func _init(strategy_name: String = "base") -> void:
	_name = strategy_name


## Return the strategy name for logging/reporting.
func get_strategy_name() -> String:
	return _name


## Select the next champion to pick.
## allies: Array[StringName] — already-picked champions on our team
## enemies: Array[StringName] — already-picked champions on opponent team
## available: Array[StringName] — champions still in the pool
## Returns: StringName of the selected champion.
func recommend_next_pick(allies: Array, enemies: Array, available: Array) -> StringName:
	push_error("DraftStrategy.recommend_next_pick() not implemented")
	return StringName("")
