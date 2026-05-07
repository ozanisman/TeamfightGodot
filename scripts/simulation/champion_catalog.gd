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

# Thread-local cache storage for safe multi-threading
static var _thread_local_caches: Dictionary = {}
static var _cache_mutex: Mutex = Mutex.new()

static func _get_thread_id() -> int:
	# Get current thread ID for thread-local storage
	return OS.get_thread_caller_id()

static func _get_thread_cache() -> Dictionary:
	var thread_id = _get_thread_id()
	_cache_mutex.lock()
	if not _thread_local_caches.has(thread_id):
		_thread_local_caches[thread_id] = {
			"catalog": {},
			"passive": {},
			"role_config": {},
			"champion_ids": []
		}
	var cache = _thread_local_caches[thread_id]
	_cache_mutex.unlock()
	return cache

static func clear_thread_cache() -> void:
	var thread_id = _get_thread_id()
	_cache_mutex.lock()
	_thread_local_caches.erase(thread_id)
	_cache_mutex.unlock()

# Array pool for effect building optimization
static var _array_pool: Array[Array] = []
static var _pool_mutex: Mutex = Mutex.new()

static func _get_pooled_array() -> Array:
	_pool_mutex.lock()
	if _array_pool.is_empty():
		_pool_mutex.unlock()
		return []
	var pooled_array: Array = _array_pool.pop_back()
	_pool_mutex.unlock()
	pooled_array.clear()
	return pooled_array

static func _return_pooled_array(array: Array) -> void:
	if array.size() > 32:  # Don't pool oversized arrays
		return
	_pool_mutex.lock()
	if _array_pool.size() < 16:  # Limit pool size
		_array_pool.push_back(array)
	_pool_mutex.unlock()

static func clear_all_caches() -> void:
	_cache_mutex.lock()
	_catalog_cache.clear()
	_passive_cache.clear()
	_role_config_cache.clear()
	_champion_ids_cache.clear()
	_thread_local_caches.clear()
	_cache_mutex.unlock()
	
	# Clear array pool
	_pool_mutex.lock()
	_array_pool.clear()
	_pool_mutex.unlock()

static func clear_export_caches() -> void:
	# Clear caches before export to ensure fresh data
	clear_all_caches()

static func _build_effect(data: Dictionary) -> EffectSpecScript:
	var params: Dictionary = data["params"].duplicate()
	var requires_target_in_range: bool = bool(data.get("requires_target_in_range", true))
	var pooled_arrays: Array[Array] = []
	
	for key in params:
		var value = params[key]
		if key == "effects" and value is Array:
			var built_effects: Array = _get_pooled_array()
			pooled_arrays.append(built_effects)
			for effect_data in value:
				built_effects.append(_build_effect(effect_data))
			params[key] = built_effects
		elif key == "splash" and value is Dictionary:
			params[key] = _build_effect(value)
	
	var result = EffectSpecScript.new(data["kind"], params, requires_target_in_range)
	
	# Return arrays to pool after effect is created
	for pooled_array in pooled_arrays:
		_return_pooled_array(pooled_array)
	
	return result

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
			"attack_damage": 28.0,
			"attack_range": 0.3,
			"attack_speed": 1.1,
			"move_speed": 0.75,
			"armor": 0.2,
			"magic_resist": 0.2,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 70.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"duelist",
			"respawn_time": 0.0,
		},
		"description": "A skilled duelist who gains attack damage with each strike and can stun groups of enemies.",
		"ability_desc": "Cleaves for 200% damage and 1.0s stun.",
		"ultimate_desc": "Whirlwind for 560% damage and 3.2s stun.",
		"passive_desc": "Gains 5 attack damage for 3 seconds after each auto-attack. (Max 5 stacks)",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 2.0,
							"trigger_on_hit": false,
							"reason": "Cleave"
						}
					},
					{
						"kind": &"stun",
						"params": {
							"duration": 1.0,
							"reason": "Cleave"
						}
					}
				],
				"reason": "Cleave"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 5.6,
							"trigger_on_hit": false,
							"reason": "Whirlwind"
						}
					},
					{
						"kind": &"stun",
						"params": {
							"duration": 3.2,
							"reason": "Whirlwind"
						}
					}
				],
				"reason": "Whirlwind"
			}
		},
		"passive_ids": [&"duelist"],
	},
	&"archer": {
		"stats": {
			"unit_id": &"archer",
			"name": &"Archer",
			"role": &"marksman",
			"max_hp": 150.0,
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
			"projectile_speed": 8.0,
			"projectile_radius": 0.0,
			"passive_id": &"eagle_eye",
			"respawn_time": 0.0,
		},
		"description": "A swift ranged attacker who fires volleys of arrows and excels at picking off distant targets.",
		"ability_desc": "Fires a volley for 140% damage.",
		"ultimate_desc": "Rain of Arrows for 380% damage with splash.",
		"passive_desc": "Increases attack damage by 25%. Rain of Arrows deals 50% splash damage in a 2.0 unit radius.",
		"ability": {
			"kind": &"projectile",
			"params": {
				"damage_ratio": 1.4,
				"reason": "Volley"
			}
		},
		"ultimate": {
			"kind": &"projectile",
			"params": {
				"damage_ratio": 3.8,
				"reason": "Rain of Arrows",
				"radius_override": 0.06
			}
		},
		"passive_ids": [&"eagle_eye"],
	},
	&"guardian": {
		"stats": {
			"unit_id": &"guardian",
			"name": &"Guardian",
			"role": &"tank",
			"max_hp": 300.0,
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
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"bastion",
			"respawn_time": 0.0,
		},
		"description": "A hulking protector who uses heavy shields to soak damage and a massive slam to stun entire groups.",
		"ability_desc": "Grants a shield worth 20% max HP to self or an ally.",
		"ultimate_desc": "Slams the ground for 290% damage and a 3.8s stun.",
		"passive_desc": "Reduces incoming damage by 10%.",
		"ability": {
			"kind": &"shield",
			"params": {
				"max_hp_ratio": 0.20,
				"reason": "Guardian Shield"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 2.9,
							"trigger_on_hit": false,
							"reason": "Aegis Crash"
						}
					},
					{
						"kind": &"stun",
						"params": {
							"duration": 3.8,
							"reason": "Aegis Crash"
						}
					}
				],
				"reason": "Aegis Crash"
			}
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
					{
						"kind": &"self_dash",
						"params": {
							"distance": 2.0
						}
					},
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.5,
							"requires_result_from": "self_dash",
							"requires_field": "reached_target",
							"requires_value": true,
							"reason": "Shadow Strike"
						}
					},
					{
						"kind": &"stun",
						"params": {
							"duration": 0.5,
							"requires_result_from": "self_dash",
							"requires_field": "reached_target",
							"requires_value": true,
							"reason": "Shadow Strike"
						}
					}
				],
				"reason": "Shadow Strike"
			},
			"requires_target_in_range": false
		},
		"ultimate": {
			"kind": &"projectile",
			"params": {
				"damage_ratio": 9.0,
				"reason": "Assassinate"
			}
		},
		"passive_ids": [&"executioner"],
	},
	&"sniper": {
		"stats": {
			"unit_id": &"sniper",
			"name": &"Sniper",
			"role": &"marksman",
			"max_hp": 150.0,
			"attack_damage": 22.0,
			"attack_range": 4.5,
			"attack_speed": 0.7,
			"move_speed": 0.4,
			"armor": 0.05,
			"magic_resist": 0.05,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 60.0,
			"mana_per_attack": 10.0,
			"ability_cd": 5.0,
			"projectile_speed": 12.0,
			"projectile_radius": 0.0,
			"passive_id": &"deadeye",
			"respawn_time": 0.0,
		},
		"description": "An elite marksman specializing in devastating single-target shots.",
		"ability_desc": "High-precision strike for 150% damage.",
		"ultimate_desc": "Lethal long-range shot for 280% damage.",
		"passive_desc": "Deals 25% bonus damage to targets further than 3 units away.",
		"ability": {
			"kind": &"projectile",
			"params": {
				"damage_ratio": 1.5,
				"reason": "Headshot"
			}
		},
		"ultimate": {
			"kind": &"projectile",
			"params": {
				"damage_ratio": 2.8,
				"speed_override": 15.0,
				"reason": "ULTIMATE"
			}
		},
		"passive_ids": [&"deadeye"],
	},
	&"berserker": {
		"stats": {
			"unit_id": &"berserker",
			"name": &"Berserker",
			"role": &"fighter",
			"max_hp": 250.0,
			"attack_damage": 23.0,
			"attack_range": 0.3,
			"attack_speed": 1.5,
			"move_speed": 0.8,
			"armor": 0.24,
			"magic_resist": 0.20,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 60.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"bloodlust",
			"respawn_time": 0.0,
		},
		"description": "A savage warrior who thrives in the heat of battle, healing himself through sheer aggression.",
		"ability_desc": "Damages self for 15% max HP and gains a 30% max HP shield.",
		"ultimate_desc": "Unleashes a devastating strike for 420% true damage.",
		"passive_desc": "Heals for 15% of his auto-attack damage.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"max_hp_ratio": 0.15,
							"damage_type": "true",
							"target_self": true,
							"reason": "Blood Price"
						}
					},
					{
						"kind": &"shield",
						"params": {
							"max_hp_ratio": 0.30,
							"target_self": true,
							"reason": "Blood Price"
						}
					}
				],
				"reason": "Blood Price"
			},
			"requires_target_in_range": false
		},
		"ultimate": {
			"kind": &"damage",
			"params": {
				"damage_ratio": 4.2,
				"damage_type": "true",
				"trigger_on_hit": false,
				"reason": "Berserker Rage"
			}
		},
		"passive_ids": [&"bloodlust"],
	},
	&"paladin": {
		"stats": {
			"unit_id": &"paladin",
			"name": &"Paladin",
			"role": &"tank",
			"max_hp": 300.0,
			"attack_damage": 21.0,
			"attack_range": 0.3,
			"attack_speed": 0.75,
			"move_speed": 0.7,
			"armor": 0.35,
			"magic_resist": 0.45,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 90.0,
			"mana_per_attack": 10.0,
			"ability_cd": 7.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"rejuvenation",
			"respawn_time": 0.0,
		},
		"description": "A holy knight who balances immense durability with powerful self-healing and divine judgment.",
		"ability_desc": "Heals self for 20% max HP.",
		"ultimate_desc": "Heals self for 40% max HP and deals 200% true damage.",
		"passive_desc": "Regenerates 2% max HP every second.",
		"ability": {
			"kind": &"heal",
			"params": {
				"max_hp_ratio": 0.20,
				"reason": "Lay on Hands"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"heal",
						"params": {
							"max_hp_ratio": 0.40,
							"reason": "Divine Judgment"
						}
					},
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 2.0,
							"damage_type": "true",
							"trigger_on_hit": false,
							"reason": "Divine Judgment"
						}
					}
				],
				"reason": "Divine Judgment"
			}
		},
		"passive_ids": [&"rejuvenation"],
	},
	&"rogue": {
		"stats": {
			"unit_id": &"rogue",
			"name": &"Rogue",
			"role": &"assassin",
			"max_hp": 190.0,
			"attack_damage": 28.0,
			"attack_range": 0.3,
			"attack_speed": 1.3,
			"move_speed": 0.98,
			"armor": 0.10,
			"magic_resist": 0.10,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 4.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"agility",
			"respawn_time": 0.0,
		},
		"description": "A nimble backline specialist who strikes with precision and vanishes before being targeted.",
		"ability_desc": "Deals 200% physical damage.",
		"ultimate_desc": "Lethal execution dealing 600% physical damage.",
		"passive_desc": "25% chance to dodge incoming auto attacks, avoiding all damage.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 2.0,
							"trigger_on_hit": false,
							"reason": "Backstab"
						}
					},
				],
				"reason": "Backstab"
			}
		},
		"ultimate": {
			"kind": &"damage",
			"params": {
				"damage_ratio": 6.0,
				"reason": "Eviscerate"
			}
		},
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
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"enlightenment",
			"respawn_time": 0.0,
		},
		"description": "A divine seer who manipulates fate through shields and mana restoration.",
		"ability_desc": "Grants a shield worth 15% max HP to self or an ally.",
		"ultimate_desc": "Restores 35% max HP and grants a shield worth 35% max HP.",
		"passive_desc": "Gains an additional 5 mana after each auto-attack.",
		"ability": {
			"kind": &"shield",
			"params": {
				"max_hp_ratio": 0.15,
				"reason": "Purify"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"shield",
						"params": {
							"max_hp_ratio": 0.35,
							"reason": "Divine Shield"
						}
					},
					{
						"kind": &"heal",
						"params": {
							"max_hp_ratio": 0.35,
							"reason": "Divine Shield"
						}
					}
				],
				"reason": "Divine Shield"
			}
		},
		"passive_ids": [&"enlightenment"],
	},
	&"colossus": {
		"stats": {
			"unit_id": &"colossus",
			"name": &"Colossus",
			"role": &"tank",
			"max_hp": 350.0,
			"attack_damage": 13.0,
			"attack_range": 0.3,
			"attack_speed": 0.6,
			"move_speed": 0.66,
			"armor": 0.45,
			"magic_resist": 0.40,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 5.5,
			"projectile_speed": 0.0,
			"projectile_radius": 0.05,
			"passive_id": &"durability",
			"respawn_time": 0.0,
		},
		"description": "A mountain of a tank with unmatched physical resistance and the ability to stun the entire battlefield.",
		"ability_desc": "Taunts all targets in a 3 unit radius for 2.5s and deals 165% damage in a 1 unit radius.",
		"ultimate_desc": "Colossal impact for 300% damage and a 2s stun.",
		"passive_desc": "Reduces all incoming damage by 10%.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"self_aoe_taunt",
						"params": {
							"radius": 3.0,
							"duration": 2.5,
							"reason": "Seismic Slam"
						}
					},
					{
						"kind": &"self_aoe_damage",
						"params": {
							"radius": 1.0,
							"damage_ratio": 1.65,
							"damage_type": "physical",
							"reason": "Seismic Slam"
						}
					}
				],
				"reason": "Seismic Slam"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 3.0,
							"trigger_on_hit": false,
							"reason": "Earthshaker"
						}
					},
					{
						"kind": &"stun",
						"params": {
							"duration": 2,
							"reason": "Earthshaker"
						}
					}
				],
				"reason": "Earthshaker"
			}
		},
		"passive_ids": [&"durability"],
	},
	&"wraith": {
		"stats": {
			"unit_id": &"wraith",
			"name": &"Wraith",
			"role": &"assassin",
			"max_hp": 220.0,
			"attack_damage": 27.0,
			"attack_range": 0.3,
			"attack_speed": 1.2,
			"move_speed": 1.0,
			"armor": 0.20,
			"magic_resist": 0.15,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 60.0,
			"mana_per_attack": 10.0,
			"ability_cd": 3,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"shadow_steps",
			"respawn_time": 0.0,
		},
		"description": "A terrifying spectral assassin that stuns its prey and moves faster than the eye can see.",
		"ability_desc": "Magic strike for 150% damage.",
		"ultimate_desc": "Magic strike for 250% magic damage and 1.5s stun.",
		"passive_desc": "Every second, enters stealth for 0.5 seconds. Stealth breaks on attack or ability cast.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.5,
							"damage_type": "magic",
							"trigger_on_hit": false,
							"reason": "Spectral Touch"
						}
					},
				],
				"reason": "Spectral Touch"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 2.5,
							"damage_type": "magic",
							"trigger_on_hit": false,
							"reason": "Phantom Strike"
						}
					},
					{
						"kind": &"stun",
						"params": {
							"duration": 1.5,
							"reason": "Phantom Strike"
						}
					}
				],
				"reason": "Phantom Strike"
			}
		},
		"passive_ids": [&"shadow_steps"],
	},
	&"warlock": {
		"stats": {
			"unit_id": &"warlock",
			"name": &"Warlock",
			"role": &"mage",
			"max_hp": 100.0,
			"attack_damage": 20.0,
			"attack_range": 3.5,
			"attack_speed": 0.65,
			"move_speed": 0.4,
			"armor": 0.05,
			"magic_resist": 0.14,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 70.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.5,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"vampirism",
			"respawn_time": 0.0,
		},
		"description": "A dark sorcerer who siphons the life force of his enemies to sustain himself and his allies.",
		"ability_desc": "Deals 100% magic damage and heals self for 10% of it.",
		"ultimate_desc": "Rifts the ground for 260% magic damage and 0.5s stun.",
		"passive_desc": "Heals for 5 HP after each auto-attack.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"projectile",
						"params": {
							"damage_ratio": 1.0,
							"damage_type": "magic",
							"reason": "Soul Siphon"
						}
					},
					{
						"kind": &"damage_based_heal",
						"params": {
							"heal_ratio": 0.1,
							"reason": "Soul Siphon"
						}
					}
				],
				"reason": "Soul Siphon"
			}
		},
		"ultimate": {
			"kind": &"projectile",
			"params": {
				"damage_ratio": 2.6,
				"damage_type": "magic",
				"stun_duration": 0.5,
				"reason": "Chaos Rift"
			}
		},
		"passive_ids": [&"vampirism"],
	},
	&"wizard": {
		"stats": {
			"unit_id": &"wizard",
			"name": &"Wizard",
			"role": &"mage",
			"max_hp": 175.0,
			"attack_damage": 21.0,
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
			"projectile_speed": 6.0,
			"projectile_radius": 0.0,
			"passive_id": &"mana_font",
			"respawn_time": 0.0,
		},
		"description": "A powerful spellcaster with a deep mana pool, capable of unleashing frequent bursts of magic damage.",
		"ability_desc": "Fires a magical projectile dealing 170% magic damage.",
		"ultimate_desc": "Calls down a meteor for 500% magic damage and deals splash damage on impact.",
		"passive_desc": "Gains 5 mana every second.",
		"ability": {
			"kind": &"projectile",
			"params": {
				"damage_ratio": 1.7,
				"damage_type": "magic",
				"speed_override": 7.0,
				"reason": "Arcane Bolt"
			}
		},
		"ultimate": {
			"kind": &"projectile",
			"params": {
				"damage_ratio": 5.0,
				"damage_type": "magic",
				"speed_override": 4.0,
				"radius_override": 0.07,
				"reason": "Meteor Bombardment"
			}
		},
		"passive_ids": [&"mana_font"],
	},
	&"monk": {
		"stats": {
			"unit_id": &"monk",
			"name": &"Monk",
			"role": &"fighter",
			"max_hp": 230.0,
			"attack_damage": 28.0,
			"attack_range": 0.3,
			"attack_speed": 1.0,
			"move_speed": 0.76,
			"armor": 0.24,
			"magic_resist": 0.24,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 70.0,
			"mana_per_attack": 10.0,
			"ability_cd": 4.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"technique",
			"respawn_time": 0.0,
		},
		"description": "A martial arts master who uses a flurry of strikes to incapacitate foes through precise pressure points.",
		"ability_desc": "Precise strike for 210% damage and 1.5s stun.",
		"ultimate_desc": "Rapid flurry for 410% damage and 2.8s stun.",
		"passive_desc": "Every 3rd attack stuns the target for 0.5s.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 2.1,
							"trigger_on_hit": false,
							"reason": "Pressure Point"
						}
					},
					{
						"kind": &"stun",
						"params": {
							"duration": 1.5,
							"reason": "Pressure Point"
						}
					}
				],
				"reason": "Pressure Point"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 4.1,
							"trigger_on_hit": false,
							"reason": "Hundred-Hand Slap"
						}
					},
					{
						"kind": &"stun",
						"params": {
							"duration": 2.8,
							"reason": "Hundred-Hand Slap"
						}
					}
				],
				"reason": "Hundred-Hand Slap"
			}
		},
		"passive_ids": [&"technique"],
	},
	&"artillery": {
		"stats": {
			"unit_id": &"artillery",
			"name": &"Artillery",
			"role": &"marksman",
			"max_hp": 150.0,
			"attack_damage": 18.0,
			"attack_range": 6.0,
			"attack_speed": 0.4,
			"move_speed": 0.2,
			"armor": 0.05,
			"magic_resist": 0.05,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 90.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.0,
			"projectile_speed": 4.0,
			"projectile_radius": 0.05,
			"passive_id": &"demolition",
			"respawn_time": 0.0,
		},
		"description": "A fragile but explosive backline siege unit that deals massive damage to anything in its sights.",
		"ability_desc": "Explosive shell dealing 150% damage and 0.5s stun.",
		"ultimate_desc": "Fires a massive artillery shell for 330% with double splash radius.",
		"passive_desc": "Attacks and abilities deal 30% splash damage in a 0.5 tile radius.",
		"ability": {
			"kind": &"projectile",
			"params": {
				"damage_ratio": 1.5,
				"stun_duration": 0.5,
				"reason": "Shell Shock"
			}
		},
		"ultimate": {
			"kind": &"projectile",
			"params": {
				"damage_ratio": 3.3,
				"radius_override": 0.1,
				"reason": "Big Bertha"
			}
		},
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
			"ability_cd": 8.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"devotion",
			"respawn_time": 0.0,
		},
		"description": "A dedicated holy healer who provides constant HP regeneration and massive burst heals for her team.",
		"ability_desc": "Heals an ally or self for 20% max HP.",
		"ultimate_desc": "Heals for 38% max HP.",
		"passive_desc": "Regenerates 2.0% of max HP every second.",
		"ability": {
			"kind": &"heal",
			"params": {
				"max_hp_ratio": 0.20,
				"reason": "Holy Mending"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"heal",
						"params": {
							"max_hp_ratio": 0.38,
							"reason": "Divine Aura"
						}
					},
				],
				"reason": "Divine Aura"
			}
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
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"siphon",
			"respawn_time": 0.0,
		},
		"description": "A captivating support who lures enemies into stuns and drains their mana with every haunting song.",
		"ability_desc": "Binds target stunning for 0.5s stun.",
		"ultimate_desc": "Shrieks for 280% magic damage and stunning for 1.0s.",
		"passive_desc": "Auto-attacks drain 5 mana from the target.",
		"ability": {
			"kind": &"stun",
			"params": {
				"duration": 0.5,
				"reason": "Enthralling Song"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 2.8,
							"damage_type": "magic",
							"trigger_on_hit": false,
							"reason": "Banshee Wail"
						}
					},
					{
						"kind": &"stun",
						"params": {
							"duration": 1.0,
							"reason": "Banshee Wail"
						}
					}
				],
				"reason": "Banshee Wail"
			}
		},
		"passive_ids": [&"siphon"],
	},
	&"valkyrie": {
		"stats": {
			"unit_id": &"valkyrie",
			"name": &"Valkyrie",
			"role": &"fighter",
			"max_hp": 280.0,
			"attack_damage": 25.0,
			"attack_range": 0.3,
			"attack_speed": 1.2,
			"move_speed": 0.7,
			"armor": 0.25,
			"magic_resist": 0.25,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 80.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"sweeping_strikes",
			"respawn_time": 0.0,
		},
		"description": "A formidable bruiser who grows more dangerous as she fights, slamming foes with her shield.",
		"ability_desc": "Bashes target for 220% damage and 0.5s stun.",
		"ultimate_desc": "War cry dealing 400% damage and 1.5s stun.",
		"passive_desc": "Deals 10 physical damage in a 0.7 radius around herself and heals for 100% of the damage dealt.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 2.2,
							"trigger_on_hit": false,
							"reason": "Shield Slam"
						}
					},
					{
						"kind": &"stun",
						"params": {
							"duration": 0.5,
							"reason": "Shield Slam"
						}
					}
				],
				"reason": "Shield Slam"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 4.0,
							"trigger_on_hit": false,
							"reason": "Valhalla Call"
						}
					},
					{
						"kind": &"stun",
						"params": {
							"duration": 1.5,
							"reason": "Valhalla Call"
						}
					}
				],
				"reason": "Valhalla Call"
			}
		},
		"passive_ids": [&"sweeping_strikes"],
	},
	&"frost_mage": {
		"stats": {
			"unit_id": &"frost_mage",
			"name": &"Frost Mage",
			"role": &"mage",
			"max_hp": 85.0,
			"attack_damage": 22.0,
			"attack_range": 3.5,
			"attack_speed": 0.95,
			"move_speed": 0.5,
			"armor": 0.10,
			"magic_resist": 0.24,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 70.0,
			"mana_per_attack": 10.0,
			"ability_cd": 4.0,
			"projectile_speed": 6.0,
			"projectile_radius": 0.0,
			"passive_id": &"frost_aura",
			"respawn_time": 0.0,
		},
		"description": "A cryomancer who controls ice and frost, slowing enemies with every attack and freezing them with powerful spells.",
		"ability_desc": "Fires an ice bolt for 180% magic damage and slows target by 40% for 2.5s.",
		"ultimate_desc": "Creates a blizzard for 320% magic damage and slows all enemies in 2.5 unit radius by 60% for 4.0s.",
		"passive_desc": "Auto-attacks slow targets by 20% for 1.5s.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.8,
							"damage_type": "magic",
							"reason": "Ice Bolt",
							"trigger_on_hit": false
						}
					},
					{
						"kind": &"slow",
						"params": {
							"slow_percentage": 0.4,
							"duration": 2.5,
							"reason": "Ice Bolt"
						}
					}
				],
				"reason": "Ice Bolt"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"target_aoe_damage",
						"params": {
							"radius": 2.5,
							"damage_ratio": 3.2,
							"damage_type": "magic",
							"reason": "Blizzard"
						}
					},
					{
						"kind": &"aoe_slow",
						"params": {
							"radius": 2.5,
							"slow_percentage": 0.6,
							"duration": 4.0,
							"reason": "Blizzard"
						}
					}
				],
				"reason": "Blizzard"
			}
		},
		"passive_ids": [&"frost_aura"],
	},
	&"earthbender": {
		"stats": {
			"unit_id": &"earthbender",
			"name": &"Earthbender",
			"role": &"fighter",
			"max_hp": 280.0,
			"attack_damage": 28.0,
			"attack_range": 0.3,
			"attack_speed": 1.1,
			"move_speed": 0.7,
			"armor": 0.35,
			"magic_resist": 0.25,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 70.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"earth_resistance",
			"respawn_time": 0.0,
		},
		"description": "A master of earth manipulation who can root enemies in place and control the battlefield.",
		"ability_desc": "Slams the ground for 180% damage and roots target for 1.8s.",
		"ultimate_desc": "Causes an earthquake for 350% damage and roots all enemies in 3.0 unit radius for 3.5s.",
		"passive_desc": "Has 15% damage reduction.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.8,
							"trigger_on_hit": false,
							"reason": "Ground Slam"
						}
					},
					{
						"kind": &"root",
						"params": {
							"duration": 1.8,
							"reason": "Ground Slam"
						}
					}
				],
				"reason": "Ground Slam"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"self_aoe_damage",
						"params": {
							"radius": 3.0,
							"damage_ratio": 3.5,
							"reason": "Earthquake"
						}
					},
					{
						"kind": &"self_aoe_root",
						"params": {
							"radius": 3.0,
							"duration": 3.5,
							"reason": "Earthquake"
						}
					}
				],
				"reason": "Earthquake"
			}
		},
		"passive_ids": [&"earth_resistance"],
	},
	&"silencer": {
		"stats": {
			"unit_id": &"silencer",
			"name": &"Silencer",
			"role": &"assassin",
			"max_hp": 200.0,
			"attack_damage": 22.0,
			"attack_range": 0.3,
			"attack_speed": 1.8,
			"move_speed": 0.85,
			"armor": 0.18,
			"magic_resist": 0.28,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 60.0,
			"mana_per_attack": 10.0,
			"ability_cd": 4.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"arcane_focus",
			"respawn_time": 0.0,
		},
		"description": "A magical assassin who specializes in silencing mages and disrupting spellcasters.",
		"ability_desc": "Strikes target for 140% damage and silences for 2.0s.",
		"ultimate_desc": "Creates a silence zone for 240% damage, silencing all enemies in 2.0 unit radius for 4.0s.",
		"passive_desc": "Deals 25% bonus damage to silenced targets.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.4,
							"trigger_on_hit": false,
							"reason": "Arcane Strike"
						}
					},
					{
						"kind": &"silence",
						"params": {
							"duration": 2.0,
							"block_abilities": true,
							"block_ultimate": true,
							"reason": "Arcane Strike"
						}
					}
				],
				"reason": "Arcane Strike"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"self_aoe_damage",
						"params": {
							"radius": 2.0,
							"damage_ratio": 2.4,
							"reason": "Silence Zone"
						}
					},
					{
						"kind": &"self_aoe_silence",
						"params": {
							"radius": 2.0,
							"duration": 4.0,
							"block_abilities": true,
							"block_ultimate": true,
							"reason": "Silence Zone"
						}
					}
				],
				"reason": "Silence Zone"
			}
		},
		"passive_ids": [&"arcane_focus"],
	},
	&"disarmer": {
		"stats": {
			"unit_id": &"disarmer",
			"name": &"Disarmer",
			"role": &"fighter",
			"max_hp": 240.0,
			"attack_damage": 26.0,
			"attack_range": 0.3,
			"attack_speed": 1.6,
			"move_speed": 0.8,
			"armor": 0.28,
			"magic_resist": 0.18,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 70.0,
			"mana_per_attack": 10.0,
			"ability_cd": 5.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"weapon_breaker",
			"respawn_time": 0.0,
		},
		"description": "A weapon specialist who can disarm enemies and prevent them from attacking.",
		"ability_desc": "Disarms target for 1.0s and deals 130% damage.",
		"ultimate_desc": "Creates a weapon suppression field that disarms all enemies in 3.0 unit radius for 2.0s.",
		"passive_desc": "Attacks against disarmed targets have 20% increased attack speed.",
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.3,
							"trigger_on_hit": false,
							"reason": "Weapon Break"
						}
					},
					{
						"kind": &"disarm",
						"params": {
							"duration": 1.0,
							"reason": "Weapon Break"
						}
					}
				],
				"reason": "Weapon Break"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"self_aoe_disarm",
						"params": {
							"radius": 3.0,
							"duration": 2.0,
							"reason": "Suppression Field"
						}
					}
				],
				"reason": "Suppression Field"
			}
		},
		"passive_ids": [&"weapon_breaker"],
	},
	&"windcaller": {
		"stats": {
			"unit_id": &"windcaller",
			"name": &"Windcaller",
			"role": &"mage",
			"max_hp": 80.0,
			"attack_damage": 14.0,
			"attack_range": 3.5,
			"attack_speed": 0.9,
			"move_speed": 0.55,
			"armor": 0.05,
			"magic_resist": 0.28,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 90.0,
			"mana_per_attack": 10.0,
			"ability_cd": 8.0,
			"projectile_speed": 7.0,
			"projectile_radius": 0.0,
			"passive_id": &"gust_protection",
			"respawn_time": 0.0,
		},
		"description": "An air elemental who controls winds to push enemies away and control battlefield positioning.",
		"ability_desc": "Blasts target with wind for 120% magic damage, knocks back 1.5 tiles, and slows by 20% for 1 second.",
		"ultimate_desc": "Creates a tornado for 300% magic damage, knocking back all enemies in a 3 tile radius by 2.5 units.",
		"passive_desc": "When knocking back enemies, gain a shield equal to 15% max hp.",
		"passive_ids": [&"gust_protection"],
		"ability": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.2,
							"damage_type": "magic",
							"trigger_on_hit": false,
							"reason": "Wind Blast"
						}
					},
					{
						"kind": &"knockback",
						"params": {
							"distance": 1.5,
							"direction": "away_from_source",
							"reason": "Wind Blast"
						}
					},
					{
						"kind": &"slow",
						"params": {
							"slow_percentage": 0.20,
							"duration": 1.0,
							"reason": "Wind Blast"
						}
					},
					{
						"kind": &"shield",
						"params": {
							"max_hp_ratio": 0.15,
							"requires_result_from": "knockback",
							"requires_field": "knockback_applied",
							"requires_value": true,
							"reason": "Gust Protection"
						}
					}
				],
				"reason": "Wind Blast"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"self_aoe_damage",
						"params": {
							"radius": 2.5,
							"damage_ratio": 3.0,
							"damage_type": "magic",
							"reason": "Tornado"
						}
					},
					{
						"kind": &"self_aoe_knockback",
						"params": {
							"radius": 2.5,
							"distance": 2.5,
							"direction": "away_from_source",
							"reason": "Tornado"
						}
					},
					{
						"kind": &"shield",
						"params": {
							"max_hp_ratio": 0.15,
							"requires_result_from": "self_aoe_knockback",
							"requires_field": "knockback_applied",
							"requires_value": true,
							"reason": "Gust Protection"
						}
					}
				],
				"reason": "Tornado"
			}
		},
	},
	&"mirror_knight": {
		"stats": {
			"unit_id": &"mirror_knight",
			"name": &"Mirror Knight",
			"role": &"tank",
			"max_hp": 320.0,
			"attack_damage": 18.0,
			"attack_range": 0.3,
			"attack_speed": 0.95,
			"move_speed": 0.6,
			"armor": 0.48,
			"magic_resist": 0.42,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 80.0,
			"mana_per_attack": 10.0,
			"ability_cd": 9.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"reflective_armor",
			"respawn_time": 0.0,
		},
		"description": "A defensive warrior who reflects damage back at attackers and protects allies with mirrored shields.",
		"ability_desc": "Gains a 25% damage reflect shield for 3.0s.",
		"ultimate_desc": "Creates a mirrored aura for 180% damage, granting 40% reflect to all allies in 2.0 unit radius for 5.0s.",
		"passive_desc": "Permanently reflects 10% of all damage taken back to attackers.",
		"ability": {
			"kind": &"reflect",
			"params": {
				"reflect_percentage": 0.25,
				"duration": 3.0,
				"reflect_type": "all",
				"reason": "Mirror Shield"
			}
		},
		"ultimate": {
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.8,
							"trigger_on_hit": false,
							"reason": "Mirror Aura"
						}
					},
					{
						"kind": &"self_aoe_reflect",
						"params": {
							"radius": 2.0,
							"reflect_percentage": 0.4,
							"duration": 5.0,
							"reflect_type": "all",
							"reason": "Mirror Aura"
						}
					}
				],
				"reason": "Mirror Aura"
			}
		},
		"passive_ids": [&"reflective_armor"],
	},
	&"mistcaller": {
		"stats": {
			"unit_id": &"mistcaller",
			"name": &"Mistcaller",
			"role": &"support",
			"max_hp": 115.0,
			"attack_damage": 15.0,
			"attack_range": 3.5,
			"attack_speed": 1.0,
			"move_speed": 0.48,
			"armor": 0.06,
			"magic_resist": 0.06,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"max_mana": 90.0,
			"mana_per_attack": 10.0,
			"ability_cd": 7.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"restorative_mist",
			"respawn_time": 0.0,
		},
		"description": "A restorative support who summons enchanted mists that gradually heal allies and sustain them through prolonged fights.",
		"ability_desc": "Heals a target for 15% of their maximum health over 5 seconds.",
		"ultimate_desc": "Heals all allies in a 3.0 unit radius for 20% of their maximum health over 5 seconds.",
		"passive_desc": "Every 5 seconds, heals all allies in a 2.0 unit radius for 10% of their missing health over 5 seconds.",
		"ability": {
			"kind": &"heal_over_time",
			"params": {
				"max_hp_ratio": 0.03,
				"duration": 5.0,
				"heal_tick_interval": 0.5,
				"stacking_mode": "refresh",
				"reason": "Healing Bloom"
			}
		},
		"ultimate": {
			"kind": &"aoe_heal_over_time",
			"params": {
				"radius": 3.0,
				"max_hp_ratio": 0.04,
				"duration": 5.0,
				"heal_tick_interval": 0.5,
				"target_self": true,
				"allow_overheal": true,
				"stacking_mode": "refresh",
				"reason": "Celestial Rain"
			}
		},
		"passive_ids": [&"restorative_mist"],
	},
}

const PASSIVE_DATA := {
	&"duelist": {
		&"post_attack": [{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "attack_damage",
				"additive": 5.0,
				"duration": 3.0,
				"max_stacks": 5,
				"target_self": true,
				"stack_behavior": "refresh",
				"reason": "Duelist",
			}
    	}],
	},
	&"eagle_eye": {
		&"on_attack": [{
			"kind": &"constant_multiplier",
			"params": {
				"multiplier": 1.25
			}
		}],
		&"post_attack": [
			{
				"kind": &"damage_threshold_trigger",
				"params": {
					"threshold_multiplier": 3.0,
					"effect": {
						"kind": &"target_aoe_damage",
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
		&"on_defense": [{
			"kind": &"constant_multiplier",
			"params": {
				"multiplier": 0.9
			}
		}],
	},
	&"executioner": {
		&"on_attack": [{
			"kind": &"hp_threshold_damage_multiplier",
			"params": {
				"below_hp_ratio": 0.3,
				"multiplier": 2.0
			}
		}],
		&"on_ability": [{
			"kind": &"hp_threshold_damage_multiplier",
			"params": {
				"below_hp_ratio": 0.3,
				"multiplier": 2.0
			}
		}],
		&"on_ultimate": [{
			"kind": &"hp_threshold_damage_multiplier",
			"params": {
				"below_hp_ratio": 0.3,
				"multiplier": 2.0
			}
		}],
	},
	&"mana_font": {
		&"on_tick": [{
			"kind": &"mana_regen",
			"params": {
				"flat_amount": 5.0
			}
		}],
	},
	&"deadeye": {
		&"on_attack": [{
			"kind": &"distance_threshold_multiplier",
			"params": {
				"min_distance": 3.0,
				"multiplier": 1.25
			}
		}],
	},
	&"bloodlust": {
		&"post_attack": [{
			"kind": &"damage_based_heal",
			"params": {
				"heal_ratio": 0.15
			}
		}],
	},
	&"rejuvenation": {
		&"on_tick": [{
			"kind": &"heal",
			"params": {
				"max_hp_ratio": 0.02,
				"on_tick_interval": 1.0,
				"reason": "Rejuvenation"
			}
		}],
	},
	&"agility": {
		&"on_defense": [{
			"kind": &"auto_dodge",
			"params": {
				"dodge_chance": 0.25,
				"on_dodge_multiplier": 0.0,
				"on_hit_multiplier": 1.0
			}
		}],
	},
	&"enlightenment": {
		&"post_attack": [{
			"kind": &"mana_restore_on_hit",
			"params": {
				"flat_amount": 5.0
			}
		}],
	},
	&"durability": {
		&"on_defense": [{
			"kind": &"constant_multiplier",
			"params": {
				"multiplier": 0.9
			}
		}],
	},
	&"swiftness": {
		&"on_attack": [{
			"kind": &"constant_multiplier",
			"params": {
				"multiplier": 1.15
			}
		}],
	},
	&"vampirism": {
		&"post_attack": [{
			"kind": &"heal",
			"params": {
				"flat_amount": 5.0,
				"reason": "Vampirism"
			}
		}],
	},
	&"technique": {
		&"post_attack": [{
			"kind": &"every_n_attacks_stun",
			"params": {
				"every_n": 3,
				"stun_duration": 0.5
			}
		}],
	},
	&"demolition": {
		&"post_attack": [{
			"kind": &"target_aoe_damage",
			"params": {
				"radius": 0.5,
				"ratio": 0.3,
				"damage_type": "physical",
				"reason": "Explosion",
				"color": [255, 100, 50]
			}
		}],
	},
	&"devotion": {
		&"on_tick": [{
			"kind": &"heal",
			"params": {
				"max_hp_ratio": 0.02,
				"on_tick_interval": 1.0,
				"reason": "Devotion"
			}
		}],
	},
	&"siphon": {
		&"post_attack": [{
			"kind": &"drain_target_mana_on_hit",
			"params": {
				"flat_amount": 5.0
			}
		}],
	},
	&"bravery": {
		&"on_attack": [{
			"kind": &"hp_threshold_damage_multiplier",
			"params": {
				"above_hp_ratio": 0.8,
				"multiplier": 1.2
			}
		}],
	},
	&"sweeping_strikes": {
		&"post_attack": [{
			"kind": &"multi",
			"params": {
				"effects": [
					{
						"kind": &"self_aoe_damage",
						"params": {
							"radius": 0.7,
							"flat_amount": 10.0,
							"damage_type": "physical",
							"reason": "Sweeping Strikes"
						}
					},
					{
						"kind": &"damage_based_heal",
						"params": {
							"heal_ratio": 1.0
						}
					}
				],
			},
		}],
	},
	&"frost_aura": {
		&"post_attack": [{
			"kind": &"slow",
			"params": {
				"slow_percentage": 0.2,
				"duration": 1.5,
				"reason": "Frost Aura"
			}
		}],
	},
	&"earth_resistance": {
		&"on_defense": [{"kind": &"constant_multiplier", "params": {"multiplier": 0.85}}],
	},
	&"arcane_focus": {
		&"on_attack": [{
			"kind": &"target_status_multiplier",
			"params": {
				"status_kind": "silence",
				"multiplier": 1.25
			}
		}],
	},
	&"weapon_breaker": {
		&"on_attack": [{
			"kind": &"target_status_multiplier",
			"params": {
				"status_kind": "disarm",
				"multiplier": 1.2
			}
		}],
	},
	# Gust Protection passive is implemented in the ability effects of Windcaller, so no hooks are needed here
	&"gust_protection": {
	},
	&"reflective_armor": {
		&"on_defense": [{
			"kind": &"reflect_damage",
			"params": {
				"reflect_percentage": 0.1,
				"reflect_type": "all"
			}
		}],
	},
	&"shadow_steps": {
		&"on_tick": [{
			"kind": &"stealth",
			"params": {
				"duration": 1.0,
				"on_tick_interval": 2.0,
				"break_conditions": {
					"on_attack": true,
					"on_ability": true,
					"on_damage_taken": false
				},
				"target_self": true,
				"reason": "Shadow Steps"
			}
		}]
	},
	&"restorative_mist": {
		&"on_tick": [{
			"kind": &"aoe_heal_over_time",
			"params": {
				"radius": 2.0,
				"on_tick_interval": 5.0,
				"missing_hp_ratio": 0.02,
				"duration": 5.0,
				"heal_tick_interval": 0.5,
				"target_self": true,
				"reason": "Restorative Mist"
			}
		}]
	},
	# Stat modifier example passives
	&"weaken": {
		&"on_attack": [{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "attack_damage",
				"multiplicative": 0.8,
				"duration": 3.0,
				"duration_type": "respawn",
				"target_self": true
			}
		}],
	},
	&"tank_boost": {
		&"on_tick": [{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "armor",
				"additive": 5.0,
				"duration": 0.0,
				"duration_type": "match",
				"target_self": true
			}
		}],
	},
	&"speed_burst": {
		&"on_attack": [{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "move_speed",
				"multiplicative": 1.2,
				"duration": 2.0,
				"duration_type": "respawn",
				"target_self": true
			}
		}],
	},
	
	# Stack-aware stat modifier examples
	&"duelist_stacking": {
		&"post_attack": [{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "attack_damage",
				"additive": 3.0,
				"duration": 8.0,
				"duration_type": "respawn",
				"max_stacks": 5,
				"stack_behavior": "refresh",
				"reason": "DuelistFury",
				"target_self": true
			}
		}],
	},
	&"accumulate_shields": {
		&"on_tick": [{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "armor",
				"additive": 2.0,
				"duration": 3.0,
				"duration_type": "respawn",
				"max_stacks": 10,
				"stack_behavior": "accumulate",
				"reason": "ShieldAccumulation",
				"target_self": true
			}
		}],
	},
	&"reset_berserk": {
		&"on_attack": [{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "attack_speed",
				"multiplicative": 1.05,
				"duration": 6.0,
				"duration_type": "respawn",
				"max_stacks": 3,
				"stack_behavior": "reset",
				"reason": "BerserkRage",
				"target_self": true
			}
		}],
	},
}

const ROLE_CONFIG_DATA := {
	&"tank": {
		"stat_mods": {},
		"passive_on_tick": null,
		"passive_post_take_damage": {
			"kind": &"post_damage_mana_gain",
			"params": {
				"damage_ratio": SimConstantsScript.TANK_MANA_GAIN_DAMAGE_RATIO
			}
		},
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
	# Use thread-local cache for multi-threading safety
	var thread_cache = _get_thread_cache()
	if not thread_cache["role_config"].is_empty():
		return thread_cache["role_config"]

	for role_id in ROLE_CONFIG_DATA:
		var data: Dictionary = ROLE_CONFIG_DATA[role_id]
		var stat_mods: Dictionary = data["stat_mods"].duplicate()
		
		var passive_on_tick = null
		if data["passive_on_tick"] != null:
			passive_on_tick = _build_effect(data["passive_on_tick"])
		
		var passive_post_take_damage = null
		if data["passive_post_take_damage"] != null:
			passive_post_take_damage = _build_effect(data["passive_post_take_damage"])
		
		thread_cache["role_config"][role_id] = RoleConfigSpecScript.new(stat_mods, passive_on_tick, passive_post_take_damage)
	
	return thread_cache["role_config"]

static func build_catalog() -> Dictionary:
	# Use thread-local cache for multi-threading safety
	var thread_cache = _get_thread_cache()
	if not thread_cache["catalog"].is_empty():
		return thread_cache["catalog"]

	# Build catalog from CHAMPION_DATA
	for unit_id in CHAMPION_DATA:
		thread_cache["catalog"][unit_id] = _build_champion(CHAMPION_DATA[unit_id])
	
	return thread_cache["catalog"]

static func build_passive_registry() -> Dictionary:
	# Use thread-local cache for multi-threading safety
	var thread_cache = _get_thread_cache()
	if not thread_cache["passive"].is_empty():
		return thread_cache["passive"]

	for passive_id in PASSIVE_DATA:
		var data: Dictionary = PASSIVE_DATA[passive_id]
		var result: Dictionary = {}
		
		for hook in data:
			var effects_data: Array = data[hook]
			var built_effects: Array = []
			for effect_data in effects_data:
				built_effects.append(_build_effect(effect_data))
			result[hook] = built_effects
		
		thread_cache["passive"][passive_id] = result
	
	return thread_cache["passive"]

static func get_passive_entry(passive_id: StringName):
	return build_passive_registry().get(passive_id, {})

static func get_champion_ids() -> Array[StringName]:
	var thread_cache = _get_thread_cache()
	if thread_cache["champion_ids"].is_empty():
		var ids: Array[StringName] = []
		for unit_id in build_catalog().keys():
			ids.append(StringName(String(unit_id)))
		thread_cache["champion_ids"] = ids
	return thread_cache["champion_ids"].duplicate()

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
