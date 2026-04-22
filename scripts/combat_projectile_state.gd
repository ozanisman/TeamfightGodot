extends RefCounted
class_name CombatProjectileState

const CombatData = preload("res://scripts/combat_data.gd")

var attacker_id: int = -1
var target_id: int = -1
var team: String = ""
var pos: Vector2 = Vector2.ZERO
var speed: float = 0.0
var radius: float = 0.0
var damage: float = 0.0
var damage_type: String = "physical"
var reason: String = ""
var stun_duration: float = 0.0
var is_ability: bool = false
var is_ultimate: bool = false
var splash_radius: float = 0.0
var splash_ratio: float = 0.0
var source_attack_range: float = 0.0
var source_projectile_radius: float = CombatData.DEFAULT_PROJECTILE_RADIUS
