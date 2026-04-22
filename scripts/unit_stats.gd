extends Resource
class_name UnitStats

@export var unit_id: String = ""
@export var name: String = ""
@export var max_hp: float = 0.0
@export var attack_damage: float = 0.0
@export var attack_range: float = 0.0
@export var attack_speed: float = 0.0
@export var move_speed: float = 0.0
@export var role: String = ""
@export var armor: float = 0.0
@export var magic_resist: float = 0.0
@export var tenacity: float = 0.0
@export var life_steal: float = 0.0
@export var max_mana: float = 50.0
@export var mana_per_attack: float = 10.0
@export var ability_cd: float = 5.0
@export var ultimate_cd: float = 20.0
@export var projectile_speed: float = 0.0
@export var projectile_radius: float = 0.0
@export var passive_id: String = ""
@export var respawn_time: float = 10.0


static func from_dict(data: Dictionary) -> UnitStats:
	var stats := UnitStats.new()
	stats.unit_id = String(data.get("unit_id", ""))
	stats.name = String(data.get("name", ""))
	stats.max_hp = float(data.get("max_hp", 0.0))
	stats.attack_damage = float(data.get("attack_damage", 0.0))
	stats.attack_range = float(data.get("attack_range", 0.0))
	stats.attack_speed = float(data.get("attack_speed", 0.0))
	stats.move_speed = float(data.get("move_speed", 0.0))
	stats.role = String(data.get("role", ""))
	stats.armor = float(data.get("armor", 0.0))
	stats.magic_resist = float(data.get("magic_resist", 0.0))
	stats.tenacity = float(data.get("tenacity", 0.0))
	stats.life_steal = float(data.get("life_steal", 0.0))
	stats.max_mana = float(data.get("max_mana", 50.0))
	stats.mana_per_attack = float(data.get("mana_per_attack", 10.0))
	stats.ability_cd = float(data.get("ability_cd", 5.0))
	stats.ultimate_cd = float(data.get("ultimate_cd", 20.0))
	stats.projectile_speed = float(data.get("projectile_speed", 0.0))
	stats.projectile_radius = float(data.get("projectile_radius", 0.0))
	stats.passive_id = String(data.get("passive_id", ""))
	stats.respawn_time = float(data.get("respawn_time", 10.0))
	return stats
