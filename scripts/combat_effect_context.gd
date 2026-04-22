extends RefCounted
class_name CombatEffectContext

var unit: Object = null
var world: Object = null
var target: Object = null
var target_ally: Object = null
var damage: float = 0.0


func _init(new_unit: Object = null, new_world: Object = null, new_target: Object = null, new_target_ally: Object = null, new_damage: float = 0.0) -> void:
	unit = new_unit
	world = new_world
	target = new_target
	target_ally = new_target_ally
	damage = new_damage
