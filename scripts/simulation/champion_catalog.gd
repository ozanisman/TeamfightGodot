class_name ChampionCatalog
extends RefCounted

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const EffectSpecScript := preload("res://scripts/simulation/effect_spec.gd")
const ChampionStatsScript := preload("res://scripts/simulation/champion_stats.gd")
const ChampionSpecScript := preload("res://scripts/simulation/champion_spec.gd")
const RoleConfigSpecScript := preload("res://scripts/simulation/role_config_spec.gd")

static var _role_config_cache: Dictionary = {}
static var _catalog_cache: Dictionary = {}
static var _passive_cache: Dictionary = {}
static var _champion_ids_cache: Array[StringName] = []
static var _role_kits: Dictionary = {}
static var _role_kits_loaded: bool = false

static func _build_effect(data: Dictionary) -> EffectSpecScript:
	var params: Dictionary = data["params"].duplicate()
	var requires_target_in_range: bool = bool(data.get("requires_target_in_range", true))
	
	for key in params:
		var value = params[key]
		if key == "effects" and value is Array:
			var built_effects: Array = []
			for effect_data in value:
				built_effects.append(_build_effect(effect_data))
			params[key] = built_effects
		elif key == "splash" and value is Dictionary:
			params[key] = _build_effect(value)
	
	return EffectSpecScript.new(data["kind"], params, requires_target_in_range)

static func _build_stats(data: Dictionary) -> ChampionStatsScript:
	var stats := ChampionStatsScript.new()
	
	stats.unit_id = data.get("unit_id", &"")
	stats.name = data.get("name", &"")
	stats.role = data.get("role", &"")
	stats.max_hp = data.get("max_hp", 0.0)
	stats.attack_damage = data.get("attack_damage", 0.0)
	stats.attack_range = data.get("attack_range", 0.0)
	stats.attack_speed = data.get("attack_speed", 0.0)
	stats.move_speed = data.get("move_speed", 0.0)
	stats.armor = data.get("armor", 0.0)
	stats.magic_resist = data.get("magic_resist", 0.0)
	stats.tenacity = data.get("tenacity", 0.0)
	stats.life_steal = data.get("life_steal", 0.0)
	stats.max_mana = data.get("max_mana", 50.0)
	stats.mana_per_attack = data.get("mana_per_attack", 10.0)
	stats.ability_cd = data.get("ability_cd", 5.0)
	stats.ultimate_cd = data.get("ultimate_cd", 20.0)
	
	var projectile_speed: float = data.get("projectile_speed", 0.0)
	if projectile_speed == 0.0:
		projectile_speed = SimConstantsScript.DEFAULT_PROJECTILE_SPEED
	stats.projectile_speed = projectile_speed
	
	var projectile_radius: float = data.get("projectile_radius", 0.0)
	if projectile_radius == 0.0:
		projectile_radius = SimConstantsScript.DEFAULT_PROJECTILE_RADIUS
	stats.projectile_radius = projectile_radius
	
	stats.passive_id = data.get("passive_id", &"")
	
	var respawn_time: float = data.get("respawn_time", 0.0)
	if respawn_time == 0.0:
		respawn_time = SimConstantsScript.RESPAWN_TIME
	stats.respawn_time = respawn_time
	
	return stats

static func _build_champion(data: Dictionary) -> ChampionSpecScript:
	var stats := _build_stats(data["stats"])
	var ability := _build_effect(data["ability"])
	var ultimate := _build_effect(data["ultimate"])
	var passive_ids: Array[StringName] = []
	for id in data["passive_ids"]:
		passive_ids.append(id)
	
	return ChampionSpecScript.new(
		stats,
		data["description"],
		data["ability_desc"],
		data["ultimate_desc"],
		data["passive_desc"],
		ability,
		ultimate,
		passive_ids
	)

static func _build_role_config(data: Dictionary) -> RoleConfigSpecScript:
	var stat_mods: Dictionary = data["stat_mods"]
	var passive_on_tick = null
	if data["passive_on_tick"] != null:
		passive_on_tick = _build_effect(data["passive_on_tick"])
	
	var passive_post_take_damage = null
	if data["passive_post_take_damage"] != null:
		passive_post_take_damage = _build_effect(data["passive_post_take_damage"])
	
	return RoleConfigSpecScript.new(stat_mods, passive_on_tick, passive_post_take_damage)

static func _effect(kind: StringName, params: Dictionary = {}):
	return EffectSpecScript.new(kind, params)

const CHAMPION_DATA := {
	&"swordsman": {
		"stats": {
			"unit_id": &"swordsman",
			"name": &"Swordsman",
			"role": &"fighter",
			"max_hp": 220.0,
			"attack_damage": 32.0,
			"attack_range": 0.3,
			"attack_speed": 1.8,
			"move_speed": 0.9,
			"armor": 0.42,
			"magic_resist": 0.32,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 60.0,
			"mana_per_attack": 10.0,
			"ability_cd": 3.5,
			"ultimate_cd": 15.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"duelist",
			"respawn_time": 0.0,
		},
		"description": "A skilled duelist who gains attack damage with each strike and can stun groups of enemies.",
		"ability_desc": "Cleaves for 270% damage and 2.0s stun.",
		"ultimate_desc": "Whirlwind for 560% damage and 3.2s stun.",
		"passive_desc": "Gains 10% attack damage per auto-attack, stacking up to 50%.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"damage", "params": {"damage_multiplier": 2.7, "reason": "Cleave", "trigger_on_hit": false}},
					{"kind": &"stun", "params": {"duration": 2.0, "reason": "Cleave"}},
				],
				"reason": "Cleave",
			},
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"damage", "params": {"damage_multiplier": 5.6, "reason": "Whirlwind", "trigger_on_hit": false}},
					{"kind": &"stun", "params": {"duration": 3.2, "reason": "Whirlwind"}},
				],
				"reason": "Whirlwind",
			},
		},
		"passive_ids": [&"duelist"],
	},
	&"archer": {
		"stats": {
			"unit_id": &"archer",
			"name": &"Archer",
			"role": &"marksman",
			"max_hp": 75.0,
			"attack_damage": 14.0,
			"attack_range": 3.5,
			"attack_speed": 1.35,
			"move_speed": 0.6,
			"armor": 0.08,
			"magic_resist": 0.10,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 60.0,
			"mana_per_attack": 10.0,
			"ability_cd": 5.5,
			"ultimate_cd": 20.0,
			"projectile_speed": 8.0,
			"projectile_radius": 0.0,
			"passive_id": &"eagle_eye",
			"respawn_time": 0.0,
		},
		"description": "A swift ranged attacker who fires volleys of arrows and excels at picking off distant targets.",
		"ability_desc": "Fires a volley for 140% damage.",
		"ultimate_desc": "Rain of Arrows for 380% damage with splash.",
		"passive_desc": "Increases attack damage by 25%. Rain of Arrows deals 50% splash damage in a 2.0 unit radius.",
		"ability": {"kind": &"projectile", "params": {"damage_multiplier": 1.4, "reason": "Volley"}},
		"ultimate": {"kind": &"projectile", "params": {"damage_multiplier": 3.8, "reason": "Rain of Arrows", "radius_override": 0.06}},
		"passive_ids": [&"eagle_eye"],
	},
	&"guardian": {
		"stats": {
			"unit_id": &"guardian",
			"name": &"Guardian",
			"role": &"tank",
			"max_hp": 280.0,
			"attack_damage": 15.0,
			"attack_range": 0.3,
			"attack_speed": 0.9,
			"move_speed": 0.58,
			"armor": 0.44,
			"magic_resist": 0.34,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 8.3,
			"ultimate_cd": 33.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"bastion",
			"respawn_time": 0.0,
		},
		"description": "A hulking protector who uses heavy shields to soak damage and a massive slam to stun entire groups.",
		"ability_desc": "Grants a shield worth 28% max HP to self or an ally.",
		"ultimate_desc": "Slams the ground for 290% damage and a 3.8s stun.",
		"passive_desc": "Reduces incoming damage by 19%.",
		"ability": {"kind": &"shield", "params": {"max_hp_ratio": 0.28, "reason": "Guardian Shield"}},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"damage", "params": {"damage_multiplier": 2.9, "reason": "Aegis Crash", "trigger_on_hit": false}},
					{"kind": &"stun", "params": {"duration": 3.8, "reason": "Aegis Crash"}},
				],
				"reason": "Aegis Crash",
			},
		},
		"passive_ids": [&"bastion"],
	},
	&"ninja": {
		"stats": {
			"unit_id": &"ninja",
			"name": &"Ninja",
			"role": &"assassin",
			"max_hp": 220.0,
			"attack_damage": 25.0,
			"attack_range": 0.3,
			"attack_speed": 1.7,
			"move_speed": 0.88,
			"armor": 0.15,
			"magic_resist": 0.15,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 90.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.0,
			"ultimate_cd": 12.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"executioner",
			"respawn_time": 0.0,
		},
		"description": "A high-mobility predator designed to dive the backline and execute wounded targets with lethal precision.",
		"ability_desc": "Dashes toward the target enemy. If reached, deal 150% damage and stun for 0.5s.",
		"ultimate_desc": "Executes a target with 900% physical damage.",
		"passive_desc": "Deals double damage to targets below 30% HP.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"self_dash", "params": {"distance": 2.0}},
					{"kind": &"damage", "params": {"damage_multiplier": 1.5, "reason": "Shadow Strike", "requires_result_from": "self_dash", "requires_field": "reached_target", "requires_value": true}},
					{"kind": &"stun", "params": {"duration": 0.5, "reason": "Shadow Strike", "requires_result_from": "self_dash", "requires_field": "reached_target", "requires_value": true}},
				],
				"reason": "Shadow Strike",
			},
			"requires_target_in_range": false,
		},
		"ultimate": {"kind": &"projectile", "params": {"damage_multiplier": 9.0, "reason": "Assassinate"}},
		"passive_ids": [&"executioner"],
	},
	&"sniper": {
		"stats": {
			"unit_id": &"sniper",
			"name": &"Sniper",
			"role": &"marksman",
			"max_hp": 60.0,
			"attack_damage": 16.0,
			"attack_range": 4.0,
			"attack_speed": 0.35,
			"move_speed": 0.4,
			"armor": 0.05,
			"magic_resist": 0.05,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 60.0,
			"mana_per_attack": 10.0,
			"ability_cd": 14.0,
			"ultimate_cd": 55.0,
			"projectile_speed": 12.0,
			"projectile_radius": 0.0,
			"passive_id": &"marksman",
			"respawn_time": 0.0,
		},
		"description": "An elite marksman with extreme range, specializing in devastating single-target ultimate shots.",
		"ability_desc": "High-precision strike for 150% damage.",
		"ultimate_desc": "Lethal long-range shot for 280% damage.",
		"passive_desc": "Deals 25% bonus damage to targets further than 3 units away.",
		"ability": {"kind": &"projectile", "params": {"damage_multiplier": 1.5, "reason": "Headshot"}},
		"ultimate": {"kind": &"projectile", "params": {"damage_multiplier": 2.8, "speed_override": 15.0, "reason": "ULTIMATE"}},
		"passive_ids": [&"marksman"],
	},
	&"berserker": {
		"stats": {
			"unit_id": &"berserker",
			"name": &"Berserker",
			"role": &"fighter",
			"max_hp": 260.0,
			"attack_damage": 22.0,
			"attack_range": 0.3,
			"attack_speed": 2.0,
			"move_speed": 0.75,
			"armor": 0.24,
			"magic_resist": 0.14,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 60.0,
			"mana_per_attack": 10.0,
			"ability_cd": 3.5,
			"ultimate_cd": 13.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"bloodlust",
			"respawn_time": 0.0,
		},
		"description": "A savage warrior who thrives in the heat of battle, healing himself through sheer aggression.",
		"ability_desc": "Damages self for 5% max HP and gains 240% of the value as a shield.",
		"ultimate_desc": "Unleashes a devastating strike for 420% true damage.",
		"passive_desc": "Heals for 24% of all damage dealt by auto-attacks.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"self_damage", "params": {"damage_ratio": 0.05, "reason": "Blood Price"}},
					{"kind": &"self_shield", "params": {"shield_ratio": 0.24, "reason": "Blood Price"}},
				],
				"reason": "Blood Price",
			},
		},
		"ultimate": {"kind": &"damage", "params": {"damage_multiplier": 4.2, "damage_type": "true", "trigger_on_hit": false, "reason": "Berserker Rage"}},
		"passive_ids": [&"bloodlust"],
	},
	&"paladin": {
		"stats": {
			"unit_id": &"paladin",
			"name": &"Paladin",
			"role": &"tank",
			"max_hp": 300.0,
			"attack_damage": 24.0,
			"attack_range": 0.3,
			"attack_speed": 1.05,
			"move_speed": 0.68,
			"armor": 0.40,
			"magic_resist": 0.40,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 80.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.5,
			"ultimate_cd": 22.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"rejuvenation",
			"respawn_time": 0.0,
		},
		"description": "A holy knight who balances immense durability with powerful self-healing and divine judgment.",
		"ability_desc": "Heals self for 26% max HP.",
		"ultimate_desc": "Heals self for 46% max HP and deals 360% damage.",
		"passive_desc": "Regenerates 11 HP every second.",
		"ability": {"kind": &"heal", "params": {"max_hp_ratio": 0.26, "reason": "Lay on Hands"}},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"heal", "params": {"max_hp_ratio": 0.46, "reason": "Divine Judgment"}},
					{"kind": &"damage", "params": {"damage_multiplier": 3.6, "reason": "Divine Judgment", "trigger_on_hit": false}},
				],
				"reason": "Divine Judgment",
			},
		},
		"passive_ids": [&"rejuvenation"],
	},
	&"rogue": {
		"stats": {
			"unit_id": &"rogue",
			"name": &"Rogue",
			"role": &"assassin",
			"max_hp": 190.0,
			"attack_damage": 44.0,
			"attack_range": 0.3,
			"attack_speed": 2.9,
			"move_speed": 0.98,
			"armor": 0.28,
			"magic_resist": 0.28,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 40.0,
			"mana_per_attack": 10.0,
			"ability_cd": 1.2,
			"ultimate_cd": 7.5,
			"projectile_speed": 5.0,
			"projectile_radius": 0.03,
			"passive_id": &"agility",
			"respawn_time": 10.0,
		},
		"description": "A nimble backline specialist who strikes with precision and vanishes before being targeted.",
		"ability_desc": "Deals 220% magic damage and stuns for 4.0s.",
		"ultimate_desc": "Lethal execution dealing 780% physical damage.",
		"passive_desc": "25% chance to dodge attacks and deal no damage when dodging.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"damage", "params": {"damage_multiplier": 2.2, "reason": "Backstab", "trigger_on_hit": false}},
					{"kind": &"stun", "params": {"duration": 4.0, "reason": "Backstab"}},
				],
				"reason": "Backstab",
			},
		},
		"ultimate": {"kind": &"damage", "params": {"damage_multiplier": 7.8, "reason": "Eviscerate"}},
		"passive_ids": [&"agility"],
	},
	&"oracle": {
		"stats": {
			"unit_id": &"oracle",
			"name": &"Oracle",
			"role": &"support",
			"max_hp": 85.0,
			"attack_damage": 14.0,
			"attack_range": 3.5,
			"attack_speed": 0.75,
			"move_speed": 0.45,
			"armor": 0.05,
			"magic_resist": 0.15,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 9.0,
			"ultimate_cd": 32.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"enlightenment",
			"respawn_time": 0.0,
		},
		"description": "A divine seer who manipulates fate through shields and mana restoration.",
		"ability_desc": "Grants a shield worth 18% max HP to self or an ally.",
		"ultimate_desc": "Restores 35% max HP and grants a shield worth 40% max HP.",
		"passive_desc": "Restores 4 mana after each auto-attack.",
		"ability": {"kind": &"shield", "params": {"max_hp_ratio": 0.18, "reason": "Purify"}},
		"ultimate": {"kind": &"shield", "params": {"max_hp_ratio": 0.35, "reason": "Divine Shield"}},
		"passive_ids": [&"enlightenment"],
	},
	&"colossus": {
		"stats": {
			"unit_id": &"colossus",
			"name": &"Colossus",
			"role": &"tank",
			"max_hp": 520.0,
			"attack_damage": 17.0,
			"attack_range": 0.3,
			"attack_speed": 0.82,
			"move_speed": 0.66,
			"armor": 0.52,
			"magic_resist": 0.46,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 1.5,
			"ultimate_cd": 30.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.05,
			"passive_id": &"durability",
			"respawn_time": 0.0,
		},
		"description": "A mountain of a tank with unmatched physical resistance and the ability to stun the entire battlefield.",
		"ability_desc": "Taunts all targets in a 2 unit radius for 2.5s and deals 165% damage in a 1 unit radius.",
		"ultimate_desc": "Colossal impact for 460% damage and a 5.2s stun.",
		"passive_desc": "Reduces all incoming damage by 10%.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"self_aoe_taunt", "params": {"radius": 2.0, "duration": 2.5, "reason": "Seismic Slam"}},
					{"kind": &"self_aoe_damage", "params": {"radius": 1.0, "damage_multiplier": 1.65, "damage_type": "physical", "reason": "Seismic Slam"}},
				],
				"reason": "Seismic Slam",
			},
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"damage", "params": {"damage_multiplier": 4.6, "reason": "Earthshaker", "trigger_on_hit": false}},
					{"kind": &"stun", "params": {"duration": 5.2, "reason": "Earthshaker"}},
				],
				"reason": "Earthshaker",
			},
		},
		"passive_ids": [&"durability"],
	},
	&"wraith": {
		"stats": {
			"unit_id": &"wraith",
			"name": &"Wraith",
			"role": &"assassin",
			"max_hp": 270.0,
			"attack_damage": 48.0,
			"attack_range": 0.3,
			"attack_speed": 2.2,
			"move_speed": 0.94,
			"armor": 0.24,
			"magic_resist": 0.38,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 40.0,
			"mana_per_attack": 10.0,
			"ability_cd": 2.6,
			"ultimate_cd": 10.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"swiftness",
			"respawn_time": 0.0,
		},
		"description": "A terrifying spectral assassin that stuns its prey and moves faster than the eye can see.",
		"ability_desc": "Magic strike for 220% damage and 1.5s stun.",
		"ultimate_desc": "Teleports for 480% magic damage and 2.8s stun.",
		"passive_desc": "Increases attack damage by 38%.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"damage", "params": {"damage_multiplier": 2.2, "damage_type": "magic", "reason": "Spectral Touch", "trigger_on_hit": false}},
					{"kind": &"stun", "params": {"duration": 1.5, "reason": "Spectral Touch"}},
				],
				"reason": "Spectral Touch",
			},
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"damage", "params": {"damage_multiplier": 4.8, "damage_type": "magic", "reason": "Phantom Strike", "trigger_on_hit": false}},
					{"kind": &"stun", "params": {"duration": 2.8, "reason": "Phantom Strike"}},
				],
				"reason": "Phantom Strike",
			},
		},
		"passive_ids": [&"swiftness"],
	},
	&"warlock": {
		"stats": {
			"unit_id": &"warlock",
			"name": &"Warlock",
			"role": &"mage",
			"max_hp": 85.0,
			"attack_damage": 14.0,
			"attack_range": 3.5,
			"attack_speed": 0.65,
			"move_speed": 0.4,
			"armor": 0.05,
			"magic_resist": 0.14,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 80.0,
			"mana_per_attack": 10.0,
			"ability_cd": 8.5,
			"ultimate_cd": 32.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"vampirism",
			"respawn_time": 0.0,
		},
		"description": "A dark sorcerer who siphons the life force of his enemies to sustain himself and his allies.",
		"ability_desc": "Deals 100% magic damage and heals self for 8% of it.",
		"ultimate_desc": "Rifts the ground for 260% magic damage and 0.5s stun.",
		"passive_desc": "Heals for 3 HP after each auto-attack.",
		"ability": {"kind": &"projectile", "params": {"damage_multiplier": 1.0, "damage_type": "magic", "reason": "Soul Siphon"}},
		"ultimate": {"kind": &"projectile", "params": {"damage_multiplier": 2.6, "damage_type": "magic", "stun_duration": 0.5, "reason": "Chaos Rift"}},
		"passive_ids": [&"vampirism"],
	},
	&"mage": {
		"stats": {
			"unit_id": &"mage",
			"name": &"Mage",
			"role": &"mage",
			"max_hp": 75.0,
			"attack_damage": 15.0,
			"attack_range": 3.5,
			"attack_speed": 0.92,
			"move_speed": 0.5,
			"armor": 0.05,
			"magic_resist": 0.24,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 80.0,
			"mana_per_attack": 10.0,
			"ability_cd": 8.2,
			"ultimate_cd": 26.0,
			"projectile_speed": 6.0,
			"projectile_radius": 0.0,
			"passive_id": &"mana_font",
			"respawn_time": 0.0,
		},
		"description": "A powerful spellcaster with a deep mana pool, capable of unleashing frequent bursts of magic damage.",
		"ability_desc": "Fires a magical projectile dealing 170% magic damage.",
		"ultimate_desc": "Calls down a meteor for 500% magic damage and deals splash damage on impact.",
		"passive_desc": "Restores 2 mana every second. Meteor impact deals 50% splash damage in a 2.5 unit radius.",
		"ability": {"kind": &"projectile", "params": {"damage_multiplier": 1.7, "damage_type": "magic", "speed_override": 7.0, "reason": "Arcane Bolt"}},
		"ultimate": {"kind": &"projectile", "params": {"damage_multiplier": 5.0, "damage_type": "magic", "speed_override": 4.0, "radius_override": 0.07, "reason": "Meteor Bombardment"}},
		"passive_ids": [&"mana_font"],
	},
	&"monk": {
		"stats": {
			"unit_id": &"monk",
			"name": &"Monk",
			"role": &"fighter",
			"max_hp": 250.0,
			"attack_damage": 32.0,
			"attack_range": 0.3,
			"attack_speed": 1.2,
			"move_speed": 0.76,
			"armor": 0.24,
			"magic_resist": 0.24,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 60.0,
			"mana_per_attack": 10.0,
			"ability_cd": 2.2,
			"ultimate_cd": 9.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"technique",
			"respawn_time": 0.0,
		},
		"description": "A martial arts master who uses a flurry of strikes to incapacitate foes through precise pressure points.",
		"ability_desc": "Precise strike for 210% damage and 1.5s stun.",
		"ultimate_desc": "Rapid flurry for 410% damage and 2.8s stun.",
		"passive_desc": "Every 3rd attack stuns the target for 1.1s.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"damage", "params": {"damage_multiplier": 2.1, "reason": "Pressure Point", "trigger_on_hit": false}},
					{"kind": &"stun", "params": {"duration": 1.5, "reason": "Pressure Point"}},
				],
				"reason": "Pressure Point",
			},
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"damage", "params": {"damage_multiplier": 4.1, "reason": "Hundred-Hand Slap", "trigger_on_hit": false}},
					{"kind": &"stun", "params": {"duration": 2.8, "reason": "Hundred-Hand Slap"}},
				],
				"reason": "Hundred-Hand Slap",
			},
		},
		"passive_ids": [&"technique"],
	},
	&"artillery": {
		"stats": {
			"unit_id": &"artillery",
			"name": &"Artillery",
			"role": &"marksman",
			"max_hp": 55.0,
			"attack_damage": 16.0,
			"attack_range": 5.0,
			"attack_speed": 0.33,
			"move_speed": 0.1,
			"armor": 0.05,
			"magic_resist": 0.05,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 50.0,
			"mana_per_attack": 10.0,
			"ability_cd": 17.0,
			"ultimate_cd": 65.0,
			"projectile_speed": 3.0,
			"projectile_radius": 0.05,
			"passive_id": &"demolition",
			"respawn_time": 0.0,
		},
		"description": "A fragile but explosive backline siege unit that deals massive damage to anything in its sights.",
		"ability_desc": "Explosive shell dealing 95% damage and 0.6s stun.",
		"ultimate_desc": "Fires a massive artillery shell for 330% damage.",
		"passive_desc": "Attacks and abilities deal 50% splash damage to enemies within 0.5 units of the target.",
		"ability": {"kind": &"projectile", "params": {"damage_multiplier": 0.95, "stun_duration": 0.6, "reason": "Shell Shock"}},
		"ultimate": {"kind": &"projectile", "params": {"damage_multiplier": 3.3, "radius_override": 0.08, "reason": "Big Bertha"}},
		"passive_ids": [&"demolition"],
	},
	&"cleric": {
		"stats": {
			"unit_id": &"cleric",
			"name": &"Cleric",
			"role": &"support",
			"max_hp": 115.0,
			"attack_damage": 13.0,
			"attack_range": 3.5,
			"attack_speed": 1.0,
			"move_speed": 0.48,
			"armor": 0.06,
			"magic_resist": 0.16,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 12.0,
			"ultimate_cd": 42.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"devotion",
			"respawn_time": 0.0,
		},
		"description": "A dedicated holy healer who provides constant HP regeneration and massive burst heals for her team.",
		"ability_desc": "Heals an ally or self for 20% max HP.",
		"ultimate_desc": "Heals for 38% max HP and stuns target for 0.9s.",
		"passive_desc": "Regenerates 1.0% of max HP every second.",
		"ability": {"kind": &"heal", "params": {"max_hp_ratio": 0.20, "reason": "Holy Mending"}},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"heal", "params": {"max_hp_ratio": 0.38, "reason": "Divine Aura"}},
					{"kind": &"stun", "params": {"duration": 0.9, "reason": "Divine Aura"}},
				],
				"reason": "Divine Aura",
			},
		},
		"passive_ids": [&"devotion"],
	},
	&"siren": {
		"stats": {
			"unit_id": &"siren",
			"name": &"Siren",
			"role": &"support",
			"max_hp": 80.0,
			"attack_damage": 14.0,
			"attack_range": 3.5,
			"attack_speed": 1.05,
			"move_speed": 0.48,
			"armor": 0.04,
			"magic_resist": 0.13,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 80.0,
			"mana_per_attack": 10.0,
			"ability_cd": 10.5,
			"ultimate_cd": 37.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"siphon",
			"respawn_time": 0.0,
		},
		"description": "A captivating support who lures enemies into stuns and drains their mana with every haunting song.",
		"ability_desc": "Binds target stunning for 0.5s stun.",
		"ultimate_desc": "Shrieks for 280% magic damage and stunning for 1.0s.",
		"passive_desc": "Drains 5 mana from the target on each auto-attack.",
		"ability": {"kind": &"stun", "params": {"duration": 0.5, "reason": "Enthralling Song"}},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"damage", "params": {"damage_multiplier": 2.8, "damage_type": "magic", "reason": "Banshee Wail", "trigger_on_hit": false}},
					{"kind": &"stun", "params": {"duration": 1.0, "reason": "Banshee Wail"}},
				],
				"reason": "Banshee Wail",
			},
		},
		"passive_ids": [&"siphon"],
	},
	&"valkyrie": {
		"stats": {
			"unit_id": &"valkyrie",
			"name": &"Valkyrie",
			"role": &"fighter",
			"max_hp": 290.0,
			"attack_damage": 26.0,
			"attack_range": 0.3,
			"attack_speed": 1.5,
			"move_speed": 0.74,
			"armor": 0.30,
			"magic_resist": 0.24,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 60.0,
			"mana_per_attack": 10.0,
			"ability_cd": 5.0,
			"ultimate_cd": 20.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"bravery",
			"respawn_time": 0.0,
		},
		"description": "A formidable bruiser who grows more dangerous as she fights, slamming foes with her shield.",
		"ability_desc": "Bashes target for 220% damage and 1.0s stun.",
		"ultimate_desc": "War cry dealing 430% damage and 2.3s stun.",
		"passive_desc": "Deals 25% bonus damage while above 80% HP.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"damage", "params": {"damage_multiplier": 2.2, "reason": "Shield Slam", "trigger_on_hit": false}},
					{"kind": &"stun", "params": {"duration": 1.0, "reason": "Shield Slam"}},
				],
				"reason": "Shield Slam",
			},
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"damage", "params": {"damage_multiplier": 4.3, "reason": "Valhalla Call", "trigger_on_hit": false}},
					{"kind": &"stun", "params": {"duration": 2.3, "reason": "Valhalla Call"}},
				],
				"reason": "Valhalla Call",
			},
		},
		"passive_ids": [&"aura_damage"],
	},
}

const PASSIVE_DATA := {
	&"duelist": {
		&"on_attack": [{"kind": &"constant_multiplier", "params": {"multiplier": 1.2}}],
	},
	&"eagle_eye": {
		&"on_attack": [{"kind": &"constant_multiplier", "params": {"multiplier": 1.25}}],
		&"post_attack": [
			{
				"kind": &"threshold_splash_damage",
				"params": {
					"threshold_multiplier": 3.0,
					"splash": {
						"kind": &"splash_damage",
						"params": {
							"radius": 2.0,
							"ratio": 0.5,
							"damage_type": "physical",
							"reason": "Rain of Arrows",
							"color": [34, 139, 34],
						},
					},
				},
			},
		],
	},
	&"bastion": {
		&"on_defense": [{"kind": &"constant_multiplier", "params": {"multiplier": 0.9}}],
	},
	&"executioner": {
		&"on_attack": [{"kind": &"target_hp_threshold_multiplier", "params": {"hp_ratio_threshold": 0.3, "multiplier": 2.0}}],
		&"on_ability": [{"kind": &"target_hp_threshold_multiplier", "params": {"hp_ratio_threshold": 0.3, "multiplier": 2.0}}],
		&"on_ultimate": [{"kind": &"target_hp_threshold_multiplier", "params": {"hp_ratio_threshold": 0.3, "multiplier": 2.0}}],
	},
	&"mana_font": {
		&"on_tick": [{"kind": &"mana_regen", "params": {"flat_amount": 3.0}}],
	},
	&"marksman": {
		&"on_attack": [{"kind": &"distance_threshold_multiplier", "params": {"min_distance": 3.0, "multiplier": 1.25}}],
	},
	&"bloodlust": {
		&"post_attack": [{"kind": &"damage_based_heal", "params": {"heal_ratio": 0.15}}],
	},
	&"rejuvenation": {
		&"on_tick": [{"kind": &"heal", "params": {"flat_amount": 5.0, "reason": "Rejuvenation"}}],
	},
	&"agility": {
		&"on_defense": [{"kind": &"dodge", "params": {"dodge_chance": 0.25, "on_dodge_multiplier": 0.0, "on_hit_multiplier": 1.0}}],
	},
	&"enlightenment": {
		&"post_attack": [{"kind": &"mana_restore_on_hit", "params": {"flat_amount": 5.0}}],
	},
	&"durability": {
		&"on_defense": [{"kind": &"constant_multiplier", "params": {"multiplier": 0.9}}],
	},
	&"swiftness": {
		&"on_attack": [{"kind": &"constant_multiplier", "params": {"multiplier": 1.15}}],
	},
	&"vampirism": {
		&"post_attack": [{"kind": &"heal", "params": {"flat_amount": 3.0, "reason": "Vampirism"}}],
	},
	&"technique": {
		&"post_attack": [{"kind": &"every_n_attacks_stun", "params": {"every_n": 3, "stun_duration": 0.5}}],
	},
	&"demolition": {
		&"post_attack": [{"kind": &"splash_damage", "params": {"radius": 0.5, "ratio": 0.5, "damage_type": "physical", "reason": "Explosion", "color": [255, 100, 50]}}],
	},
	&"devotion": {
		&"on_tick": [{"kind": &"heal", "params": {"max_hp_ratio": 0.02, "reason": "Devotion"}}],
	},
	&"siphon": {
		&"post_attack": [{"kind": &"drain_target_mana_on_hit", "params": {"flat_amount": 5.0}}],
	},
	&"bravery": {
		&"on_attack": [{"kind": &"self_hp_threshold_multiplier", "params": {"min_hp_ratio": 0.8, "multiplier": 1.2}}],
	},
	&"aura_damage": {
		&"on_tick": [{
			"kind": &"multi",
			"params": {
				"effects": [
					{"kind": &"self_aoe_damage", "params": {"radius": 1.0, "flat_amount": 5.0, "damage_type": "physical", "reason": "Aura Damage"}},
					{"kind": &"damage_based_heal", "params": {"heal_ratio": 1.0}},
				],
			},
		}],
	},
}

const ROLE_CONFIG_DATA := {
	&"tank": {
		"stat_mods": {},
		"passive_on_tick": null,
		"passive_post_take_damage": {"kind": &"post_damage_mana_gain", "params": {"damage_ratio": SimConstantsScript.TANK_MANA_GAIN_DAMAGE_RATIO}},
	},
	&"fighter": {
		"stat_mods": {},
		"passive_on_tick": null,
		"passive_post_take_damage": null,
	},
	&"marksman": {
		"stat_mods": {},
		"passive_on_tick": null,
		"passive_post_take_damage": null,
	},
	&"assassin": {
		"stat_mods": {},
		"passive_on_tick": null,
		"passive_post_take_damage": null,
	},
	&"mage": {
		"stat_mods": {},
		"passive_on_tick": {"kind": &"mana_regen", "params": {"flat_amount": SimConstantsScript.MAGE_MANA_REGEN_TICK}},
		"passive_post_take_damage": null,
	},
	&"support": {
		"stat_mods": {},
		"passive_on_tick": null,
		"passive_post_take_damage": null,
	},
}

static func build_role_configs() -> Dictionary:
	if not _role_config_cache.is_empty():
		return _role_config_cache

	for role_id in ROLE_CONFIG_DATA:
		var data: Dictionary = ROLE_CONFIG_DATA[role_id]
		var stat_mods: Dictionary = data["stat_mods"].duplicate()
		
		var passive_on_tick = null
		if data["passive_on_tick"] != null:
			passive_on_tick = _build_effect(data["passive_on_tick"])
		
		var passive_post_take_damage = null
		if data["passive_post_take_damage"] != null:
			passive_post_take_damage = _build_effect(data["passive_post_take_damage"])
		
		_role_config_cache[role_id] = RoleConfigSpecScript.new(stat_mods, passive_on_tick, passive_post_take_damage)
	
	return _role_config_cache

static func build_catalog() -> Dictionary:
	if not _catalog_cache.is_empty():
		return _catalog_cache

	for unit_id in CHAMPION_DATA:
		_catalog_cache[unit_id] = _build_champion(CHAMPION_DATA[unit_id])
	
	return _catalog_cache

static func build_passive_registry() -> Dictionary:
	if not _passive_cache.is_empty():
		return _passive_cache

	for passive_id in PASSIVE_DATA:
		var data: Dictionary = PASSIVE_DATA[passive_id]
		var result: Dictionary = {}
		
		for hook in data:
			var effects_data: Array = data[hook]
			var built_effects: Array = []
			for effect_data in effects_data:
				built_effects.append(_build_effect(effect_data))
			result[hook] = built_effects
		
		_passive_cache[passive_id] = result
	
	return _passive_cache

static func get_passive_entry(passive_id: StringName):
	return build_passive_registry().get(passive_id, {})

static func get_champion_ids() -> Array[StringName]:
	if _champion_ids_cache.is_empty():
		var ids: Array[StringName] = []
		for unit_id in build_catalog().keys():
			ids.append(StringName(String(unit_id)))
		_champion_ids_cache = ids
	return _champion_ids_cache.duplicate()

static func get_champion(unit_id: StringName):
	return build_catalog().get(unit_id, null)

static func _load_role_kits() -> void:
	if _role_kits_loaded:
		return
	var file := FileAccess.open("res://fixtures/goldens/role_kits.json", FileAccess.READ)
	if file == null:
		_role_kits = {}
		_role_kits_loaded = true
		return
	var json_string := file.get_as_text()
	file.close()
	var json := JSON.new()
	var parse_result := json.parse(json_string)
	if parse_result != OK:
		_role_kits = {}
		_role_kits_loaded = true
		return
	var data: Dictionary = json.data
	_role_kits = data.get("kits", {})
	_role_kits_loaded = true

static func reload_role_kits() -> void:
	_role_kits_loaded = false
	_role_kits.clear()
	_role_config_cache.clear()
	_load_role_kits()

static func get_effective_stats(hero_id: StringName) -> Dictionary:
	var champion = get_champion(hero_id)
	if champion == null:
		return {}
	var stats_dict: Dictionary = champion.stats.to_dict().duplicate(true)
	var role: StringName = stats_dict.get("role", &"")
	
	# Apply role kit overrides
	_load_role_kits()
	for kit_id in _role_kits.keys():
		var kit: Dictionary = _role_kits[kit_id]
		if kit.get("role", "") == String(role):
			var kit_stat_mods: Dictionary = kit.get("stat_mods", {})
			for key in kit_stat_mods.keys():
				var mod_data: Dictionary = kit_stat_mods[key]
				var mod_type: String = mod_data.get("type", "add")
				var mod_value: float = mod_data.get("value", 0.0)
				var current_value: float = stats_dict.get(key, 0.0)
				match mod_type:
					"multiply":
						stats_dict[key] = current_value * mod_value
					"divide":
						stats_dict[key] = current_value / mod_value if mod_value != 0.0 else current_value
					"subtract":
						stats_dict[key] = current_value - mod_value
					_:  # add (default)
						stats_dict[key] = current_value + mod_value
	
	return stats_dict

static func export_schema_dict() -> Dictionary:
	var schema: Dictionary = {}
	var catalog := build_catalog()
	for unit_id in catalog.keys():
		var champ = catalog[unit_id]
		schema[String(unit_id)] = champ.to_dict()
	return schema
