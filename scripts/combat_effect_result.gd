extends RefCounted
class_name CombatEffectResult

const CombatData = preload("res://scripts/combat_data.gd")

var use_projectile: bool = false
var damage: float = 0.0
var damage_type: String = "physical"
var reason: String = ""
var projectile_speed: float = CombatData.DEFAULT_PROJECTILE_SPEED
var projectile_radius: float = CombatData.DEFAULT_PROJECTILE_RADIUS
var stun_duration: float = 0.0
var splash_radius: float = 0.0
var splash_ratio: float = 0.0
var healing: float = 0.0
var shield_added: float = 0.0
var shield_absorbed: float = 0.0
var total_impact: float = 0.0
var taunted_count: int = 0
var emit_event: bool = false
var event_target: Object = null


func is_empty() -> bool:
	return not emit_event and not use_projectile and damage == 0.0 and healing == 0.0 and shield_added == 0.0 and shield_absorbed == 0.0 and total_impact == 0.0 and taunted_count == 0 and reason.is_empty()


func merge(other: CombatEffectResult) -> void:
	if other == null:
		return
	use_projectile = use_projectile or other.use_projectile
	damage += other.damage
	healing += other.healing
	shield_added += other.shield_added
	shield_absorbed += other.shield_absorbed
	total_impact += other.total_impact
	taunted_count += other.taunted_count
	emit_event = emit_event or other.emit_event
	if other.event_target != null:
		event_target = other.event_target
	if other.reason != "":
		reason = other.reason
	damage_type = other.damage_type if other.damage_type != "" else damage_type
	projectile_speed = other.projectile_speed
	projectile_radius = other.projectile_radius
	stun_duration = other.stun_duration
	splash_radius = other.splash_radius
	splash_ratio = other.splash_ratio
