class_name EffectSpec
extends RefCounted

var kind: StringName = &""
var params: Dictionary = {}
## Cast range gate: -1 = use attack range, 0 = no gate, >0 = max distance.
var cast_range: float = -1.0

func _init(_kind: StringName = &"", _params: Dictionary = {}, _cast_range: float = -1.0) -> void:
	kind = _kind
	params = _params.duplicate(true)
	cast_range = _cast_range

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
		"cast_range": cast_range,
	}

static func from_dict(data: Dictionary):
	var effect := new()
	effect.kind = StringName(String(data.get("kind", "")))
	effect.params = Dictionary(data.get("params", {}))
	effect.cast_range = float(data.get("cast_range", -1.0))
	return effect
