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

const HERO_DETAILS := {
	"swordsman": {
		"description": "A balanced frontline duelist who excels at locking down single targets with concussive strikes.",
		"ability_desc": "Deals 150% damage and stuns the target for 1.0s.",
		"ultimate_desc": "Strikes for 300% damage and applies a 2.0s stun.",
		"passive_desc": "Increases attack damage by 20%.",
		"passive_id": "duelist",
	},
	"archer": {
		"description": "A classic long-range scout that provides consistent physical DPS from the backline.",
		"ability_desc": "Fires a focused shot dealing 150% damage.",
		"ultimate_desc": "Rains down arrows for 400% massive physical damage and deals splash damage on impact.",
		"passive_desc": "Increases attack damage by 25%. Rain of Arrows deals 50% splash damage in a 2.0 unit radius.",
		"passive_id": "eagle_eye",
	},
	"guardian": {
		"description": "A hulking protector who uses heavy shields to soak damage and a massive slam to stun entire groups.",
		"ability_desc": "Grants a shield worth 20% max HP to self or an ally.",
		"ultimate_desc": "Slams the ground for 200% damage and a 3.0s stun.",
		"passive_desc": "Reduces incoming damage by 10%.",
		"passive_id": "bastion",
	},
	"assassin": {
		"description": "A high-mobility predator designed to dive the backline and execute wounded targets with lethal precision.",
		"ability_desc": "Quick dash for 120% damage and a short 0.5s stun.",
		"ultimate_desc": "Executes a target with 500% physical damage.",
		"passive_desc": "Deals double damage to targets below 30% HP.",
		"passive_id": "executioner",
	},
	"mage": {
		"description": "A powerful spellcaster with a deep mana pool, capable of unleashing frequent bursts of magic damage.",
		"ability_desc": "Fires a magical projectile dealing 200% magic damage.",
		"ultimate_desc": "Calls down a meteor for 600% magic damage and deals splash damage on impact.",
		"passive_desc": "Restores 3 mana every second. Meteor impact deals 50% splash damage in a 2.5 unit radius.",
		"passive_id": "mana_font",
	},
	"sniper": {
		"description": "An elite marksman with extreme range, specializing in devastating single-target ultimate shots.",
		"ability_desc": "High-precision strike for 180% damage.",
		"ultimate_desc": "Lethal long-range shot for 350% damage.",
		"passive_desc": "Deals 25% bonus damage to targets further than 3 units away.",
		"passive_id": "marksman",
	},
	"berserker": {
		"description": "A savage warrior who thrives in the heat of battle, healing himself through sheer aggression.",
		"ability_desc": "Damages self for 10% max HP and gains 150% of the value as a shield.",
		"ultimate_desc": "Unleashes a devastating strike for 300% true damage.",
		"passive_desc": "Heals for 15% of all damage dealt by auto-attacks.",
		"passive_id": "bloodlust",
	},
	"paladin": {
		"description": "A holy knight who balances immense durability with powerful self-healing and divine judgment.",
		"ability_desc": "Heals self for 15% max HP.",
		"ultimate_desc": "Heals self for 30% max HP and deals 200% damage.",
		"passive_desc": "Regenerates 5 HP every second.",
		"passive_id": "rejuvenation",
	},
	"rogue": {
		"description": "An evasive melee fighter who relies on high dodge chance and speed to survive encounters.",
		"ability_desc": "Deals 80% magic damage and stuns for 2.0s.",
		"ultimate_desc": "Lethal execution dealing 350% physical damage.",
		"passive_desc": "Grants a 25% chance to dodge incoming damage.",
		"passive_id": "agility",
	},
	"oracle": {
		"description": "A mystical seer who converts mana into healing, sustaining allies through long-range purification.",
		"ability_desc": "Heals an ally or self for 20% max HP.",
		"ultimate_desc": "Shields an ally or self for 40% max HP.",
		"passive_desc": "Restores 5 mana after each auto-attack.",
		"passive_id": "enlightenment",
	},
	"colossus": {
		"description": "A mountain of a tank with unmatched physical resistance and the ability to stun the entire battlefield.",
		"ability_desc": "Deals 100% damage and taunts all targets in a 1 unit radius for 2.0s.",
		"ultimate_desc": "Colossal impact for 250% damage and a 3.5s stun.",
		"passive_desc": "Reduces all incoming damage by 10%.",
		"passive_id": "tenacity",
	},
	"wraith": {
		"description": "A terrifying spectral assassin that stuns its prey and moves faster than the eye can see.",
		"ability_desc": "Magic strike for 120% damage and 0.8s stun.",
		"ultimate_desc": "Teleports for 250% magic damage and 1.5s stun.",
		"passive_desc": "Increases attack damage by 15%.",
		"passive_id": "swiftness",
	},
	"warlock": {
		"description": "A dark sorcerer who siphons the life force of his enemies to sustain himself and his allies.",
		"ability_desc": "Deals 150% magic damage and heals self for 20% of it.",
		"ultimate_desc": "Rifts the ground for 400% magic damage and 1.0s stun.",
		"passive_desc": "Heals for 3 HP after each auto-attack.",
		"passive_id": "vampirism",
	},
	"monk": {
		"description": "A martial arts master who uses a flurry of strikes to incapacitate foes through precise pressure points.",
		"ability_desc": "Precise strike for 120% damage and 0.8s stun.",
		"ultimate_desc": "Rapid flurry for 250% damage and 1.5s stun.",
		"passive_desc": "Every 3rd attack stuns the target for 0.5s.",
		"passive_id": "technique",
	},
	"artillery": {
		"description": "A fragile but explosive backline siege unit that deals massive damage to anything in its sights.",
		"ability_desc": "Explosive shell dealing 120% damage and 0.8s stun.",
		"ultimate_desc": "Fires a massive artillery shell for 500% damage.",
		"passive_desc": "Attacks and abilities deal 50% splash damage to enemies within 0.5 units of the target.",
		"passive_id": "demolition",
	},
	"cleric": {
		"description": "A dedicated holy healer who provides constant HP regeneration and massive burst heals for her team.",
		"ability_desc": "Heals an ally or self for 30% max HP.",
		"ultimate_desc": "Heals for 55% max HP and stuns target for 1.5s.",
		"passive_desc": "Regenerates 2% of max HP every second.",
		"passive_id": "devotion",
	},
	"siren": {
		"description": "A captivating support who lures enemies into stuns and drains their mana with every haunting song.",
		"ability_desc": "Binds target stunning for 0.5s stun.",
		"ultimate_desc": "Shrieks for 300% magic damage and stunning for 1.0s.",
		"passive_desc": "Drains 5 mana from the target on each auto-attack.",
		"passive_id": "siphon",
	},
	"valkyrie": {
		"description": "A formidable bruiser who grows more dangerous as she fights, slamming foes with her shield.",
		"ability_desc": "Bashes target for 150% damage and 0.5s stun.",
		"ultimate_desc": "War cry dealing 300% damage and 1.5s stun.",
		"passive_desc": "Deals 20% bonus damage while above 80% HP.",
		"passive_id": "bravery",
	},
}


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
	_register_systemic_role_effects(registry)
	_register_champion_effects(registry)


static func _augment_hero(hero: Dictionary) -> Dictionary:
	var result := hero.duplicate(true)
	var role := String(result.get("role", ""))
	var hero_id := String(result.get("id", ""))
	result.merge(_defaults_for(role, hero_id), true)
	if HERO_DETAILS.has(hero_id):
		result.merge(HERO_DETAILS[hero_id], true)
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
				"passive_id": "bastion",
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
				"passive_id": "duelist",
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
				"passive_id": "executioner",
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
				"passive_id": "marksman",
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
				"passive_id": "mana_font",
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
				"passive_id": "enlightenment",
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


static func _register_systemic_role_effects(registry: Object) -> void:
	for hero in HEROES:
		var hero_id := String(hero.get("id", ""))
		var role := String(hero.get("role", ""))
		if role == "tank":
			registry.call("register_passive", "post_take_damage", hero_id, _effect_post_damage_mana_gain(0.10))
		elif role == "mage":
			registry.call("register_passive", "on_tick", hero_id, _effect_mana_regen(1.0))


static func _register_champion_effects(registry: Object) -> void:
	registry.call("register_ability", "swordsman", _effect_multi([
		_effect_damage(1.5, "physical", "Concussive Blow"),
		_effect_stun(1.0, "Concussive Blow"),
	], "Concussive Blow"))
	registry.call("register_ultimate", "swordsman", _effect_multi([
		_effect_damage(3.0, "physical", "Blade Dance"),
		_effect_stun(2.0, "Blade Dance"),
	], "Blade Dance"))
	registry.call("register_passive", "on_attack", "duelist", _effect_constant_multiplier(1.2))

	registry.call("register_ability", "archer", _effect_projectile(1.5, "physical", "Volley", 8.0, 0.03))
	registry.call("register_ultimate", "archer", _effect_projectile(4.0, "physical", "Rain of Arrows", 10.0, 0.06, 0.0, 2.0, 0.5))
	registry.call("register_passive", "on_attack", "eagle_eye", _effect_constant_multiplier(1.25))
	registry.call("register_passive", "post_attack", "eagle_eye", _effect_post_attack_splash(2.0, 0.5, "physical", "Rain of Arrows", 3.0))

	registry.call("register_ability", "guardian", _effect_shield(0.0, 0.2, "Guardian Shield"))
	registry.call("register_ultimate", "guardian", _effect_multi([
		_effect_damage(2.0, "physical", "Aegis Crash"),
		_effect_stun(3.0, "Aegis Crash"),
	], "Aegis Crash"))
	registry.call("register_passive", "on_defense", "bastion", _effect_constant_multiplier(0.9))

	registry.call("register_ability", "assassin", _effect_multi([
		_effect_damage(1.2, "physical", "Shadow Dash"),
		_effect_stun(0.5, "Shadow Dash"),
	], "Shadow Dash"))
	registry.call("register_ultimate", "assassin", _effect_damage(3.5, "physical", "Assassinate"))
	registry.call("register_passive", "on_attack", "executioner", _effect_target_hp_threshold_multiplier(0.30, 2.0))

	registry.call("register_ability", "mage", _effect_projectile(2.0, "magic", "Arcane Bolt", 7.0, 0.03))
	registry.call("register_ultimate", "mage", _effect_projectile(6.0, "magic", "Meteor Bombardment", 4.0, 0.07, 0.0, 2.5, 0.5))
	registry.call("register_passive", "on_tick", "mana_font", _effect_mana_regen(3.0))

	registry.call("register_ability", "sniper", _effect_projectile(1.8, "physical", "Headshot", 12.0, CombatData.DEFAULT_PROJECTILE_RADIUS))
	registry.call("register_ultimate", "sniper", _effect_projectile(3.5, "physical", "ULTIMATE", 15.0, CombatData.DEFAULT_PROJECTILE_RADIUS))
	registry.call("register_passive", "on_attack", "marksman", _effect_distance_threshold_multiplier(3.0, 1.25))

	registry.call("register_ability", "berserker", _effect_multi([
		_effect_self_damage(0.10, "Blood Price"),
		_effect_self_shield(0.15, "Blood Price"),
	], "Blood Price"))
	registry.call("register_ultimate", "berserker", _effect_damage(3.0, "true", "Berserker Rage"))
	registry.call("register_passive", "post_attack", "bloodlust", _effect_damage_based_heal(0.15))

	registry.call("register_ability", "paladin", _effect_heal(0.0, 0.15, "Lay on Hands"))
	registry.call("register_ultimate", "paladin", _effect_multi([
		_effect_heal(0.0, 0.30, "Divine Judgment"),
		_effect_damage(2.0, "physical", "Divine Judgment"),
	], "Divine Judgment"))
	registry.call("register_passive", "on_tick", "rejuvenation", _effect_heal(5.0, 0.0, "Rejuvenation"))

	registry.call("register_ability", "rogue", _effect_multi([
		_effect_damage(0.8, "magic", "Poison Vial"),
		_effect_stun(2.0, "Poison Vial"),
	], "Poison Vial"))
	registry.call("register_ultimate", "rogue", _effect_damage(3.5, "physical", "Eviscerate"))
	registry.call("register_passive", "on_defense", "agility", _effect_dodge(0.25, 0.0, 1.0))

	registry.call("register_ability", "oracle", _effect_heal(0.0, 0.2, "Purify"))
	registry.call("register_ultimate", "oracle", _effect_shield(0.0, 0.4, "Divine Shield"))
	registry.call("register_passive", "post_attack", "enlightenment", _effect_mana_restore_on_hit(5.0))

	registry.call("register_ability", "colossus", _effect_multi([
		_effect_self_aoe_taunt(1.0, 2.0, "Seismic Slam"),
		_effect_self_aoe_damage(1.0, 1.0, "physical", "Seismic Slam"),
	], "Seismic Slam"))
	registry.call("register_ultimate", "colossus", _effect_multi([
		_effect_damage(2.5, "physical", "Earthshaker"),
		_effect_stun(3.5, "Earthshaker"),
	], "Earthshaker"))
	registry.call("register_passive", "on_defense", "tenacity", _effect_constant_multiplier(0.9))

	registry.call("register_ability", "wraith", _effect_multi([
		_effect_damage(1.2, "magic", "Spectral Touch"),
		_effect_stun(0.8, "Spectral Touch"),
	], "Spectral Touch"))
	registry.call("register_ultimate", "wraith", _effect_multi([
		_effect_damage(2.5, "magic", "Phantom Strike"),
		_effect_stun(1.5, "Phantom Strike"),
	], "Phantom Strike"))
	registry.call("register_passive", "on_attack", "swiftness", _effect_constant_multiplier(1.15))

	registry.call("register_ability", "warlock", _effect_projectile(1.5, "magic", "Soul Siphon", 5.0, CombatData.DEFAULT_PROJECTILE_RADIUS))
	registry.call("register_ultimate", "warlock", _effect_projectile(4.0, "magic", "Chaos Rift", 5.0, CombatData.DEFAULT_PROJECTILE_RADIUS, 1.0))
	registry.call("register_passive", "post_attack", "vampirism", _effect_heal(3.0, 0.0, "Vampirism"))

	registry.call("register_ability", "monk", _effect_multi([
		_effect_damage(1.2, "physical", "Pressure Point"),
		_effect_stun(0.8, "Pressure Point"),
	], "Pressure Point"))
	registry.call("register_ultimate", "monk", _effect_multi([
		_effect_damage(2.5, "physical", "Hundred-Hand Slap"),
		_effect_stun(1.5, "Hundred-Hand Slap"),
	], "Hundred-Hand Slap"))
	registry.call("register_passive", "post_attack", "technique", _effect_every_n_attacks_stun(3, 0.5, "Pressure Point"))

	registry.call("register_ability", "artillery", _effect_projectile(1.2, "physical", "Shell Shock", 3.0, 0.05, 0.8))
	registry.call("register_ultimate", "artillery", _effect_projectile(5.0, "physical", "Big Bertha", 3.0, 0.08))
	registry.call("register_passive", "post_attack", "demolition", _effect_post_attack_splash(0.5, 0.5, "physical", "Explosion"))

	registry.call("register_ability", "cleric", _effect_heal(0.0, 0.3, "Holy Mending"))
	registry.call("register_ultimate", "cleric", _effect_multi([
		_effect_heal(0.0, 0.55, "Divine Aura"),
		_effect_stun(1.5, "Divine Aura"),
	], "Divine Aura"))
	registry.call("register_passive", "on_tick", "devotion", _effect_heal(0.0, 0.02, "Devotion"))

	registry.call("register_ability", "siren", _effect_stun(0.5, "Enthralling Song"))
	registry.call("register_ultimate", "siren", _effect_multi([
		_effect_damage(3.0, "magic", "Banshee Wail"),
		_effect_stun(1.0, "Banshee Wail"),
	], "Banshee Wail"))
	registry.call("register_passive", "post_attack", "siphon", _effect_drain_target_mana_on_hit(5.0))

	registry.call("register_ability", "valkyrie", _effect_multi([
		_effect_damage(1.5, "physical", "Shield Slam"),
		_effect_stun(0.5, "Shield Slam"),
	], "Shield Slam"))
	registry.call("register_ultimate", "valkyrie", _effect_multi([
		_effect_damage(3.0, "physical", "Valhalla Call"),
		_effect_stun(1.5, "Valhalla Call"),
	], "Valhalla Call"))
	registry.call("register_passive", "on_attack", "bravery", _effect_self_hp_threshold_multiplier(0.8, 1.2))


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


static func _effect_damage_based_heal(damage_ratio: float) -> Object:
	return CombatEffectScript.new().setup_damage_based_heal(damage_ratio)


static func _effect_drain_target_mana_on_hit(flat_amount: float) -> Object:
	return CombatEffectScript.new().setup_drain_target_mana_on_hit(flat_amount)


static func _effect_self_hp_threshold_multiplier(threshold: float, multiplier: float) -> Object:
	return CombatEffectScript.new().setup_self_hp_threshold_multiplier(threshold, multiplier)


static func _effect_self_damage(damage_ratio: float, effect_reason: String = "") -> Object:
	return CombatEffectScript.new().setup_self_damage(damage_ratio, effect_reason)


static func _effect_self_shield(shield_ratio: float, effect_reason: String = "") -> Object:
	return CombatEffectScript.new().setup_self_shield(shield_ratio, effect_reason)


static func _effect_self_aoe_damage(radius: float, multiplier: float = 1.0, damage_type: String = "physical", effect_reason: String = "") -> Object:
	return CombatEffectScript.new().setup_self_aoe_damage(radius, multiplier, damage_type, effect_reason)


static func _effect_self_aoe_taunt(radius: float, duration: float, effect_reason: String = "") -> Object:
	return CombatEffectScript.new().setup_self_aoe_taunt(radius, duration, effect_reason)


static func _effect_post_attack_splash(radius: float, ratio: float, damage_type: String = "physical", effect_reason: String = "", threshold_multiplier: float = 0.0) -> Object:
	return CombatEffectScript.new().setup_post_attack_splash(radius, ratio, damage_type, effect_reason, threshold_multiplier)


static func _effect_every_n_attacks_stun(every_n: int, duration: float, effect_reason: String = "") -> Object:
	return CombatEffectScript.new().setup_every_n_attacks_stun(every_n, duration, effect_reason)


static func _effect_dodge(chance: float, on_dodge_multiplier: float = 0.0, on_hit_multiplier: float = 1.0) -> Object:
	return CombatEffectScript.new().setup_dodge(chance, on_dodge_multiplier, on_hit_multiplier)


static func _best_by_power(heroes: Array[Dictionary]) -> Dictionary:
	var best := heroes[0]
	var best_score := power_score(best)
	for hero in heroes:
		var score := power_score(hero)
		if score > best_score:
			best = hero
			best_score = score
	return best
