extends RefCounted
class_name ChampionCatalog

const CombatRegistryScript = preload("res://scripts/combat_registry.gd")
const CombatEffectScript = preload("res://scripts/combat_effect.gd")
const CombatData = preload("res://scripts/combat_data.gd")

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
	var result: Array[Dictionary] = []
	for hero in HEROES:
		result.append(_augment_hero(hero))
	return result


static func get_by_id(hero_id: String) -> Dictionary:
	for hero in HEROES:
		if String(hero.get("id", "")) == hero_id:
			return _augment_hero(hero)
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
		result.append(_augment_hero(hero))
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


static func bootstrap_combat_registry(registry: Object) -> void:
	if registry == null:
		return
	registry.call("reset")
	_register_role_effects(registry)


static func _augment_hero(hero: Dictionary) -> Dictionary:
	var result := hero.duplicate(true)
	var role := String(result.get("role", ""))
	var hero_id := String(result.get("id", ""))
	result.merge(_defaults_for(role, hero_id), true)
	return result


static func _defaults_for(role: String, hero_id: String) -> Dictionary:
	match role:
		"tank":
			return {
				"max_mana": 100.0,
				"mana_per_attack": 10.0,
				"ability_cd": 10.0,
				"ultimate_cd": 40.0,
				"magic_resist": 0.25,
				"tenacity": 0.20,
				"respawn_time": CombatData.RESPAWN_TIME,
				"projectile_radius": CombatData.DEFAULT_PROJECTILE_RADIUS,
				"passive_id": "%s_tank" % hero_id,
			}
		"fighter":
			return {
				"max_mana": 60.0,
				"mana_per_attack": 10.0,
				"ability_cd": 5.0,
				"ultimate_cd": 18.0,
				"magic_resist": 0.10,
				"tenacity": 0.10,
				"life_steal": 0.10,
				"respawn_time": CombatData.RESPAWN_TIME,
				"projectile_radius": CombatData.DEFAULT_PROJECTILE_RADIUS,
				"passive_id": "%s_fighter" % hero_id,
			}
		"assassin":
			return {
				"max_mana": 60.0,
				"mana_per_attack": 10.0,
				"ability_cd": 4.0,
				"ultimate_cd": 20.0,
				"magic_resist": 0.05,
				"respawn_time": CombatData.RESPAWN_TIME,
				"projectile_radius": CombatData.DEFAULT_PROJECTILE_RADIUS,
				"passive_id": "%s_assassin" % hero_id,
			}
		"marksman":
			return {
				"max_mana": 100.0,
				"mana_per_attack": 10.0,
				"ability_cd": 8.0,
				"ultimate_cd": 30.0,
				"magic_resist": 0.05,
				"respawn_time": CombatData.RESPAWN_TIME,
				"projectile_radius": CombatData.DEFAULT_PROJECTILE_RADIUS,
				"passive_id": "%s_marksman" % hero_id,
			}
		"mage":
			return {
				"max_mana": 80.0,
				"mana_per_attack": 10.0,
				"ability_cd": 7.0,
				"ultimate_cd": 22.0,
				"magic_resist": 0.25,
				"respawn_time": CombatData.RESPAWN_TIME,
				"projectile_radius": CombatData.DEFAULT_PROJECTILE_RADIUS,
				"passive_id": "%s_mage" % hero_id,
			}
		"support":
			return {
				"max_mana": 100.0,
				"mana_per_attack": 10.0,
				"ability_cd": 6.0,
				"ultimate_cd": 24.0,
				"magic_resist": 0.20,
				"respawn_time": CombatData.RESPAWN_TIME,
				"projectile_radius": CombatData.DEFAULT_PROJECTILE_RADIUS,
				"passive_id": "%s_support" % hero_id,
			}
		_:
			return {
				"max_mana": 50.0,
				"mana_per_attack": 10.0,
				"ability_cd": 5.0,
				"ultimate_cd": 20.0,
				"magic_resist": 0.0,
				"respawn_time": CombatData.RESPAWN_TIME,
				"projectile_radius": CombatData.DEFAULT_PROJECTILE_RADIUS,
				"passive_id": hero_id,
			}


static func _register_role_effects(registry: Object) -> void:
	registry.call("register_ability", "tank", _effect_shield(0.0, 0.2, "Guardian Shield"))
	registry.call("register_ultimate", "tank", _effect_multi([
		_effect_damage(2.0, "physical", "Aegis Crash"),
		_effect_stun(3.0, "Aegis Crash"),
	], "Aegis Crash"))
	registry.call("register_passive", "on_defense", "tank", _effect_constant_multiplier(0.9))
	registry.call("register_passive", "post_take_damage", "tank", _effect_post_damage_mana_gain(0.10))

	registry.call("register_ability", "fighter", _effect_multi([
		_effect_damage(1.2, "physical", "Precision Strike"),
		_effect_stun(0.5, "Precision Strike"),
	], "Precision Strike"))
	registry.call("register_ultimate", "fighter", _effect_multi([
		_effect_damage(2.5, "physical", "Fury Burst"),
		_effect_stun(1.5, "Fury Burst"),
	], "Fury Burst"))
	registry.call("register_passive", "on_attack", "fighter", _effect_constant_multiplier(1.15))

	registry.call("register_ability", "assassin", _effect_multi([
		_effect_damage(1.2, "physical", "Shadow Strike"),
		_effect_stun(0.5, "Shadow Strike"),
	], "Shadow Strike"))
	registry.call("register_ultimate", "assassin", _effect_damage(3.5, "physical", "Execution"))
	registry.call("register_passive", "on_attack", "assassin", _effect_target_hp_threshold_multiplier(0.30, 2.0))

	registry.call("register_ability", "marksman", _effect_projectile(1.5, "physical", "Focused Shot", 8.0, 0.03))
	registry.call("register_ultimate", "marksman", _effect_projectile(4.0, "physical", "Rain of Arrows", 10.0, 0.05))
	registry.call("register_passive", "on_attack", "marksman", _effect_distance_threshold_multiplier(3.0, 1.25))

	registry.call("register_ability", "mage", _effect_projectile(2.0, "magic", "Arcane Bolt", 7.0, 0.03))
	registry.call("register_ultimate", "mage", _effect_projectile(6.0, "magic", "Meteor Bombardment", 4.0, 0.07, 0.0, 2.5, 0.5))
	registry.call("register_passive", "on_tick", "mage", _effect_mana_regen(3.0))

	registry.call("register_ability", "support", _effect_heal(0.0, 0.2, "Purify"))
	registry.call("register_ultimate", "support", _effect_shield(0.0, 0.4, "Divine Shield"))
	registry.call("register_passive", "post_attack", "support", _effect_mana_restore_on_hit(5.0))


static func _effect_multi(effects: Array, effect_reason: String = "") -> Object:
	return CombatEffectScript.new().setup_multi(effects, effect_reason)


static func _effect_damage(multiplier: float, damage_type: String = "physical", effect_reason: String = "", stun_duration: float = 0.0) -> Object:
	return CombatEffectScript.new().setup_damage(multiplier, damage_type, effect_reason, stun_duration)


static func _effect_heal(flat_amount: float = 0.0, max_hp_ratio: float = 0.0, effect_reason: String = "") -> Object:
	return CombatEffectScript.new().setup_heal(flat_amount, max_hp_ratio, effect_reason)


static func _effect_shield(flat_amount: float = 0.0, max_hp_ratio: float = 0.0, effect_reason: String = "") -> Object:
	return CombatEffectScript.new().setup_shield(flat_amount, max_hp_ratio, effect_reason)


static func _effect_stun(duration: float, effect_reason: String = "") -> Object:
	return CombatEffectScript.new().setup_stun(duration, effect_reason)


static func _effect_projectile(multiplier: float, damage_type: String = "physical", effect_reason: String = "", projectile_speed: float = CombatData.DEFAULT_PROJECTILE_SPEED, projectile_radius: float = CombatData.DEFAULT_PROJECTILE_RADIUS, stun_duration: float = 0.0, splash_radius: float = 0.0, splash_ratio: float = 0.0) -> Object:
	return CombatEffectScript.new().setup_projectile(multiplier, damage_type, effect_reason, projectile_speed, projectile_radius, stun_duration, splash_radius, splash_ratio)


static func _effect_constant_multiplier(multiplier: float) -> Object:
	return CombatEffectScript.new().setup_constant_multiplier(multiplier)


static func _effect_target_hp_threshold_multiplier(threshold: float, multiplier: float) -> Object:
	return CombatEffectScript.new().setup_target_hp_threshold_multiplier(threshold, multiplier)


static func _effect_distance_threshold_multiplier(threshold: float, multiplier: float) -> Object:
	return CombatEffectScript.new().setup_distance_threshold_multiplier(threshold, multiplier)


static func _effect_mana_regen(flat_amount: float) -> Object:
	return CombatEffectScript.new().setup_mana_regen(flat_amount)


static func _effect_mana_restore_on_hit(flat_amount: float) -> Object:
	return CombatEffectScript.new().setup_mana_restore_on_hit(flat_amount)


static func _effect_post_damage_mana_gain(damage_ratio: float) -> Object:
	return CombatEffectScript.new().setup_post_damage_mana_gain(damage_ratio)


static func _best_by_power(heroes: Array[Dictionary]) -> Dictionary:
	var best := heroes[0]
	var best_score := power_score(best)
	for hero in heroes:
		var score := power_score(hero)
		if score > best_score:
			best = hero
			best_score = score
	return best
