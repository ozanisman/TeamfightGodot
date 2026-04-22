extends Node2D
class_name DummyTarget

var display_name: String = "Dummy"
var role: String = "dummy"
var team: String = "enemy"
var world_pos: Vector2 = Vector2.ZERO
var max_hp: float = 1000.0
var hp: float = 1000.0
var shield: float = 0.0
var mana: float = 0.0
var alive: bool = true
var instance_id: int = 0
var current_target = null
var current_ally_target = null
var spawn_pos: Vector2 = Vector2.ZERO
var incoming_target_count: int = 0
var perceived_threat: float = 0.0


func is_alive() -> bool:
	return alive and hp > 0.0


func update(_dt: float, _world: Object) -> void:
	pass


func set_world_position(pos: Vector2) -> void:
	global_position = pos
	world_pos = pos


func set_spawn_position(pos: Vector2) -> void:
	spawn_pos = pos


func take_damage(amount: float, _world: Object, _damage_type: String = "true", _source_id: int = -1) -> Dictionary:
	var absorbed := minf(shield, amount)
	shield -= absorbed
	var hp_loss := maxf(0.0, amount - absorbed)
	hp = maxf(0.0, hp - hp_loss)
	if hp <= 0.0:
		alive = false
	return {"absorbed": absorbed, "hp_loss": hp_loss}


func apply_stun(_duration: float, _world: Object, _source_id: int = -1) -> void:
	pass


func apply_taunt(_duration: float, _source_id: int) -> void:
	pass


func heal(amount: float, _world: Object, _source_id: int = -1) -> void:
	hp = minf(max_hp, hp + amount)


func add_shield(amount: float, _world: Object, _source_id: int = -1) -> void:
	shield += amount
