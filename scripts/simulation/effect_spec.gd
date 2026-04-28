class_name EffectSpec
extends RefCounted

var kind: StringName = &""
var params: Dictionary = {}
var requires_target_in_range: bool = true

func _init(_kind: StringName = &"", _params: Dictionary = {}, _requires_target_in_range: bool = true) -> void:
	kind = _kind
	params = _params.duplicate(true)
	requires_target_in_range = _requires_target_in_range

func to_dict() -> Dictionary:
	var normalized_params: Dictionary = {}
	for key in params:
		var value = params[key]
		if value is EffectSpec:
			normalized_params[key] = value.to_dict()
		elif value is Array:
			var normalized_array: Array = []
			for item in value:
				if item is EffectSpec:
					normalized_array.append(item.to_dict())
				else:
					normalized_array.append(item)
			normalized_params[key] = normalized_array
		elif value is Dictionary:
			var normalized_dict: Dictionary = {}
			for dict_key in value:
				var dict_value = value[dict_key]
				if dict_value is EffectSpec:
					normalized_dict[dict_key] = dict_value.to_dict()
				elif dict_value is Array:
					var normalized_nested_array: Array = []
					for nested_item in dict_value:
						if nested_item is EffectSpec:
							normalized_nested_array.append(nested_item.to_dict())
						else:
							normalized_nested_array.append(nested_item)
					normalized_dict[dict_key] = normalized_nested_array
				else:
					normalized_dict[dict_key] = dict_value
			normalized_params[key] = normalized_dict
		else:
			normalized_params[key] = value
	return {
		"kind": String(kind),
		"params": normalized_params,
		"requires_target_in_range": requires_target_in_range,
	}

static func from_dict(data: Dictionary):
	var effect := new()
	effect.kind = StringName(String(data.get("kind", "")))
	effect.params = Dictionary(data.get("params", {}))
	effect.requires_target_in_range = bool(data.get("requires_target_in_range", true))
	return effect
