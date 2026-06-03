class_name SatelliteSpec
extends RefCounted
## Configuration for a satellite tooltip panel.

var id: StringName
var content_builder: Callable
var data_source: Callable
var style_config: Dictionary
var position_hint: StringName
var on_show: Callable
var on_hide: Callable


func _init(
	p_id: StringName,
	p_content_builder: Callable,
	p_data_source: Callable,
	p_style_config: Dictionary = {},
	p_position_hint: StringName = &"grid",
	p_on_show: Callable = Callable(),
	p_on_hide: Callable = Callable()
) -> void:
	id = p_id
	content_builder = p_content_builder
	data_source = p_data_source
	style_config = p_style_config
	position_hint = p_position_hint
	on_show = p_on_show
	on_hide = p_on_hide
