class_name KitSpec
extends RefCounted

const EffectSpecScript := preload("res://scripts/simulation/effect_spec.gd")

var ability: EffectSpecScript = null
var ultimate: EffectSpecScript = null
var passive_ids: Array[StringName] = []

func _init() -> void:
	pass

func to_dict() -> Dictionary:
	var result: Dictionary = {}
	
	if ability != null:
		result["ability"] = _effect_to_dict(ability)
	
	if ultimate != null:
		result["ultimate"] = _effect_to_dict(ultimate)
	
	if not passive_ids.is_empty():
		result["passive_ids"] = passive_ids
	
	return result

func _effect_to_dict(effect: EffectSpecScript) -> Dictionary:
	var effect_dict: Dictionary = {
		"kind": effect.kind,
		"params": effect.params.duplicate(true),
		"cast_range": effect.cast_range
	}
	
	# Handle nested effects
	var params = effect_dict["params"]
	for key in params:
		var value = params[key]
		if key == "effects" and value is Array:
			var nested_effects: Array = []
			for nested_effect in value:
				if nested_effect is EffectSpecScript:
					nested_effects.append(_effect_to_dict(nested_effect))
			params[key] = nested_effects
		elif key == "splash" and value is EffectSpecScript:
			params[key] = _effect_to_dict(value)
	
	return effect_dict
