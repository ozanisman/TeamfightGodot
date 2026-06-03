class_name SatelliteContext
extends RefCounted
## Shared context object passed to all satellites for inter-satellite communication.

var main_tooltip_data: Dictionary
var satellite_states: Dictionary
var user_data: Dictionary


func _init(
	p_main_tooltip_data: Dictionary = {},
	p_satellite_states: Dictionary = {},
	p_user_data: Dictionary = {}
) -> void:
	main_tooltip_data = p_main_tooltip_data
	satellite_states = p_satellite_states
	user_data = p_user_data


## Get the state of a specific satellite by ID.
func get_satellite_state(satellite_id: StringName) -> Variant:
	return satellite_states.get(satellite_id)


## Set the state of a specific satellite by ID.
func set_satellite_state(satellite_id: StringName, state: Variant) -> void:
	satellite_states[satellite_id] = state


## Get user-defined data by key.
func get_user_data(key: StringName) -> Variant:
	return user_data.get(key)


## Set user-defined data by key.
func set_user_data(key: StringName, value: Variant) -> void:
	user_data[key] = value


## Get main tooltip data by key.
func get_main_data(key: StringName) -> Variant:
	return main_tooltip_data.get(key)


## Set main tooltip data by key.
func set_main_data(key: StringName, value: Variant) -> void:
	main_tooltip_data[key] = value
