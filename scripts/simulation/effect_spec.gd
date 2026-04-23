class_name EffectSpec
extends RefCounted

var kind: StringName = &""
var params: Dictionary = {}

func _init(_kind: StringName = &"", _params: Dictionary = {}) -> void:
	kind = _kind
	params = _params.duplicate(true)

func to_dict() -> Dictionary:
	return {
		"kind": String(kind),
		"params": params.duplicate(true),
	}

static func from_dict(data: Dictionary):
	var effect := new()
	effect.kind = StringName(String(data.get("kind", "")))
	effect.params = Dictionary(data.get("params", {}))
	return effect
