class_name KitStats
extends RefCounted

var stat_mods: Dictionary = {}

func _init() -> void:
	pass

func add_stat_mod(stat_name: String, value: float, mod_type: String = "add") -> void:
	stat_mods[stat_name] = {
		"value": value,
		"type": mod_type
	}

func get_stat_mod(stat_name: String) -> Dictionary:
	return stat_mods.get(stat_name, {})

func has_stat_mod(stat_name: String) -> bool:
	return stat_mods.has(stat_name)

func to_dict() -> Dictionary:
	return stat_mods.duplicate(true)

func apply_to_stats(base_stats: Dictionary) -> Dictionary:
	var modified_stats: Dictionary = base_stats.duplicate(true)
	
	for stat_name in stat_mods:
		var mod_data: Dictionary = stat_mods[stat_name]
		var mod_type: String = mod_data.get("type", "add")
		var mod_value: float = mod_data.get("value", 0.0)
		var current_value: float = modified_stats.get(stat_name, 0.0)
		
		match mod_type:
			"add":
				modified_stats[stat_name] = current_value + mod_value
			"multiply":
				modified_stats[stat_name] = current_value * mod_value
			"subtract":
				modified_stats[stat_name] = current_value - mod_value
			"set":
				modified_stats[stat_name] = mod_value
			_:
				push_warning("Unknown stat mod type: %s for stat: %s" % [mod_type, stat_name])
	
	return modified_stats
