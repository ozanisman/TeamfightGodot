extends RefCounted
class_name ChampionCatalog

const HEROES: Array[Dictionary] = [
	{"id": "swordsman", "name": "Swordsman", "role": "fighter", "max_hp": 120.0, "attack_damage": 14.0, "attack_speed": 1.2, "attack_range": 0.3, "move_speed": 1.1, "armor": 0.25, "projectile_speed": 0.0},
	{"id": "archer", "name": "Archer", "role": "marksman", "max_hp": 75.0, "attack_damage": 21.0, "attack_speed": 0.9, "attack_range": 3.0, "move_speed": 1.0, "armor": 0.05, "projectile_speed": 8.0},
	{"id": "guardian", "name": "Guardian", "role": "tank", "max_hp": 220.0, "attack_damage": 10.0, "attack_speed": 0.8, "attack_range": 0.3, "move_speed": 1.1, "armor": 0.35, "projectile_speed": 0.0},
	{"id": "assassin", "name": "Assassin", "role": "assassin", "max_hp": 165.0, "attack_damage": 25.0, "attack_speed": 1.4, "attack_range": 0.3, "move_speed": 1.3, "armor": 0.05, "projectile_speed": 0.0},
	{"id": "mage", "name": "Mage", "role": "mage", "max_hp": 80.0, "attack_damage": 19.0, "attack_speed": 1.0, "attack_range": 3.5, "move_speed": 1.2, "armor": 0.05, "projectile_speed": 6.0},
	{"id": "sniper", "name": "Sniper", "role": "marksman", "max_hp": 60.0, "attack_damage": 21.0, "attack_speed": 0.4, "attack_range": 4.0, "move_speed": 0.9, "armor": 0.05, "projectile_speed": 12.0},
	{"id": "berserker", "name": "Berserker", "role": "fighter", "max_hp": 185.0, "attack_damage": 12.0, "attack_speed": 1.6, "attack_range": 0.3, "move_speed": 1.3, "armor": 0.15, "projectile_speed": 0.0},
	{"id": "paladin", "name": "Paladin", "role": "tank", "max_hp": 200.0, "attack_damage": 14.0, "attack_speed": 0.8, "attack_range": 0.3, "move_speed": 1.4, "armor": 0.25, "projectile_speed": 0.0},
	{"id": "rogue", "name": "Rogue", "role": "assassin", "max_hp": 90.0, "attack_damage": 18.0, "attack_speed": 1.8, "attack_range": 0.3, "move_speed": 2.4, "armor": 0.10, "projectile_speed": 0.0},
	{"id": "oracle", "name": "Oracle", "role": "support", "max_hp": 90.0, "attack_damage": 15.0, "attack_speed": 1.1, "attack_range": 3.5, "move_speed": 1.5, "armor": 0.05, "projectile_speed": 5.0},
	{"id": "colossus", "name": "Colossus", "role": "tank", "max_hp": 350.0, "attack_damage": 8.0, "attack_speed": 0.6, "attack_range": 0.3, "move_speed": 0.8, "armor": 0.30, "projectile_speed": 0.0},
	{"id": "wraith", "name": "Wraith", "role": "assassin", "max_hp": 155.0, "attack_damage": 26.0, "attack_speed": 1.5, "attack_range": 0.3, "move_speed": 2.5, "armor": 0.05, "projectile_speed": 0.0},
	{"id": "warlock", "name": "Warlock", "role": "mage", "max_hp": 100.0, "attack_damage": 21.0, "attack_speed": 0.9, "attack_range": 3.5, "move_speed": 1.4, "armor": 0.05, "projectile_speed": 5.0},
	{"id": "monk", "name": "Monk", "role": "fighter", "max_hp": 155.0, "attack_damage": 18.0, "attack_speed": 2.2, "attack_range": 0.3, "move_speed": 1.4, "armor": 0.10, "projectile_speed": 0.0},
	{"id": "artillery", "name": "Artillery", "role": "marksman", "max_hp": 55.0, "attack_damage": 25.0, "attack_speed": 0.4, "attack_range": 5.0, "move_speed": 0.3, "armor": 0.05, "projectile_speed": 3.0},
	{"id": "cleric", "name": "Cleric", "role": "support", "max_hp": 125.0, "attack_damage": 15.0, "attack_speed": 1.2, "attack_range": 3.5, "move_speed": 1.5, "armor": 0.10, "projectile_speed": 0.0},
	{"id": "siren", "name": "Siren", "role": "support", "max_hp": 85.0, "attack_damage": 15.0, "attack_speed": 1.1, "attack_range": 3.5, "move_speed": 1.6, "armor": 0.05, "projectile_speed": 0.0},
	{"id": "valkyrie", "name": "Valkyrie", "role": "fighter", "max_hp": 210.0, "attack_damage": 15.0, "attack_speed": 1.1, "attack_range": 0.3, "move_speed": 1.5, "armor": 0.20, "projectile_speed": 0.0},
]

const ROLE_ORDER: Array[String] = ["tank", "fighter", "assassin", "marksman", "mage", "support"]


static func get_all() -> Array[Dictionary]:
	return HEROES.duplicate(true)


static func get_by_id(hero_id: String) -> Dictionary:
	for hero in HEROES:
		if String(hero.get("id", "")) == hero_id:
			return hero
	return {}


static func power_score(hero: Dictionary) -> float:
	return float(hero.get("max_hp", 0.0)) * float(hero.get("attack_damage", 0.0)) * float(hero.get("attack_speed", 0.0))


static func available_heroes(
		player_picks: Array[String],
		enemy_picks: Array[String],
		banned_heroes: Array[String],
		active_role_filters: Array[String]
	) -> Array[Dictionary]:
	var taken := {}
	for hero_id in player_picks:
		taken[hero_id] = true
	for hero_id in enemy_picks:
		taken[hero_id] = true
	for hero_id in banned_heroes:
		taken[hero_id] = true

	var result: Array[Dictionary] = []
	for hero in HEROES:
		if taken.has(String(hero.get("id", ""))):
			continue
		var role := String(hero.get("role", ""))
		if not active_role_filters.is_empty() and not active_role_filters.has(role):
			continue
		result.append(hero)
	return result


static func choose_ai_pick(available: Array[Dictionary], enemy_picks: Array[String]) -> Dictionary:
	if available.is_empty():
		return {}

	var enemy_roles: Array[String] = []
	for hero_id in enemy_picks:
		var hero := get_by_id(hero_id)
		if not hero.is_empty():
			enemy_roles.append(String(hero.get("role", "")))

	var tanks := available.filter(func(hero: Dictionary) -> bool: return String(hero.get("role", "")) == "tank")
	if not enemy_roles.has("tank") and not tanks.is_empty():
		return _best_by_power(tanks)

	var has_ranged := false
	for role in enemy_roles:
		if role == "marksman" or role == "mage":
			has_ranged = true
			break
	if not has_ranged:
		var ranged := available.filter(func(hero: Dictionary) -> bool:
			var role := String(hero.get("role", ""))
			return role == "marksman" or role == "mage"
		)
		if not ranged.is_empty():
			return _best_by_power(ranged)

	return _best_by_power(available)


static func _best_by_power(heroes: Array[Dictionary]) -> Dictionary:
	var best := heroes[0]
	var best_score := power_score(best)
	for hero in heroes:
		var score := power_score(hero)
		if score > best_score:
			best = hero
			best_score = score
	return best
