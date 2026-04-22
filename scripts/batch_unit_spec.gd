extends RefCounted
class_name BatchUnitSpec

const CombatData = preload("res://scripts/combat_data.gd")
const CombatUnitStateScript = preload("res://scripts/combat_unit_state.gd")

var hero_id: String = ""
var display_name: String = ""
var role: String = ""
var team: String = ""
var passive_id: String = ""

var spawn_position: Vector2 = Vector2.ZERO

var max_hp: float = 1.0
var attack_damage: float = 0.0
var attack_range: float = 0.3
var attack_speed: float = 1.0
var move_speed: float = 1.0
var armor: float = 0.0
var projectile_speed: float = CombatData.DEFAULT_PROJECTILE_SPEED
var magic_resist: float = 0.0
var tenacity: float = 0.0
var life_steal: float = 0.0
var respawn_time: float = CombatData.RESPAWN_TIME
var mana_per_attack: float = 0.0
var max_mana: float = 0.0
var ability_cd: float = 0.0
var ultimate_cd: float = 0.0
var projectile_radius: float = CombatData.DEFAULT_PROJECTILE_RADIUS


static func from_hero(hero: Dictionary, team_name: String, index: int):
	var spec := new()
	spec.hero_id = String(hero.get("id", ""))
	spec.display_name = String(hero.get("name", spec.hero_id))
	spec.role = String(hero.get("role", ""))
	spec.team = team_name
	spec.passive_id = String(hero.get("passive_id", ""))
	spec.spawn_position = _initial_world_position(index, team_name)
	spec.max_hp = float(hero.get("max_hp", 1.0))
	spec.attack_damage = float(hero.get("attack_damage", 0.0))
	spec.attack_range = float(hero.get("attack_range", 0.3))
	spec.attack_speed = float(hero.get("attack_speed", 1.0))
	spec.move_speed = float(hero.get("move_speed", 1.0))
	spec.armor = float(hero.get("armor", 0.0))
	spec.projectile_speed = float(hero.get("projectile_speed", CombatData.DEFAULT_PROJECTILE_SPEED))
	spec.magic_resist = float(hero.get("magic_resist", 0.0))
	spec.tenacity = float(hero.get("tenacity", 0.0))
	spec.life_steal = float(hero.get("life_steal", 0.0))
	spec.respawn_time = float(hero.get("respawn_time", CombatData.RESPAWN_TIME))
	spec.mana_per_attack = float(hero.get("mana_per_attack", 0.0))
	spec.max_mana = float(hero.get("max_mana", 0.0))
	spec.ability_cd = float(hero.get("ability_cd", 0.0))
	spec.ultimate_cd = float(hero.get("ultimate_cd", 0.0))
	spec.projectile_radius = float(hero.get("projectile_radius", CombatData.DEFAULT_PROJECTILE_RADIUS))
	return spec


static func from_dictionary(data: Dictionary):
	var spec := new()
	spec.hero_id = String(data.get("hero_id", ""))
	spec.display_name = String(data.get("display_name", spec.hero_id))
	spec.role = String(data.get("role", ""))
	spec.team = String(data.get("team", ""))
	spec.passive_id = String(data.get("passive_id", ""))
	spec.spawn_position = data.get("spawn_position", Vector2.ZERO)
	spec.max_hp = float(data.get("max_hp", 1.0))
	spec.attack_damage = float(data.get("attack_damage", 0.0))
	spec.attack_range = float(data.get("attack_range", 0.3))
	spec.attack_speed = float(data.get("attack_speed", 1.0))
	spec.move_speed = float(data.get("move_speed", 1.0))
	spec.armor = float(data.get("armor", 0.0))
	spec.projectile_speed = float(data.get("projectile_speed", CombatData.DEFAULT_PROJECTILE_SPEED))
	spec.magic_resist = float(data.get("magic_resist", 0.0))
	spec.tenacity = float(data.get("tenacity", 0.0))
	spec.life_steal = float(data.get("life_steal", 0.0))
	spec.respawn_time = float(data.get("respawn_time", CombatData.RESPAWN_TIME))
	spec.mana_per_attack = float(data.get("mana_per_attack", 0.0))
	spec.max_mana = float(data.get("max_mana", 0.0))
	spec.ability_cd = float(data.get("ability_cd", 0.0))
	spec.ultimate_cd = float(data.get("ultimate_cd", 0.0))
	spec.projectile_radius = float(data.get("projectile_radius", CombatData.DEFAULT_PROJECTILE_RADIUS))
	return spec


func to_dictionary() -> Dictionary:
	return {
		"hero_id": hero_id,
		"display_name": display_name,
		"role": role,
		"team": team,
		"passive_id": passive_id,
		"spawn_position": spawn_position,
		"max_hp": max_hp,
		"attack_damage": attack_damage,
		"attack_range": attack_range,
		"attack_speed": attack_speed,
		"move_speed": move_speed,
		"armor": armor,
		"projectile_speed": projectile_speed,
		"magic_resist": magic_resist,
		"tenacity": tenacity,
		"life_steal": life_steal,
		"respawn_time": respawn_time,
		"mana_per_attack": mana_per_attack,
		"max_mana": max_mana,
		"ability_cd": ability_cd,
		"ultimate_cd": ultimate_cd,
		"projectile_radius": projectile_radius,
	}


func to_combat_unit_state(metrics: Object) -> CombatUnitState:
	var unit: CombatUnitState = CombatUnitStateScript.new()
	unit.set_identity(
		hero_id,
		display_name,
		role,
		team,
		Color(0.35, 0.60, 1.0) if team == "player" else Color(0.92, 0.35, 0.35),
		false,
		passive_id
	)
	unit.set_combat_stats(
		max_hp,
		attack_damage,
		attack_range,
		attack_speed,
		move_speed,
		armor,
		projectile_speed,
		magic_resist,
		tenacity,
		life_steal,
		respawn_time,
		mana_per_attack,
		max_mana,
		ability_cd,
		ultimate_cd,
		projectile_radius
	)
	unit.set_spawn_position(spawn_position)
	unit.apply_combat_metrics(metrics)
	unit.set_world_position(spawn_position)
	unit.set_collision_enabled(true)
	return unit


static func _initial_world_position(index: int, team_name: String) -> Vector2:
	var row := floori(index / 2)
	var col := index % 2
	var x := 1.0 + float(col) * 1.15
	var y := 2.0 + float(row) * 2.0
	var world_point := Vector2(x, y)
	if team_name == "enemy":
		world_point.x = 9.0 - float(col) * 1.15
		world_point.x = clampf(world_point.x, CombatData.WORLD_SIZE - 4.5, CombatData.WORLD_SIZE - 0.5)
		return Vector2(clampf(world_point.x, CombatData.WORLD_SIZE - 4.5, CombatData.WORLD_SIZE - 0.5), clampf(world_point.y, 0.5, 9.5))
	return Vector2(clampf(world_point.x, 0.5, 4.5), clampf(world_point.y, 0.5, 9.5))
