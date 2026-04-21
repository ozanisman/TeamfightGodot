extends RefCounted
class_name CombatEffectContext

var unit: Node2D = null
var world: Object = null
var target: Node2D = null
var target_ally: Node2D = null
var damage: float = 0.0


func _init(new_unit: Node2D = null, new_world: Object = null, new_target: Node2D = null, new_target_ally: Node2D = null, new_damage: float = 0.0) -> void:
	unit = new_unit
	world = new_world
	target = new_target
	target_ally = new_target_ally
	damage = new_damage
