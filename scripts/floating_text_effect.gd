extends RefCounted
class_name FloatingTextEffect

var text: String = ""
var world_pos: Vector2 = Vector2.ZERO
var color: Color = Color.WHITE
var lifetime: float = 1.0
var velocity: Vector2 = Vector2(0.0, -0.4)


func _init(new_text: String = "", new_world_pos: Vector2 = Vector2.ZERO, new_color: Color = Color.WHITE, new_lifetime: float = 1.0) -> void:
	text = new_text
	world_pos = new_world_pos
	color = new_color
	lifetime = new_lifetime


func step(dt: float) -> void:
	lifetime = maxf(0.0, lifetime - dt)
	world_pos += velocity * dt


func is_alive() -> bool:
	return lifetime > 0.0
