## Random draft pick strategy — baseline for A/B testing.

func get_strategy_name() -> String:
	return "random"


func recommend_next_pick(allies: Array, enemies: Array, available: Array, draft_step: int = -1) -> StringName:
	if available.is_empty():
		return StringName("")
	var idx := randi() % available.size()
	return StringName(available[idx])


func recommend_next_ban(allies: Array, enemies: Array, available: Array, draft_step: int = -1, side: String = "blue", weight_overrides: Dictionary = {}) -> StringName:
	if available.is_empty():
		return StringName("")
	var idx := randi() % available.size()
	return StringName(available[idx])
