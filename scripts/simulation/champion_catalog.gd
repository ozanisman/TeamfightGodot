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

static func _effect(kind: StringName, params: Dictionary = {}):
	return EffectSpecScript.new(kind, params)

static func _stats(
	unit_id: StringName,
	name: StringName,
	role: StringName,
	max_hp: float,
	attack_damage: float,
	attack_range: float,
	attack_speed: float,
	move_speed: float,
	armor: float = 0.0,
	magic_resist: float = 0.0,
	tenacity: float = 0.0,
	life_steal: float = 0.0,
	max_mana: float = 50.0,
	mana_per_attack: float = 10.0,
	ability_cd: float = 5.0,
	ultimate_cd: float = 20.0,
	projectile_speed: float = SimConstantsScript.DEFAULT_PROJECTILE_SPEED,
	projectile_radius: float = SimConstantsScript.DEFAULT_PROJECTILE_RADIUS,
	passive_id: StringName = &"",
	respawn_time: float = SimConstantsScript.RESPAWN_TIME
):
	var stats := ChampionStatsScript.new()
	stats.unit_id = unit_id
	stats.name = name
	stats.role = role
	stats.max_hp = max_hp
	stats.attack_damage = attack_damage
	stats.attack_range = attack_range
	stats.attack_speed = attack_speed
	stats.move_speed = move_speed
	stats.armor = armor
	stats.magic_resist = magic_resist
	stats.tenacity = tenacity
	stats.life_steal = life_steal
	stats.max_mana = max_mana
	stats.mana_per_attack = mana_per_attack
	stats.ability_cd = ability_cd
	stats.ultimate_cd = ultimate_cd
	stats.projectile_speed = projectile_speed
	stats.projectile_radius = projectile_radius
	stats.passive_id = passive_id
	stats.respawn_time = respawn_time
	return stats

static func _champ(
	stats,
	description: String,
	ability_desc: String,
	ultimate_desc: String,
	passive_desc: String,
	ability,
	ultimate,
	passive_ids: Array[StringName]
):
	return ChampionSpecScript.new(stats, description, ability_desc, ultimate_desc, passive_desc, ability, ultimate, passive_ids)

static func _role_config(
	stat_mods: Dictionary = {},
	passive_on_tick = null,
	passive_post_take_damage = null
):
	return RoleConfigSpecScript.new(stat_mods, passive_on_tick, passive_post_take_damage)

static func build_role_configs() -> Dictionary:
	if not _role_config_cache.is_empty():
		return _role_config_cache

	_role_config_cache = {
		&"tank": _role_config(
			{&"tenacity": SimConstantsScript.TANK_TENACITY_MOD},
			null,
			_effect(&"post_damage_mana_gain", {&"damage_ratio": SimConstantsScript.TANK_MANA_GAIN_DAMAGE_RATIO})
		),
		&"fighter": _role_config(
			{&"life_steal": SimConstantsScript.FIGHTER_LIFESTEAL_MOD, &"tenacity": SimConstantsScript.FIGHTER_TENACITY_MOD}
		),
		&"marksman": _role_config(
			{&"attack_speed": SimConstantsScript.MARKSMAN_AS_MOD}
		),
		&"assassin": _role_config(
			{&"move_speed": SimConstantsScript.ASSASSIN_MS_MOD}
		),
		&"mage": _role_config(
			{},
			_effect(&"mana_regen", {&"flat_amount": SimConstantsScript.MAGE_MANA_REGEN_TICK})
		),
		&"support": _role_config(
			{&"ability_cd": SimConstantsScript.SUPPORT_ABILITY_CD_FLAT}
		),
	}
	return _role_config_cache

static func build_catalog() -> Dictionary:
	if not _catalog_cache.is_empty():
		return _catalog_cache

	_catalog_cache = {
		&"swordsman": _champ(
			_stats(&"swordsman", &"Swordsman", &"fighter", 120.0, 14.0, 0.3, 1.2, 1.1, 0.25, 0.15, 0.0, 0.0, 50.0, 10.0, 6.0, 25.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"duelist"),
			"A balanced frontline duelist who excels at locking down single targets with concussive strikes.",
			"Deals 150% damage and stuns the target for 1.0s.",
			"Strikes for 300% damage and applies a 2.0s stun.",
			"Increases attack damage by 20%.",
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 1.5, &"reason": "Concussive Blow", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 1.0, &"reason": "Concussive Blow"}).to_dict(),
				],
				&"reason": "Concussive Blow",
			}),
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 3.0, &"reason": "Blade Dance", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 2.0, &"reason": "Blade Dance"}).to_dict(),
				],
				&"reason": "Blade Dance",
			}),
			[&"duelist"]
		),
		&"archer": _champ(
			_stats(&"archer", &"Archer", &"marksman", 75.0, 21.0, 3.0, 0.9, 1.0, 0.05, 0.05, 0.0, 0.0, 100.0, 10.0, 8.0, 30.0, 8.0, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"eagle_eye"),
			"A classic long-range scout that provides consistent physical DPS from the backline.",
			"Fires a focused shot dealing 150% damage.",
			"Rains down arrows for 400% massive physical damage and deals splash damage on impact.",
			"Increases attack damage by 25%. Rain of Arrows deals 50% splash damage in a 2.0 unit radius.",
			_effect(&"projectile", {&"damage_multiplier": 1.5, &"reason": "Volley"}),
			_effect(&"projectile", {&"damage_multiplier": 4.0, &"reason": "Rain of Arrows", &"radius_override": 0.06}),
			[&"eagle_eye"]
		),
		&"guardian": _champ(
			_stats(&"guardian", &"Guardian", &"tank", 220.0, 10.0, 0.3, 0.8, 1.1, 0.35, 0.25, 0.0, 0.0, 100.0, 10.0, 10.0, 40.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"bastion"),
			"A hulking protector who uses heavy shields to soak damage and a massive slam to stun entire groups.",
			"Grants a shield worth 20% max HP to self or an ally.",
			"Slams the ground for 200% damage and a 3.0s stun.",
			"Reduces incoming damage by 10%.",
			_effect(&"shield", {&"max_hp_ratio": 0.2, &"reason": "Guardian Shield"}),
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 2.0, &"reason": "Aegis Crash", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 3.0, &"reason": "Aegis Crash"}).to_dict(),
				],
				&"reason": "Aegis Crash",
			}),
			[&"bastion"]
		),
		&"assassin": _champ(
			_stats(&"assassin", &"Assassin", &"assassin", 165.0, 25.0, 0.3, 1.4, 1.3, 0.05, 0.05, 0.0, 0.0, 60.0, 10.0, 4.0, 20.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"executioner"),
			"A high-mobility predator designed to dive the backline and execute wounded targets with lethal precision.",
			"Quick dash for 120% damage and a short 0.5s stun.",
			"Executes a target with 500% physical damage.",
			"Deals double damage to targets below 30% HP.",
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 1.2, &"reason": "Shadow Dash", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 0.5, &"reason": "Shadow Dash"}).to_dict(),
				],
				&"reason": "Shadow Dash",
			}),
			_effect(&"projectile", {&"damage_multiplier": 5.0, &"reason": "Assassinate"}),
			[&"executioner"]
		),
		&"mage": _champ(
			_stats(&"mage", &"Mage", &"mage", 80.0, 19.0, 3.5, 1.0, 1.2, 0.05, 0.25, 0.0, 0.0, 80.0, 10.0, 7.0, 22.0, 6.0, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"mana_font"),
			"A powerful spellcaster with a deep mana pool, capable of unleashing frequent bursts of magic damage.",
			"Fires a magical projectile dealing 200% magic damage.",
			"Calls down a meteor for 600% magic damage and deals splash damage on impact.",
			"Restores 3 mana every second. Meteor impact deals 50% splash damage in a 2.5 unit radius.",
			_effect(&"projectile", {&"damage_multiplier": 2.0, &"damage_type": "magic", &"speed_override": 7.0, &"reason": "Arcane Bolt"}),
			_effect(&"projectile", {&"damage_multiplier": 6.0, &"damage_type": "magic", &"speed_override": 4.0, &"radius_override": 0.07, &"reason": "Meteor Bombardment"}),
			[&"mana_font"]
		),
		&"sniper": _champ(
			_stats(&"sniper", &"Sniper", &"marksman", 60.0, 21.0, 4.0, 0.4, 0.9, 0.05, 0.05, 0.0, 0.0, 60.0, 10.0, 12.0, 45.0, 12.0, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"marksman"),
			"An elite marksman with extreme range, specializing in devastating single-target ultimate shots.",
			"High-precision strike for 180% damage.",
			"Lethal long-range shot for 350% damage.",
			"Deals 25% bonus damage to targets further than 3 units away.",
			_effect(&"projectile", {&"damage_multiplier": 1.8, &"reason": "Headshot"}),
			_effect(&"projectile", {&"damage_multiplier": 3.5, &"speed_override": 15.0, &"reason": "ULTIMATE"}),
			[&"marksman"]
		),
		&"berserker": _champ(
			_stats(&"berserker", &"Berserker", &"fighter", 185.0, 12.0, 0.3, 1.6, 1.3, 0.15, 0.05, 0.0, 0.0, 60.0, 10.0, 5.0, 18.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"bloodlust"),
			"A savage warrior who thrives in the heat of battle, healing himself through sheer aggression.",
			"Damages self for 10% max HP and gains 150% of the value as a shield.",
			"Unleashes a devastating strike for 300% true damage.",
			"Heals for 15% of all damage dealt by auto-attacks.",
			_effect(&"multi", {
				&"effects": [
					_effect(&"self_damage", {&"damage_ratio": 0.1, &"reason": "Blood Price"}).to_dict(),
					_effect(&"self_shield", {&"shield_ratio": 0.15, &"reason": "Blood Price"}).to_dict(),
				],
				&"reason": "Blood Price",
			}),
			_effect(&"damage", {&"damage_multiplier": 3.0, &"damage_type": "true", &"trigger_on_hit": false, &"reason": "Berserker Rage"}),
			[&"bloodlust"]
		),
		&"paladin": _champ(
			_stats(&"paladin", &"Paladin", &"tank", 200.0, 14.0, 0.3, 0.8, 1.4, 0.25, 0.25, 0.0, 0.0, 80.0, 10.0, 10.0, 35.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"rejuvenation"),
			"A holy knight who balances immense durability with powerful self-healing and divine judgment.",
			"Heals self for 15% max HP.",
			"Heals self for 30% max HP and deals 200% damage.",
			"Regenerates 5 HP every second.",
			_effect(&"heal", {&"max_hp_ratio": 0.15, &"reason": "Lay on Hands"}),
			_effect(&"multi", {
				&"effects": [
					_effect(&"heal", {&"max_hp_ratio": 0.3, &"reason": "Divine Judgment"}).to_dict(),
					_effect(&"damage", {&"damage_multiplier": 2.0, &"reason": "Divine Judgment", &"trigger_on_hit": false}).to_dict(),
				],
				&"reason": "Divine Judgment",
			}),
			[&"rejuvenation"]
		),
		&"rogue": _champ(
			_stats(&"rogue", &"Rogue", &"assassin", 90.0, 18.0, 0.3, 1.8, 2.4, 0.10, 0.10, 0.0, 0.0, 40.0, 10.0, 4.0, 18.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"agility"),
			"An evasive melee fighter who relies on high dodge chance and speed to survive encounters.",
			"Deals 80% magic damage and stuns for 2.0s.",
			"Lethal execution dealing 350% physical damage.",
			"Grants a 25% chance to dodge incoming damage.",
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 0.8, &"damage_type": "magic", &"reason": "Poison Vial", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 2.0, &"reason": "Poison Vial"}).to_dict(),
				],
				&"reason": "Poison Vial",
			}),
			_effect(&"damage", {&"damage_multiplier": 3.5, &"reason": "Eviscerate"}),
			[&"agility"]
		),
		&"oracle": _champ(
			_stats(&"oracle", &"Oracle", &"support", 90.0, 15.0, 3.5, 1.1, 1.5, 0.05, 0.20, 0.0, 0.0, 150.0, 10.0, 6.0, 24.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"enlightenment"),
			"A mystical seer who converts mana into healing, sustaining allies through long-range purification.",
			"Heals an ally or self for 20% max HP.",
			"Shields an ally or self for 40% max HP.",
			"Restores 5 mana after each auto-attack.",
			_effect(&"heal", {&"max_hp_ratio": 0.2, &"reason": "Purify"}),
			_effect(&"shield", {&"max_hp_ratio": 0.4, &"reason": "Divine Shield"}),
			[&"enlightenment"]
		),
		&"colossus": _champ(
			_stats(&"colossus", &"Colossus", &"tank", 350.0, 8.0, 0.3, 0.6, 0.8, 0.30, 0.25, 0.0, 0.0, 100.0, 10.0, 12.0, 50.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, 0.05, &"tenacity"),
			"A mountain of a tank with unmatched physical resistance and the ability to stun the entire battlefield.",
			"Deals 100% damage and taunts all targets in a 1 unit radius for 2.0s.",
			"Colossal impact for 250% damage and a 3.5s stun.",
			"Reduces all incoming damage by 10%.",
			_effect(&"multi", {
				&"effects": [
					_effect(&"self_aoe_taunt", {&"radius": 1.0, &"duration": 2.0, &"reason": "Seismic Slam"}).to_dict(),
					_effect(&"self_aoe_damage", {&"radius": 1.0, &"damage_multiplier": 1.0, &"damage_type": "physical", &"reason": "Seismic Slam"}).to_dict(),
				],
				&"reason": "Seismic Slam",
			}),
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 2.5, &"reason": "Earthshaker", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 3.5, &"reason": "Earthshaker"}).to_dict(),
				],
				&"reason": "Earthshaker",
			}),
			[&"tenacity"]
		),
		&"wraith": _champ(
			_stats(&"wraith", &"Wraith", &"assassin", 155.0, 26.0, 0.3, 1.5, 2.5, 0.05, 0.15, 0.0, 0.0, 40.0, 10.0, 5.0, 15.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"swiftness"),
			"A terrifying spectral assassin that stuns its prey and moves faster than the eye can see.",
			"Magic strike for 120% damage and 0.8s stun.",
			"Teleports for 250% magic damage and 1.5s stun.",
			"Increases attack damage by 15%.",
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 1.2, &"damage_type": "magic", &"reason": "Spectral Touch", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 0.8, &"reason": "Spectral Touch"}).to_dict(),
				],
				&"reason": "Spectral Touch",
			}),
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 2.5, &"damage_type": "magic", &"reason": "Phantom Strike", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 1.5, &"reason": "Phantom Strike"}).to_dict(),
				],
				&"reason": "Phantom Strike",
			}),
			[&"swiftness"]
		),
		&"warlock": _champ(
			_stats(&"warlock", &"Warlock", &"mage", 100.0, 21.0, 3.5, 0.9, 1.4, 0.05, 0.20, 0.0, 0.0, 100.0, 10.0, 6.0, 22.0, 5.0, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"vampirism"),
			"A dark sorcerer who siphons the life force of his enemies to sustain himself and his allies.",
			"Deals 150% magic damage and heals self for 20% of it.",
			"Rifts the ground for 400% magic damage and 1.0s stun.",
			"Heals for 3 HP after each auto-attack.",
			_effect(&"projectile", {&"damage_multiplier": 1.5, &"damage_type": "magic", &"reason": "Soul Siphon"}),
			_effect(&"projectile", {&"damage_multiplier": 4.0, &"damage_type": "magic", &"stun_duration": 1.0, &"reason": "Chaos Rift"}),
			[&"vampirism"]
		),
		&"monk": _champ(
			_stats(&"monk", &"Monk", &"fighter", 155.0, 18.0, 0.3, 2.2, 1.4, 0.10, 0.10, 0.0, 0.0, 60.0, 10.0, 4.0, 16.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"technique"),
			"A martial arts master who uses a flurry of strikes to incapacitate foes through precise pressure points.",
			"Precise strike for 120% damage and 0.8s stun.",
			"Rapid flurry for 250% damage and 1.5s stun.",
			"Every 3rd attack stuns the target for 0.5s.",
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 1.2, &"reason": "Pressure Point", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 0.8, &"reason": "Pressure Point"}).to_dict(),
				],
				&"reason": "Pressure Point",
			}),
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 2.5, &"reason": "Hundred-Hand Slap", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 1.5, &"reason": "Hundred-Hand Slap"}).to_dict(),
				],
				&"reason": "Hundred-Hand Slap",
			}),
			[&"technique"]
		),
		&"artillery": _champ(
			_stats(&"artillery", &"Artillery", &"marksman", 55.0, 25.0, 5.0, 0.4, 0.3, 0.05, 0.05, 0.0, 0.0, 50.0, 10.0, 14.0, 50.0, 3.0, 0.05, &"demolition"),
			"A fragile but explosive backline siege unit that deals massive damage to anything in its sights.",
			"Explosive shell dealing 120% damage and 0.8s stun.",
			"Fires a massive artillery shell for 500% damage.",
			"Attacks and abilities deal 50% splash damage to enemies within 0.5 units of the target.",
			_effect(&"projectile", {&"damage_multiplier": 1.2, &"stun_duration": 0.8, &"reason": "Shell Shock"}),
			_effect(&"projectile", {&"damage_multiplier": 5.0, &"radius_override": 0.08, &"reason": "Big Bertha"}),
			[&"demolition"]
		),
		&"cleric": _champ(
			_stats(&"cleric", &"Cleric", &"support", 125.0, 15.0, 3.5, 1.2, 1.5, 0.10, 0.20, 0.0, 0.0, 100.0, 10.0, 8.0, 32.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"devotion"),
			"A dedicated holy healer who provides constant HP regeneration and massive burst heals for her team.",
			"Heals an ally or self for 30% max HP.",
			"Heals for 55% max HP and stuns target for 1.5s.",
			"Regenerates 2% of max HP every second.",
			_effect(&"heal", {&"max_hp_ratio": 0.3, &"reason": "Holy Mending"}),
			_effect(&"multi", {
				&"effects": [
					_effect(&"heal", {&"max_hp_ratio": 0.55, &"reason": "Divine Aura"}).to_dict(),
					_effect(&"stun", {&"duration": 1.5, &"reason": "Divine Aura"}).to_dict(),
				],
				&"reason": "Divine Aura",
			}),
			[&"devotion"]
		),
		&"siren": _champ(
			_stats(&"siren", &"Siren", &"support", 85.0, 15.0, 3.5, 1.1, 1.6, 0.05, 0.15, 0.0, 0.0, 80.0, 10.0, 10.0, 35.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"siphon"),
			"A captivating support who lures enemies into stuns and drains their mana with every haunting song.",
			"Binds target stunning for 0.5s stun.",
			"Shrieks for 300% magic damage and stunning for 1.0s.",
			"Drains 5 mana from the target on each auto-attack.",
			_effect(&"stun", {&"duration": 0.5, &"reason": "Enthralling Song"}),
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 3.0, &"damage_type": "magic", &"reason": "Banshee Wail", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 1.0, &"reason": "Banshee Wail"}).to_dict(),
				],
				&"reason": "Banshee Wail",
			}),
			[&"siphon"]
		),
		&"valkyrie": _champ(
			_stats(&"valkyrie", &"Valkyrie", &"fighter", 210.0, 15.0, 0.3, 1.1, 1.5, 0.20, 0.15, 0.0, 0.0, 60.0, 10.0, 7.0, 28.0, SimConstantsScript.DEFAULT_PROJECTILE_SPEED, SimConstantsScript.DEFAULT_PROJECTILE_RADIUS, &"bravery"),
			"A formidable bruiser who grows more dangerous as she fights, slamming foes with her shield.",
			"Bashes target for 150% damage and 0.5s stun.",
			"War cry dealing 300% damage and 1.5s stun.",
			"Deals 20% bonus damage while above 80% HP.",
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 1.5, &"reason": "Shield Slam", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 0.5, &"reason": "Shield Slam"}).to_dict(),
				],
				&"reason": "Shield Slam",
			}),
			_effect(&"multi", {
				&"effects": [
					_effect(&"damage", {&"damage_multiplier": 3.0, &"reason": "Valhalla Call", &"trigger_on_hit": false}).to_dict(),
					_effect(&"stun", {&"duration": 1.5, &"reason": "Valhalla Call"}).to_dict(),
				],
				&"reason": "Valhalla Call",
			}),
			[&"bravery"]
		),
	}
	return _catalog_cache

static func build_passive_registry() -> Dictionary:
	if not _passive_cache.is_empty():
		return _passive_cache

	_passive_cache = {
		&"duelist": {
			&"on_attack": [_effect(&"constant_multiplier", {&"multiplier": 1.2})],
		},
		&"eagle_eye": {
			&"on_attack": [_effect(&"constant_multiplier", {&"multiplier": 1.25})],
			&"post_attack": [
				_effect(&"threshold_splash_damage", {
					&"threshold_multiplier": 3.0,
					&"splash": _effect(&"splash_damage", {
						&"radius": 2.0,
						&"ratio": 0.5,
						&"damage_type": "physical",
						&"reason": "Rain of Arrows",
						&"color": [34, 139, 34],
					}).to_dict(),
				}),
			],
		},
		&"bastion": {
			&"on_defense": [_effect(&"constant_multiplier", {&"multiplier": 0.9})],
		},
		&"executioner": {
			&"on_attack": [_effect(&"target_hp_threshold_multiplier", {&"hp_ratio_threshold": 0.3, &"multiplier": 2.0})],
		},
		&"mana_font": {
			&"on_tick": [_effect(&"mana_regen", {&"flat_amount": 3.0})],
		},
		&"marksman": {
			&"on_attack": [_effect(&"distance_threshold_multiplier", {&"min_distance": 3.0, &"multiplier": 1.25})],
		},
		&"bloodlust": {
			&"post_attack": [_effect(&"damage_based_heal", {&"heal_ratio": 0.15})],
		},
		&"rejuvenation": {
			&"on_tick": [_effect(&"heal", {&"flat_amount": 5.0, &"reason": "Rejuvenation"})],
		},
		&"agility": {
			&"on_defense": [_effect(&"dodge", {&"dodge_chance": 0.25, &"on_dodge_multiplier": 0.0, &"on_hit_multiplier": 1.0})],
		},
		&"enlightenment": {
			&"post_attack": [_effect(&"mana_restore_on_hit", {&"flat_amount": 5.0})],
		},
		&"tenacity": {
			&"on_defense": [_effect(&"constant_multiplier", {&"multiplier": 0.9})],
		},
		&"swiftness": {
			&"on_attack": [_effect(&"constant_multiplier", {&"multiplier": 1.15})],
		},
		&"vampirism": {
			&"post_attack": [_effect(&"heal", {&"flat_amount": 3.0, &"reason": "Vampirism"})],
		},
		&"technique": {
			&"post_attack": [_effect(&"every_n_attacks_stun", {&"every_n": 3, &"stun_duration": 0.5})],
		},
		&"demolition": {
			&"post_attack": [_effect(&"splash_damage", {&"radius": 0.5, &"ratio": 0.5, &"damage_type": "physical", &"reason": "Explosion", &"color": [255, 100, 50]})],
		},
		&"devotion": {
			&"on_tick": [_effect(&"heal", {&"max_hp_ratio": 0.02, &"reason": "Devotion"})],
		},
		&"siphon": {
			&"post_attack": [_effect(&"drain_target_mana_on_hit", {&"flat_amount": 5.0})],
		},
		&"bravery": {
			&"on_attack": [_effect(&"self_hp_threshold_multiplier", {&"min_hp_ratio": 0.8, &"multiplier": 1.2})],
		},
	}
	return _passive_cache

static func get_passive_entry(passive_id: StringName):
	return build_passive_registry().get(passive_id, {})

static func get_champion_ids() -> Array[StringName]:
	var ids: Array[StringName] = []
	for unit_id in build_catalog().keys():
		ids.append(StringName(String(unit_id)))
	return ids

static func get_champion(unit_id: StringName):
	return build_catalog().get(unit_id, null)

static func export_schema_dict() -> Dictionary:
	var schema: Dictionary = {}
	var catalog := build_catalog()
	for unit_id in catalog.keys():
		var champ = catalog[unit_id]
		schema[String(unit_id)] = champ.to_dict()
	return schema
