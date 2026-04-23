class_name ChampionStats
extends RefCounted

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

var unit_id: StringName = &""
var name: StringName = &""
var max_hp: float = 0.0
var attack_damage: float = 0.0
var attack_range: float = 0.0
var attack_speed: float = 0.0
var move_speed: float = 0.0
var role: StringName = &""
var armor: float = 0.0
var magic_resist: float = 0.0
var tenacity: float = 0.0
var life_steal: float = 0.0
var max_mana: float = 50.0
var mana_per_attack: float = 10.0
var ability_cd: float = 5.0
var ultimate_cd: float = 20.0
var projectile_speed: float = SimConstantsScript.DEFAULT_PROJECTILE_SPEED
var projectile_radius: float = SimConstantsScript.DEFAULT_PROJECTILE_RADIUS
var passive_id: StringName = &""
var respawn_time: float = SimConstantsScript.RESPAWN_TIME

func to_dict() -> Dictionary:
	return {
		"unit_id": String(unit_id),
		"name": String(name),
		"max_hp": max_hp,
		"attack_damage": attack_damage,
		"attack_range": attack_range,
		"attack_speed": attack_speed,
		"move_speed": move_speed,
		"role": String(role),
		"armor": armor,
		"magic_resist": magic_resist,
		"tenacity": tenacity,
		"life_steal": life_steal,
		"max_mana": max_mana,
		"mana_per_attack": mana_per_attack,
		"ability_cd": ability_cd,
		"ultimate_cd": ultimate_cd,
		"projectile_speed": projectile_speed,
		"projectile_radius": projectile_radius,
		"passive_id": String(passive_id),
		"respawn_time": respawn_time,
	}

static func from_dict(data: Dictionary):
	var stats := new()
	stats.unit_id = StringName(String(data.get("unit_id", "")))
	stats.name = StringName(String(data.get("name", "")))
	stats.max_hp = float(data.get("max_hp", 0.0))
	stats.attack_damage = float(data.get("attack_damage", 0.0))
	stats.attack_range = float(data.get("attack_range", 0.0))
	stats.attack_speed = float(data.get("attack_speed", 0.0))
	stats.move_speed = float(data.get("move_speed", 0.0))
	stats.role = StringName(String(data.get("role", "")))
	stats.armor = float(data.get("armor", 0.0))
	stats.magic_resist = float(data.get("magic_resist", 0.0))
	stats.tenacity = float(data.get("tenacity", 0.0))
	stats.life_steal = float(data.get("life_steal", 0.0))
	stats.max_mana = float(data.get("max_mana", 50.0))
	stats.mana_per_attack = float(data.get("mana_per_attack", 10.0))
	stats.ability_cd = float(data.get("ability_cd", 5.0))
	stats.ultimate_cd = float(data.get("ultimate_cd", 20.0))
	stats.projectile_speed = float(data.get("projectile_speed", SimConstantsScript.DEFAULT_PROJECTILE_SPEED))
	stats.projectile_radius = float(data.get("projectile_radius", SimConstantsScript.DEFAULT_PROJECTILE_RADIUS))
	stats.passive_id = StringName(String(data.get("passive_id", "")))
	stats.respawn_time = float(data.get("respawn_time", SimConstantsScript.RESPAWN_TIME))
	return stats
