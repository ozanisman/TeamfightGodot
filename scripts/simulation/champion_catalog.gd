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
static var _native_core: Object = null

# Thread-local cache storage for safe multi-threading
static var _thread_local_caches: Dictionary = {}
static var _cache_mutex: Mutex = Mutex.new()

# One deep snapshot from the main thread; workers skip rebuilding heavy specs (read-only after freeze).
static var _frozen_worker_specs_active: bool = false
static var _frozen_catalog: Dictionary = {}
static var _frozen_role_config: Dictionary = {}
static var _frozen_passive: Dictionary = {}
static var _frozen_minion: Dictionary = {}
static var _frozen_champion_ids: Array[StringName] = []

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
			"champion_ids": [],
			"minion": {}
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
	_frozen_worker_specs_active = false
	_frozen_catalog.clear()
	_frozen_role_config.clear()
	_frozen_passive.clear()
	_frozen_minion.clear()
	_frozen_champion_ids.clear()
	_native_core = null  # Clear native core to force reinitialization
	_cache_mutex.unlock()
	
	# Clear array pool
	_pool_mutex.lock()
	_array_pool.clear()
	_pool_mutex.unlock()

static func clear_export_caches() -> void:
	# Clear caches before export to ensure fresh data
	clear_all_caches()


## Builds catalog/role/passive/minion on the caller thread once, deep-copies once; workers reuse snapshots from [method build_catalog] / [method build_role_configs] / [method build_passive_registry] / [method build_minion_catalog].[br]Must not mutate shared champion specs after freezing. [method clear_all_caches] clears snapshots.
static func freeze_built_specs_for_worker_reuse() -> void:
	var cat := build_catalog()
	var roles := build_role_configs()
	var passives := build_passive_registry()
	var minions := build_minion_catalog()
	var ids: Array[StringName] = []
	for unit_id in cat.keys():
		ids.append(StringName(String(unit_id)))
	_cache_mutex.lock()
	_frozen_catalog = cat.duplicate(true)
	_frozen_role_config = roles.duplicate(true)
	_frozen_passive = passives.duplicate(true)
	_frozen_minion = minions.duplicate(true)
	_frozen_champion_ids = ids.duplicate()
	_frozen_worker_specs_active = true
	_cache_mutex.unlock()


static func _maybe_install_frozen_specs(thread_cache: Dictionary) -> void:
	if not _frozen_worker_specs_active:
		return
	var copy_cat: Dictionary = _frozen_catalog
	var copy_roles: Dictionary = _frozen_role_config
	var copy_passive: Dictionary = _frozen_passive
	var copy_minion: Dictionary = _frozen_minion
	var copy_ids: Array[StringName] = _frozen_champion_ids
	if copy_cat.is_empty():
		return
	thread_cache["catalog"] = copy_cat
	thread_cache["role_config"] = copy_roles
	thread_cache["passive"] = copy_passive
	thread_cache["minion"] = copy_minion
	thread_cache["champion_ids"] = copy_ids.duplicate()


static func _build_effect(data: Dictionary) -> EffectSpecScript:
	# Handle empty effect dictionaries (e.g., champions without ultimates)
	if not data.has("kind"):
		return null
	
	var params: Dictionary = data.get("params", {}).duplicate() if data.has("params") else {}
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
	stats.mana_cost = data.get("mana_cost", 50.0)
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
	var stats := _build_stats(data.get("stats", {}))
	var ability := _build_effect(data.get("ability", {}))
	var ultimate := _build_effect(data.get("ultimate", {}))
	var passive_ids: Array[StringName] = []
	for id in data.get("passive_ids", []):
		passive_ids.append(id)
	
	return ChampionSpecScript.new(
		stats,
		data["description"],
		data["ability_desc"],
		data["ultimate_desc"],
		data["passive_desc"],
		data.get("passive_name", ""),
		data.get("ability_name", ""),
		data.get("ultimate_name", ""),
		ability,
		ultimate,
		passive_ids
	)

static func _build_role_config(data: Dictionary) -> RoleConfigSpecScript:
	var stat_mods: Dictionary = data.get("stat_mods", {})
	var passive_on_tick = null
	if data.get("passive_on_tick") != null:
		passive_on_tick = _build_effect(data.get("passive_on_tick", {}))
	
	var passive_post_take_damage = null
	if data.get("passive_post_take_damage") != null:
		passive_post_take_damage = _build_effect(data.get("passive_post_take_damage", {}))
	
	return RoleConfigSpecScript.new(stat_mods, passive_on_tick, passive_post_take_damage)

static func _effect(kind: StringName, params: Dictionary = {}):
	return EffectSpecScript.new(kind, params)

const CHAMPION_DATA := {
	&"swordsman": {
		"stats": {
			"unit_id": &"swordsman",
			"name": &"Swordsman",
			"role": &"fighter",
			"max_hp": 290.0,
			"attack_damage": 17.0,
			"attack_range": 0.3,
			"attack_speed": 1.2,
			"move_speed": 0.75,
			"armor": 0.20,
			"magic_resist": 0.20,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 80.0,
			"mana_per_attack": 10.0,
			"ability_cd": 5.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"duelist",
			"respawn_time": 0.0,
		},
		"description": "A skilled duelist who grows more dangerous as he fights, excelling in prolonged combats.",
		"ability_desc": "Deals 120% physical damage and applies a bleed that deals 250% physical damage as a bleed over 3s.",
		"ultimate_desc": "Deals 250% physical damage. Consumes all passive stacks to increase the damage by 50% per stack. If it kills the target, gain full passive stacks.",
		"passive_desc": "Gains 2 attack damage for 3s after each auto-attack. (Max 7 stacks)",
		"passive_name": "Duelist",
		"ability_name": "Bleeding Cut",
		"ultimate_name": "Execution",
		"ability": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.2,
							"reason": "Bleeding Cut"
						}
					},
					{
						"kind": &"damage_over_time",
						"params": {
							"damage_ratio": 2.5,
							"duration": 3.0,
							"tick_interval": 0.5,
							"stacking_mode": "separate",
							"max_stacks": 0,
							"effect_type": "generic",
							"reason": "Bleeding Cut"
						}
					}
				],
				"reason": "Bleeding Cut"
			}
		},
		"ultimate": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"consume_stacks_damage",
						"params": {
							"stat_name": "attack_damage",
							"base_damage_ratio": 2.5,
							"stack_bonus_ratio": 0.5,
							"stacking_mode": "additive",
							"damage_type": "physical",
							"stack_reason": "Duelist",
							"reason": "execution_damage"
						}
					},
					{
						"kind": &"set_stacks",
						"params": {
							"stat_name": "attack_damage",
							"to_max": true,
							"max_stacks": 7,
							"duration": 3.0,
							"additive_per_stack": 2.0,
							"requires_result_from": "consume_stacks_damage",
							"requires_field": "target_killed",
							"requires_value": true,
							"reason": "Duelist"
						}
					}
				],
				"reason": "Execution"
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
			"attack_damage": 15.0,
			"attack_range": 3.5,
			"attack_speed": 1.15,
			"move_speed": 0.80,
			"armor": 0.05,
			"magic_resist": 0.05,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 4.5,
			"projectile_speed": 8.0,
			"projectile_radius": 0.0,
			"passive_id": &"eagle_eye",
			"respawn_time": 0.0,
		},
		"description": "A swift ranged attacker who fires volleys of arrows that reduce enemy armor.",
		"ability_desc": "Fires 3 arrows at the closest enemy, each dealing 70% physical damage.",
		"ultimate_desc": "Channels for 1.5 seconds, shooting 5 arrows every 0.5s at the closest enemy dealing 40% physical damage each.",
		"passive_desc": "Each arrow reduces the target's armor by 2 for 3.0 seconds. (Max 7 stacks)",
		"passive_name": "Eagle Eye",
		"ability_name": "Volley",
		"ultimate_name": "Rain of Arrows",
		"ability": {
			"kind": &"multi_target",
			"params": {
				"target_count": 1,
				"repeat_count": 3,
				"selection_strategy": "closest",
				"team_filter": "enemy",
				"sub_effects": {
					"kind": &"projectile",
					"params": {
						"on_hit": {
							"kind": &"damage",
							"params": {
								"damage_ratio": 0.7,
								"damage_type": "physical",
								"trigger_on_hit": true,
								"reason": "Volley"
							}
						},
						"reason": "Volley",
					}
				}
			}
		},
		"ultimate": {
			"kind": &"channel",
			"params": {
				"duration": 1.5,
				"tick_interval": 0.5,
				"allow_movement": true,
				"target_mode": "fixed",
				"reason": "Rain of Arrows",
				"sub_effect": {
					"kind": &"multi_target",
					"params": {
						"target_count": 1,
						"repeat_count": 5,
						"excess_handling": "stack",
						"selection_strategy": "closest",
						"team_filter": "enemy",
						"sub_effects": {
							"kind": &"projectile",
							"params": {
								"on_hit": {
									"kind": &"damage",
									"params": {
										"damage_ratio": 0.4,
										"damage_type": "physical",
										"trigger_on_hit": true,
										"reason": "Rain of Arrows"
									}
								},
								"reason": "Rain of Arrows",
							}
						}
					}
				}
			}
		},
		"passive_ids": [&"eagle_eye"],
	},
	&"guardian": {
		"stats": {
			"unit_id": &"guardian",
			"name": &"Guardian",
			"role": &"tank",
			"max_hp": 350.0,
			"attack_damage": 14.0,
			"attack_range": 0.3,
			"attack_speed": 0.8,
			"move_speed": 0.58,
			"armor": 0.32,
			"magic_resist": 0.32,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"bastion",
			"respawn_time": 0.0,
		},
		"description": "A hulking protector who uses heavy shields to shield his allies.",
		"ability_desc": "Grants a 20% max HP shield to himself AND tand he closest ally. If alone, he gains a 40% max HP shield instead.",
		"ultimate_desc": "Slams the ground for 300% physical damage and a 3.5s stun.",
		"passive_desc": "Redirects 20% of damage taken by allies in a 2.0 tile radius.",
		"passive_name": "Bastion",
		"ability_name": "Protection",
		"ultimate_name": "Ground Slam",
		"ability": {
			"kind": &"multi_target",
			"params": {
				"target_count": 2,
				"selection_strategy": "closest",
				"team_filter": "ally",
				"include_self": true,
				"excess_handling": "stack",
				"repeat_count": 1,
				"sub_effects": {
					"kind": &"shield",
					"params": {
						"max_hp_ratio": 0.20,
						"reason": "Protection"
					}
				}
			}
		},
		"ultimate": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 3.0,
							"trigger_on_hit": false,
							"reason": "Aegis Crash"
						}
					},
					{
						"kind": &"stun",
						"params": {
							"duration": 3.5,
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
			"max_hp": 190.0,
			"attack_damage": 25.0,
			"attack_range": 0.3,
			"attack_speed": 1.2,
			"move_speed": 0.90,
			"armor": 0.11,
			"magic_resist": 0.11,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 4.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"executioner",
			"respawn_time": 0.0,
		},
		"description": "A high-mobility predator designed to dive the backline and execute wounded targets with lethal precision.",
		"ability_desc": "Dashes toward the target enemy. If reached, deal 150% physical damage and stun for 0.5s.",
		"ultimate_desc": "Executes a target with 900% physical damage.",
		"passive_desc": "Deals double damage to targets below 30% HP.",
		"passive_name": "Executioner",
		"ability_name": "Shadow Dash",
		"ultimate_name": "Execute",
		"ability": {
			"kind": &"multi_effect",
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
				"reason": "Assassinate",
				"on_hit": {
					"kind": &"damage",
					"params": {
						"damage_ratio": 9.0,
						"damage_type": "physical",
						"trigger_on_hit": true,
						"reason": "Assassinate"
					}
				}
			}
		},
		"passive_ids": [&"executioner"],
	},
	&"sniper": {
		"stats": {
			"unit_id": &"sniper",
			"name": &"Sniper",
			"role": &"marksman",
			"max_hp": 140.0,
			"attack_damage": 17.0,
			"attack_range": 4.5,
			"attack_speed": 0.7,
			"move_speed": 0.4,
			"armor": 0.05,
			"magic_resist": 0.05,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.0,
			"projectile_speed": 10.0,
			"projectile_radius": 0.0,
			"passive_id": &"deadeye",
			"respawn_time": 0.0,
		},
		"description": "An elite marksman specializing in devastating single-target shots.",
		"ability_desc": "High-precision strike for 150% physical damage. [MAKE MORE INTERESTING]",
		"ultimate_desc": "Lethal long-range shot for 350% physical damage. [MAKE MORE INTERESTING]",
		"passive_desc": "Deals 30% bonus damage to targets at least 2.0 tiles away.",
		"passive_name": "Deadeye",
		"ability_name": "Headshot",
		"ultimate_name": "Snipe",
		"ability": {
			"kind": &"projectile",
			"params": {
				"reason": "Headshot",
				"on_hit": {
					"kind": &"damage",
					"params": {
						"damage_ratio": 1.5,
						"damage_type": "physical",
						"trigger_on_hit": true,
						"reason": "Headshot"
					}
				}
			}
		},
		"ultimate": {
			"kind": &"projectile",
			"params": {
				"speed_override": 15.0,
				"reason": "Snipe",
				"on_hit": {
					"kind": &"damage",
					"params": {
						"damage_ratio": 3.5,
						"damage_type": "physical",
						"trigger_on_hit": true,
						"reason": "Snipe"
					}
				}
			}
		},
		"passive_ids": [&"deadeye"],
	},
	&"berserker": {
		"stats": {
			"unit_id": &"berserker",
			"name": &"Berserker",
			"role": &"fighter",
			"max_hp": 270.0,
			"attack_damage": 23.0,
			"attack_range": 0.3,
			"attack_speed": 1.15,
			"move_speed": 0.80,
			"armor": 0.18,
			"magic_resist": 0.15,
			"tenacity": 0.0,
			"life_steal": 0.10,
			"mana_cost": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 5.5,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"bloodlust",
			"respawn_time": 0.0,
		},
		"description": "A savage warrior who thrives in the heat of battle, healing himself through sheer aggression.",
		"ability_desc": "Damages himself for 20% current HP, gains a 40% max HP shield, and gains 25% attack speed for 2.5s.",
		"ultimate_desc": "Unleashes a devastating strike for 400% true damage that applies lifesteal.",
		"passive_desc": "Has innate 10% lifesteal and permanent gains +5% lifesteal per takedown.",
		"passive_name": "Bloodlust",
		"ability_name": "Blood Sacrifice",
		"ultimate_name": "Ravage",
		"ability": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"current_hp_ratio": 0.20,
							"damage_type": "true",
							"target_self": true,
							"reason": "Blood Price"
						}
					},
					{
						"kind": &"shield",
						"params": {
							"max_hp_ratio": 0.40,
							"target_self": true,
							"reason": "Blood Price"
						}
					},
					{
						"kind": &"stat_modifier",
						"params": {
							"stat_name": "attack_speed",
							"multiplicative": 1.25,
							"duration": 2.5,
							"duration_type": "respawn",
							"target_self": true,
							"reason": "Blood Price",
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
				"damage_ratio": 4.0,
				"damage_type": "true",
				"trigger_on_hit": true,
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
			"max_hp": 350.0,
			"attack_damage": 16.0,
			"attack_range": 0.3,
			"attack_speed": 0.70,
			"move_speed": 0.7,
			"armor": 0.35,
			"magic_resist": 0.45,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 90.0,
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
		"passive_name": "Rejuvenation",
		"ability_name": "Lay on Hands",
		"ultimate_name": "Divine Judgment",
		"ability": {
			"kind": &"heal",
			"params": {
				"max_hp_ratio": 0.20,
				"reason": "Lay on Hands"
			}
		},
		"ultimate": {
			"kind": &"multi_effect",
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
			"max_hp": 210.0,
			"attack_damage": 27.0,
			"attack_range": 0.3,
			"attack_speed": 1.5,
			"move_speed": 0.98,
			"armor": 0.10,
			"magic_resist": 0.10,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 90.0,
			"mana_per_attack": 10.0,
			"ability_cd": 4.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"agility",
			"respawn_time": 0.0,
		},
		"description": "A nimble backline specialist who strikes with precision and vanishes before being targeted.",
		"ability_desc": "Deals 200% physical damage. [MAKE MORE INTERESTING]",
		"ultimate_desc": "Lethal execution dealing 600% physical damage. [MAKE MORE INTERESTING]",
		"passive_desc": "25% chance to dodge incoming auto attacks.",
		"passive_name": "Agility",
		"ability_name": "Backstab",
		"ultimate_name": "Execute",
		"ability": {
			"kind": &"multi_effect",
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
			"max_hp": 185.0,
			"attack_damage": 14.0,
			"attack_range": 3.5,
			"attack_speed": 0.75,
			"move_speed": 0.45,
			"armor": 0.05,
			"magic_resist": 0.15,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 100.0,
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
		"passive_name": "Enlightenment",
		"ability_name": "Purify",
		"ultimate_name": "Divine Intervention",
		"ability": {
			"kind": &"shield",
			"params": {
				"max_hp_ratio": 0.15,
				"reason": "Purify"
			}
		},
		"ultimate": {
			"kind": &"multi_effect",
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
			"max_hp": 360.0,
			"attack_damage": 13.0,
			"attack_range": 0.3,
			"attack_speed": 0.75,
			"move_speed": 0.66,
			"armor": 0.45,
			"magic_resist": 0.40,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 5.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.05,
			"passive_id": &"durability",
			"respawn_time": 0.0,
		},
		"description": "A mountain of a tank with unmatched physical resistance and the ability to stun the entire battlefield.",
		"ability_desc": "Taunts all targets in a 2.5 tile radius for 2.5s and deals 130% physical damage in a 1.0 tile radius.",
		"ultimate_desc": "Colossal impact in a 2.5 tile radius for 250% physical damage and a 2.0s stun.",
		"passive_desc": "Reduces incoming damage by 10%. (Doesn't affect true damage)",
		"passive_name": "Durability",
		"ability_name": "Taunt",
		"ultimate_name": "Colossal Impact",
		"ability": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"aoe_taunt",
						"params": {
							"shape": "circle",
							"anchor": "self",
							"radius": 2.5,
							"duration": 2.5,
							"reason": "Seismic Slam"
						}
					},
					{
						"kind": &"aoe_damage",
						"params": {
							"shape": "circle",
							"anchor": "self",
							"radius": 1.0,
							"damage_ratio": 1.3,
							"damage_type": "physical",
							"reason": "Seismic Slam"
						}
					}
				],
				"reason": "Seismic Slam"
			}
		},
		"ultimate": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"aoe_damage",
						"params": {
							"shape": "circle",
							"anchor": "self",
							"radius": 2.5,
							"damage_ratio": 2.5,
							"damage_type": "physical",
							"reason": "Earthshaker"
						}
					},
					{
						"kind": &"aoe_stun",
						"params": {
							"shape": "circle",
							"anchor": "self",
							"radius": 2.5,
							"duration": 2.0,
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
			"max_hp": 210.0,
			"attack_damage": 21.0,
			"attack_range": 0.3,
			"attack_speed": 0.85,
			"move_speed": 0.9,
			"armor": 0.10,
			"magic_resist": 0.10,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 80.0,
			"mana_per_attack": 10.0,
			"ability_cd": 4.5,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"shadow_steps",
			"respawn_time": 0.0,
		},
		"description": "A terrifying spectral assassin that stuns its prey and moves faster than the eye can see.",
		"ability_desc": "Deals 150% magic damage and a 1.0s disarm.",
		"ultimate_desc": "Deals 250% magic damage and 1.5s stun. [MAKE MORE INTERESTING]",
		"passive_desc": "Every second, enters stealth for 0.5s.",
		"passive_name": "Shadow Steps",
		"ability_name": "Spectral Strike",
		"ultimate_name": "Haunt",
		"ability": {
			"kind": &"multi_effect",
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
					{
						"kind": &"disarm",
						"params": {
							"duration": 1.0,
							"reason": "Spectral Touch"
						}
					},
				],
				"reason": "Spectral Touch"
			}
		},
		"ultimate": {
			"kind": &"multi_effect",
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
			"max_hp": 250.0,
			"attack_damage": 15.0,
			"attack_range": 0.3,
			"attack_speed": 0.70,
			"move_speed": 0.70,
			"armor": 0.15,
			"magic_resist": 0.15,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 30.0,
			"mana_per_attack": 10.0,
			"ability_cd": 2.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"soul_feast",
			"respawn_time": 0.0,
		},
		"description": "A melee sorcerer who siphons the life force of his enemies to sustain himself.",
		"ability_desc": "Channels for 3s. Every 0.5s deals 50% magic damage in a 1.5 tile radius and heals for 25% of damage dealt. At the end, explodes in a 3.0 tile radius for 50% of the total damage dealt and heals for the same amount.",
		"ultimate_desc": "[NEED TO ADD AN ULTIMATE]",
		"passive_desc": "Gains permanent max HP equal to 50% of self healing.",
		"passive_name": "Soul Feast",
		"ability_name": "Chaos Rift",
		"ultimate_name": "",
		"ability": {
			"kind": &"channel",
			"params": {
				"duration": 3.0,
				"tick_interval": 0.5,
				"allow_movement": false,
				"target_mode": "fixed",
				"reason": "Chaos Rift",
				"sub_effect": {
					"kind": &"multi_effect",
					"params": {
						"effects": [
							{
								"kind": &"aoe_damage",
								"params": {
									"radius": 1.5,
									"damage_ratio": 0.50,
									"damage_type": "magic",
									"reason": "Chaos Rift"
								}
							},
							{
								"kind": &"damage_based_heal",
								"params": {
									"damage_ratio": 0.25,
									"use_accumulated_damage": false,
									"reason": "Chaos Rift"
								}
							}
						],
						"reason": "Chaos Rift"
					}
				},
				"post_complete_effect": {
					"kind": &"multi_effect",
					"params": {
						"effects": [
							{
								"kind": &"aoe_damage",
								"params": {
									"radius": 3.0,
									"damage_ratio": 0.5,
									"use_accumulated_damage": true,
									"damage_type": "magic",
									"reason": "Chaos Rift"
								}
							},
							{
								"kind": &"damage_based_heal",
								"params": {
									"damage_ratio": 1.0,
									"use_accumulated_damage": true,
									"reason": "Chaos Rift"
								}
							}
						],
						"reason": "Chaos Rift"
					}
				},
				"post_interrupt_effect": {
					"kind": &"multi_effect",
					"params": {
						"effects": [
							{
								"kind": &"aoe_damage",
								"params": {
									"radius": 3.0,
									"damage_ratio": 1.0,
									"use_accumulated_damage": true,
									"damage_type": "magic",
									"reason": "Chaos Rift"
								}
							},
							{
								"kind": &"damage_based_heal",
								"params": {
									"damage_ratio": 1.0,
									"use_accumulated_damage": true,
									"reason": "Chaos Rift"
								}
							}
						],
						"reason": "Chaos Rift"
					}
				}
			}
		},
		"ultimate": {
			
		},
		"passive_ids": [&"soul_feast"],
	},
	&"wizard": {
		"stats": {
			"unit_id": &"wizard",
			"name": &"Wizard",
			"role": &"mage",
			"max_hp": 160.0,
			"attack_damage": 20.0,
			"attack_range": 3.5,
			"attack_speed": 0.92,
			"move_speed": 0.5,
			"armor": 0.05,
			"magic_resist": 0.17,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 8.2,
			"projectile_speed": 6.0,
			"projectile_radius": 0.0,
			"passive_id": &"mana_font",
			"respawn_time": 0.0,
		},
		"description": "A powerful spellcaster with a deep mana pool, capable of unleashing frequent bursts of magic damage.",
		"ability_desc": "Fires a magical projectile dealing 150% magic damage.",
		"ultimate_desc": "Calls down a meteor for 500% magic damage.",
		"passive_desc": "Gains 5 mana every second.",
		"passive_name": "Mana Font",
		"ability_name": "Arcane Bolt",
		"ultimate_name": "Meteor Bombardment",
		"ability": {
			"kind": &"projectile",
			"params": {
				"speed_override": 7.0,
				"reason": "Arcane Bolt",
				"on_hit": {
					"kind": &"damage",
					"params": {
						"damage_ratio": 1.5,
						"damage_type": "magic",
						"trigger_on_hit": true,
						"reason": "Arcane Bolt"
					}
				}
			}
		},
		"ultimate": {
			"kind": &"projectile",
			"params": {
				"speed_override": 4.0,
				"reason": "Meteor Bombardment",
				"on_hit": {
					"kind": &"damage",
					"params": {
						"damage_ratio": 5.0,
						"damage_type": "magic",
						"trigger_on_hit": true,
						"reason": "Meteor Bombardment"
					}
				}
			}
		},
		"passive_ids": [&"mana_font"],
	},
	&"monk": {
		"stats": {
			"unit_id": &"monk",
			"name": &"Monk",
			"role": &"fighter",
			"max_hp": 300.0,
			"attack_damage": 25.0,
			"attack_range": 0.3,
			"attack_speed": 1.1,
			"move_speed": 0.76,
			"armor": 0.22,
			"magic_resist": 0.22,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 70.0,
			"mana_per_attack": 10.0,
			"ability_cd": 4.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"stunning_strikes",
			"respawn_time": 0.0,
		},
		"description": "A martial arts master who uses a flurry of strikes to incapacitate foes through precise pressure points.",
		"ability_desc": "Precise strike for 160% physical damage that silences the target for 1.0s.",
		"ultimate_desc": "Gains 25% max HP shield and reflects 75% of incoming damage for 5s.",
		"passive_desc": "Every 3rd attack stuns the target for 0.5s.",
		"passive_name": "Stunning Strikes",
		"ability_name": "Pressure Point",
		"ultimate_name": "Drunken Stance",
		"ability": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.6,
							"trigger_on_hit": false,
							"reason": "Pressure Point"
						}
					},
					{
						"kind": &"silence",
						"params": {
							"duration": 1.0,
							"reason": "Pressure Point"
						}
					}
				],
				"reason": "Pressure Point"
			}
		},
		"ultimate": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"shield",
						"params": {
							"max_hp_ratio": 0.25,
							"reason": "Drunken Stance"
						}
					},
					{
						"kind": &"reflect",
						"params": {
							"reflect_percentage": 0.50,
							"duration": 5.0,
							"reflect_type": "all",
							"reason": "Drunken Stance"
						}
					}
				],
				"reason": "Drunken Stance"
			}
		},
		"passive_ids": [&"stunning_strikes"],
	},
	&"artillery": {
		"stats": {
			"unit_id": &"artillery",
			"name": &"Artillery",
			"role": &"marksman",
			"max_hp": 140.0,
			"attack_damage": 17.0,
			"attack_range": 6.0,
			"attack_speed": 0.4,
			"move_speed": 0.2,
			"armor": 0.05,
			"magic_resist": 0.05,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 90.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.0,
			"projectile_speed": 4.0,
			"projectile_radius": 0.05,
			"passive_id": &"demolition",
			"respawn_time": 0.0,
		},
		"description": "A fragile slow backline siege unit that deals explosive damage to anything in its sights.",
		"ability_desc": "Explosive shell that deals 150% physical  damage, stuns for 0.5s, and knocks the target 1.0 tiles away.",
		"ultimate_desc": "Fires a massive artillery shell for 330% with double splash radius.",
		"passive_desc": "Attacks and abilities deal additional 30% physical damage as splash in a 0.5 tile radius.",
		"passive_name": "Demolition",
		"ability_name": "Explosive Shell",
		"ultimate_name": "Barrage",
		"ability": {
			"kind": &"projectile",
			"params": {
				"reason": "Shell Shock",
				"on_hit": {
					"kind": &"multi_effect",
					"params": {
						"effects": [
							{
								"kind": &"damage",
								"params": {
									"damage_ratio": 1.5,
									"damage_type": "physical",
									"trigger_on_hit": true,
									"reason": "Shell Shock"
								}
							},
							{
								"kind": &"stun",
								"params": {
									"duration": 0.5,
									"reason": "Shell Shock"
								}
							},
							{
								"kind": &"knockback",
								"params": {
									"distance": 1.0,
									"direction": "away_from_source",
									"reason": "Shell Shock"
								}
							},
						],
						"reason": "Shell Shock"
					}
				}
			}
		},
		"ultimate": {
			"kind": &"projectile",
			"params": {
				"radius_override": 0.1,
				"reason": "Big Bertha",
				"on_hit": {
					"kind": &"damage",
					"params": {
						"damage_ratio": 3.3,
						"damage_type": "physical",
						"trigger_on_hit": true,
						"reason": "Big Bertha"
					}
				}
			}
		},
		"passive_ids": [&"demolition"],
	},
	&"cleric": {
		"stats": {
			"unit_id": &"cleric",
			"name": &"Cleric",
			"role": &"support",
			"max_hp": 130.0,
			"attack_damage": 0.0,
			"attack_range": 3.5,
			"attack_speed": 0.0,
			"move_speed": 0.70,
			"armor": 0.07,
			"magic_resist": 0.10,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 1.3,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"devotion",
			"respawn_time": 0.0,
		},
		"description": "A dedicated holy healer who constantly heals her allies instead of attacking enemies.",
		"ability_desc": "Heals an ally for 20 HP.",
		"ultimate_desc": "Heals all allies for 25% missing HP.",
		"passive_desc": "Can't attack, but gains 10 mana per second passively.",
		"passive_name": "Devotion",
		"ability_name": "Heal",
		"ultimate_name": "Mass Restoration",
		"ability": {
			"kind": &"heal",
			"params": {
				"flat_amount": 20.0,
				"reason": "Holy Mending"
			},
			"requires_target_in_range": false
		},
		"ultimate": {
			"kind": &"multi_target",
			"params": {
				"target_count": -1,  # -1 means all targets
				"team_filter": "ally",
				"include_self": true,
				"sub_effects": {
					"kind": &"heal",
					"params": {
						"missing_hp_ratio": 0.25,
						"reason": "Mass Restoration"
					}
				},
			}
		},
		"passive_ids": [&"devotion"],
	},
	&"siren": {
		"stats": {
			"unit_id": &"siren",
			"name": &"Siren",
			"role": &"support",
			"max_hp": 150.0,
			"attack_damage": 13.0,
			"attack_range": 3.5,
			"attack_speed": 1.15,
			"move_speed": 0.48,
			"armor": 0.04,
			"magic_resist": 0.13,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 80.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"siphon",
			"respawn_time": 0.0,
		},
		"description": "A captivating support who lures enemies into stuns and drains their mana with every haunting song.",
		"ability_desc": "Binds target stunning for 0.5s stun.",
		"ultimate_desc": "Shrieks for 280% magic damage and stunning for 1.0s.",
		"passive_desc": "Auto-attacks drain 5 mana from the target.",
		"passive_name": "Siphon",
		"ability_name": "Enthralling Song",
		"ultimate_name": "Siren's Shriek",
		"ability": {
			"kind": &"stun",
			"params": {
				"duration": 0.5,
				"reason": "Enthralling Song"
			}
		},
		"ultimate": {
			"kind": &"multi_effect",
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
			"max_hp": 270.0,
			"attack_damage": 18.0,
			"attack_range": 0.3,
			"attack_speed": 1.0,
			"move_speed": 0.75,
			"armor": 0.17,
			"magic_resist": 0.17,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"sweeping_strikes",
			"respawn_time": 0.0,
		},
		"description": "A formidable bruiser who wildly slashes through foes with her massive axe.",
		"ability_desc": "Cleaves enemies in a 150 degree cone within 0.75 tiles for 150% physical damage and a 0.5s stun.",
		"ultimate_desc": "Hits enemies in a 2.0 tile radius for 15% max HP physical damage and shields for 200% of the damage dealt.",
		"passive_desc": "Auto attacks deal an additional 7 physical damage in a 0.75 tile radius and heal for 100% of the damage dealt.",
		"passive_name": "Sweeping Strikes",
		"ability_name": "Cleave",
		"ultimate_name": "Whirlwind",
		"ability": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"aoe_damage",
						"params": {
							"damage_ratio": 1.5,
							"shape": &"cone",
							"anchor": "forward",
							"radius": 0.75,
							"width": 150.0,
							"reason": "Cleave"
						}
					},
					{
						"kind": &"aoe_stun",
						"params": {
							"duration": 0.5,
							"shape": &"cone",
							"anchor": "forward",
							"radius": 0.75,
							"width": 150.0,
							"reason": "Cleave"
						}
					}
				],
				"reason": "Cleave"
			}
		},
		"ultimate": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"aoe_damage",
						"params": {
							"max_hp_ratio": 0.15,
							"shape": &"circle",
							"anchor": "self",
							"radius": 2.0,
							"reason": "Whirlwind"
						}
					},
					{
						"kind": &"damage_based_shield",
						"params": {
							"damage_ratio": 2.0,
							"reason": "Whirlwind"
						}
					}
				],
				"reason": "Whirlwind"
			}
		},
		"passive_ids": [&"sweeping_strikes"],
	},
	&"frost_mage": {
		"stats": {
			"unit_id": &"frost_mage",
			"name": &"Frost Mage",
			"role": &"mage",
			"max_hp": 140.0,
			"attack_damage": 18.0,
			"attack_range": 3.5,
			"attack_speed": 0.80,
			"move_speed": 0.5,
			"armor": 0.10,
			"magic_resist": 0.15,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 80.0,
			"mana_per_attack": 10.0,
			"ability_cd": 5.0,
			"projectile_speed": 6.0,
			"projectile_radius": 0.0,
			"passive_id": &"bitter_chill",
			"respawn_time": 0.0,
		},
		"description": "A cryomancer who controls ice and frost, slowing enemies with every attack and freezing them with powerful spells.",
		"ability_desc": "Fires an ice bolt for 150% magic damage and slows target by 30% for 2.5s.",
		"ultimate_desc": "Creates a blizzard for 320% magic damage and slows all enemies in 2.5 tile radius by 50% for 4.0s.",
		"passive_desc": "Auto-attacks slow the target by 10% for 1.5s.",
		"passive_name": "Bitter Chill",
		"ability_name": "Ice Bolt",
		"ultimate_name": "Blizzard",
		"ability": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.5,
							"damage_type": "magic",
							"reason": "Ice Bolt",
							"trigger_on_hit": false
						}
					},
					{
						"kind": &"slow",
						"params": {
							"slow_percentage": 0.3,
							"duration": 2.5,
							"reason": "Ice Bolt"
						}
					}
				],
				"reason": "Ice Bolt"
			}
		},
		"ultimate": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"aoe_damage",
						"params": {
							"shape": "circle",
							"anchor": "target",
							"radius": 2.5,
							"damage_ratio": 3.2,
							"damage_type": "magic",
							"reason": "Blizzard"
						}
					},
					{
						"kind": &"aoe_slow",
						"params": {
							"shape": "circle",
							"anchor": "target",
							"radius": 2.5,
							"slow_percentage": 0.5,
							"duration": 4.0,
							"reason": "Blizzard"
						}
					}
				],
				"reason": "Blizzard"
			}
		},
		"passive_ids": [&"bitter_chill"],
	},
	&"earthbender": {
		"stats": {
			"unit_id": &"earthbender",
			"name": &"Earthbender",
			"role": &"tank",
			"max_hp": 350.0,
			"attack_damage": 12.0,
			"attack_range": 0.3,
			"attack_speed": 0.80,
			"move_speed": 0.70,
			"armor": 0.15,
			"magic_resist": 0.15,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 70.0,
			"mana_per_attack": 10.0,
			"ability_cd": 5.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"earthen_protection",
			"respawn_time": 0.0,
		},
		"description": "A master of earth manipulation who builds up his durability during combat and roots enemies in place to control the battlefield.",
		"ability_desc": "Creates a tree tendril that roots the target for 2.0s and deals 130% magic damage.",
		"ultimate_desc": "Causes an earthquake in 3.0 tile radius that roots enemies for 3.5s and deals 200% magic damage.",
		"passive_desc": "When taking damage, gain a stack of 5% armor and magic resist for 5.0s. (Max 10 stacks)",
		"passive_name": "Earthen Protection",
		"ability_name": "Tendril Grasp",
		"ultimate_name": "Earthquake",
		"ability": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.3,
							"damage_type": "magic",
							"trigger_on_hit": false,
							"reason": "Tendril Grasp"
						}
					},
					{
						"kind": &"root",
						"params": {
							"duration": 2.0,
							"reason": "Tendril Grasp"
						}
					}
				],
				"reason": "Tendril Grasp"
			}
		},
		"ultimate": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"aoe_damage",
						"params": {
							"shape": "circle",
							"anchor": "self",
							"radius": 3.0,
							"damage_ratio": 2.0,
							"damage_type": "magic",
							"reason": "Earthquake"
						}
					},
					{
						"kind": &"aoe_root",
						"params": {
							"shape": "circle",
							"anchor": "self",
							"radius": 3.0,
							"duration": 3.5,
							"reason": "Earthquake"
						}
					}
				],
				"reason": "Earthquake"
			}
		},
		"passive_ids": [&"earthen_protection"],
	},
	&"silencer": {
		"stats": {
			"unit_id": &"silencer",
			"name": &"Silencer",
			"role": &"assassin",
			"max_hp": 210.0,
			"attack_damage": 24.0,
			"attack_range": 0.3,
			"attack_speed": 1.3,
			"move_speed": 0.85,
			"armor": 0.17,
			"magic_resist": 0.13,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 70.0,
			"mana_per_attack": 10.0,
			"ability_cd": 4.5,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"arcane_focus",
			"respawn_time": 0.0,
		},
		"description": "A magical assassin who specializes in silencing mages and disrupting spellcasters.",
		"ability_desc": "Strikes target for 170% physical damage and silences for 2.0s.",
		"ultimate_desc": "Creates a silence zone for 340% physical damage, silencing all enemies in 3.0 tile radius for 4.0s.",
		"passive_desc": "Deals 25% bonus damage to silenced targets.",
		"passive_name": "Arcane Focus",
		"ability_name": "Arcane Strike",
		"ultimate_name": "Silence Zone",
		"ability": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.7,
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
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"aoe_damage",
						"params": {
							"shape": "circle",
							"anchor": "self",
							"radius": 3.0,
							"damage_ratio": 3.4,
							"reason": "Silence Zone"
						}
					},
					{
						"kind": &"aoe_silence",
						"params": {
							"shape": "circle",
							"anchor": "self",
							"radius": 3.0,
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
			"max_hp": 280.0,
			"attack_damage": 23.0,
			"attack_range": 0.3,
			"attack_speed": 1.20,
			"move_speed": 0.8,
			"armor": 0.22,
			"magic_resist": 0.18,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 5.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"weapon_breaker",
			"respawn_time": 0.0,
		},
		"description": "A weapon specialist who can disarm enemies and prevent them from attacking.",
		"ability_desc": "Disarms target for 1.5s and deals 150% physical damage.",
		"ultimate_desc": "Creates a weapon suppression field that disarms all enemies in 3.0 tile radius for 3.0s.",
		"passive_desc": "Attacking a disarmed target grants 25% increased attack speed for 3 seconds.",
		"passive_name": "Weapon Breaker",
		"ability_name": "Disarm",
		"ultimate_name": "Weapon Suppression",
		"ability": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"damage",
						"params": {
							"damage_ratio": 1.5,
							"trigger_on_hit": false,
							"reason": "Weapon Break"
						}
					},
					{
						"kind": &"disarm",
						"params": {
							"duration": 1.5,
							"reason": "Weapon Break"
						}
					}
				],
				"reason": "Weapon Break"
			}
		},
		"ultimate": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"aoe_disarm",
						"params": {
							"shape": "circle",
							"anchor": "self",
							"radius": 3.0,
							"duration": 3.0,
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
			"max_hp": 170.0,
			"attack_damage": 18.0,
			"attack_range": 3.5,
			"attack_speed": 0.9,
			"move_speed": 0.60,
			"armor": 0.10,
			"magic_resist": 0.18,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 7.0,
			"projectile_speed": 7.0,
			"projectile_radius": 0.0,
			"passive_id": &"gust_protection",
			"respawn_time": 0.0,
		},
		"description": "An air elemental who controls winds to push enemies away and control battlefield positioning.",
		"ability_desc": "Blasts a target with wind for 120% magic damage, knocking them back 1.0 tiles and slowing them by 20% for 1.5s.",
		"ultimate_desc": "Creates a tornado for 250% magic damage, knocking back all enemies in a 3.5 tile radius by 2.5 tiles.",
		"passive_desc": "Gaisn a 10% max hp shield after knocking back enemies.",
		"passive_name": "Gust Protection",
		"ability_name": "Wind Blast",
		"ultimate_name": "Tornado",
		"passive_ids": [&"gust_protection"],
		"ability": {
			"kind": &"multi_effect",
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
							"distance": 1.0,
							"direction": "away_from_source",
							"reason": "Wind Blast"
						}
					},
					{
						"kind": &"slow",
						"params": {
							"slow_percentage": 0.20,
							"duration": 1.5,
							"reason": "Wind Blast"
						}
					},
					{
						"kind": &"shield",
						"params": {
							"max_hp_ratio": 0.10,
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
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"aoe_damage",
						"params": {
							"shape": "circle",
							"anchor": "self",
							"radius": 3.5,
							"damage_ratio": 2.5,
							"damage_type": "magic",
							"reason": "Tornado"
						}
					},
					{
						"kind": &"aoe_knockback",
						"params": {
							"shape": "circle",
							"anchor": "self",
							"radius": 3.5,
							"distance": 2.5,
							"direction": "away_from_source",
							"reason": "Tornado"
						}
					},
					{
						"kind": &"shield",
						"params": {
							"max_hp_ratio": 0.10,
							"requires_result_from": "aoe_knockback",
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
			"max_hp": 350.0,
			"attack_damage": 14.0,
			"attack_range": 0.3,
			"attack_speed": 0.85,
			"move_speed": 0.6,
			"armor": 0.40,
			"magic_resist": 0.35,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 80.0,
			"mana_per_attack": 10.0,
			"ability_cd": 5.5,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"reflective_armor",
			"respawn_time": 0.0,
		},
		"description": "A defensive warrior who reflects incoming damage back at attackers.",
		"ability_desc": "Gains a 10% max HP shield and 25% reflect for 2.5s.",
		"ultimate_desc": "Grants 50% reflect to all allies in 4.0 tile radius for 5.0s.",
		"passive_desc": "Reflects 10% of all damage taken back to attackers.",
		"passive_name": "Reflective Armor",
		"ability_name": "Mirror Shield",
		"ultimate_name": "Mirrored Dimension",
		"ability": {
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"shield",
						"params": {
							"max_hp_ratio": 0.30,
							"target_self": true,
							"reason": "Mirror Shield"
						}
					},
					{
						"kind": &"reflect",
						"params": {
							"reflect_percentage": 0.25,
							"duration": 2.5,
							"reflect_type": "all",
							"reason": "Mirror Shield"
						}
					}
				],
				"reason": "Mirror Shield"
			}
		},
		"ultimate": {
			"kind": &"aoe_reflect",
			"params": {
				"shape": "circle",
				"anchor": "self",
				"radius": 4.0,
				"reflect_percentage": 0.50,
				"duration": 5.0,
				"reflect_type": "all",
				"reason": "Mirror Dimension"
			}
		},
		"passive_ids": [&"reflective_armor"],
	},
	&"mistcaller": {
		"stats": {
			"unit_id": &"mistcaller",
			"name": &"Mistcaller",
			"role": &"support",
			"max_hp": 150.0,
			"attack_damage": 13.0,
			"attack_range": 3.0,
			"attack_speed": 0.8,
			"move_speed": 0.48,
			"armor": 0.06,
			"magic_resist": 0.06,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 100.0,
			"mana_per_attack": 10.0,
			"ability_cd": 8.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"restorative_mist",
			"respawn_time": 0.0,
		},
		"description": "A restorative support who healls allies with enchanted mists through prolonged fights.",
		"ability_desc": "Heals a target for 15% of their maximum HP over 3s.",
		"ultimate_desc": "Heals all allies in a 3.5 tile radius for 25% of their maximum HP over 3s. Excess healing is converted into temporary maximum HP.",
		"passive_desc": "All of Mistcaller's excess healing is converted to temporary maximum HP. Every 5s, heals all allies in a 3.0 tile radius for 5 + 5% missing HP over 3s.",
		"passive_name": "Restorative Mist",
		"ability_name": "Healing Bloom",
		"ultimate_name": "Enveloping Mist",
		"ability": {
			"kind": &"heal_over_time",
			"params": {
				"max_hp_ratio": 0.15,
				"duration": 3.0,
				"tick_interval": 0.2,
				"allow_overheal": true,
				"stacking_mode": "separate",
				"max_stacks": 0,
				"reason": "Healing Bloom"
			}
		},
		"ultimate": {
			"kind": &"aoe_heal_over_time",
			"params": {
				"shape": "circle",
				"anchor": "self",
				"radius": 3.5,
				"max_hp_ratio": 0.25,
				"duration": 3.0,
				"tick_interval": 0.2,
				"target_self": true,
				"allow_overheal": true,
				"stacking_mode": "separate",
				"max_stacks": 0,
				"reason": "Celestial Rain"
			}
		},
		"passive_ids": [&"restorative_mist"],
	},
	&"necromancer": {
		"stats": {
			"unit_id": &"necromancer",
			"name": &"Necromancer",
			"role": &"mage",
			"max_hp": 200.0,
			"attack_damage": 10.0,
			"attack_range": 2.5,
			"attack_speed": 0.8,
			"move_speed": 0.65,
			"armor": 0.10,
			"magic_resist": 0.20,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 120.0,
			"mana_per_attack": 10.0,
			"ability_cd": 6.5,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"",
			"respawn_time": 0.0,
		},
		"description": "A dark sorcerer who commands the undead, summoning allies to fight for him.",
		"ability_desc": "Summons 2 Skeleton minions.",
		"ultimate_desc": "Summons 3 Ghoul minions.",
		"passive_desc": "",
		"passive_name": "",
		"ability_name": "Raise Dead",
		"ultimate_name": "Army of Darkness",
		"ability": {
			"kind": &"summon_ally",
			"params": {
				"spawn_radius": 2.0,
				"minions": [
					{"minion_id": "skeleton", "count": 2}
				],
				"reason": "Raise Dead"
			},
			"requires_target_in_range": false,
		},
		"ultimate": {
			"kind": &"summon_ally",
			"params": {
				"spawn_radius": 2.0,
				"minions": [
					{"minion_id": "ghoul", "count": 3}
				],
				"reason": "Army of Darkness"
			}
		},
		"passive_ids": [],
	},
}

const PASSIVE_DATA := {
	&"duelist": {
		&"post_attack": [{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "attack_damage",
				"additive": 2.0,
				"duration": 3.0,
				"duration_type": "respawn",
				"max_stacks": 7,
				"target_self": true,
				"stack_behavior": "refresh",
				"reason": "Duelist",
			}
    	}],
	},
	&"eagle_eye": {
		&"post_attack": [{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "armor",
				"additive": -0.02,
				"duration": 3.0,
				"duration_type": "respawn",
				"max_stacks": 7,
				"stack_behavior": "refresh",
				"reason": "Eagle Eye"
			}
		}],
	},
	&"bastion": {
		&"on_ally_defense": [{
			"kind": &"redirect_damage",
			"params": {
				"redirect_ratio": 0.20,
				"reduction_ratio": 0.0,
				"redirect_cap": 0.0,
				"reason": "Bastion"
			}
		}],
		"radius": 2.0,
		"trigger_phase": "before",
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
				"min_distance": 2.0,
				"multiplier": 1.30
			}
		}],
	},
	&"bloodlust": {
		&"on_takedown": [{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "life_steal",
				"additive": 0.05,
				"duration_type": "match",
				"target_self": true,
				"reason": "Bloodlust"
			}
		}],
	},
	&"unstable_creation": {
		&"on_tick": [{
			"kind": &"damage",
			"params": {
				"flat_amount": 5.0,
				"damage_type": "true",
				"target_self": true,
				"on_tick_interval": 1.0,
				"reason": "Unstable Creation"
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
				"multiplier": 0.90
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
	&"soul_feast": {
		&"post_heal": [{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "max_hp",
				"heal_gained_ratio": 0.50,
				"duration_type": "match",
				"target_self": true,
				"reason": "Soul Feast"
			}
		}]
	},
	&"stunning_strikes": {
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
			"kind": &"aoe_damage",
			"params": {
				"shape": "circle",
				"anchor": "target",
				"radius": 0.5,
				"damage_ratio": 0.3,
				"damage_type": "physical",
				"reason": "Explosion",
			}
		}],
	},
	&"devotion": {
		&"on_tick": [{
			"kind": &"mana_regen",
			"params": {
				"flat_amount": 10.0
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
			"kind": &"multi_effect",
			"params": {
				"effects": [
					{
						"kind": &"aoe_damage",
						"params": {
							"shape": "circle",
							"anchor": "self",
							"radius": 0.75,
							"flat_amount": 7.0,
							"damage_type": "physical",
							"reason": "Sweeping Strikes"
						}
					},
					{
						"kind": &"damage_based_heal",
						"params": {
							"damage_ratio": 1.0,
							"reason": "Sweeping Strikes"
						}
					}
				],
			},
		}],
	},
	&"bitter_chill": {
		&"post_attack": [{
			"kind": &"slow",
			"params": {
				"slow_percentage": 0.1,
				"duration": 1.5,
				"reason": "Bitter Chill"
			}
		}],
	},
	&"earthen_protection": {
		&"on_defense": [
		{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "armor",
				"additive": 0.05,
				"duration": 5.0,
				"duration_type": "respawn",
				"max_stacks": 10,
				"stack_behavior": "refresh",
				"reason": "Earthen Protection",
				"target_self": true
			}
		},
		{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "magic_resist",
				"additive": 0.05,
				"duration": 5.0,
				"duration_type": "respawn",
				"max_stacks": 10,
				"stack_behavior": "refresh",
				"reason": "Earthen Protection",
				"target_self": true
			}
		}
	],
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
				"shape": "circle",
				"anchor": "self",
				"radius": 3.0,
				"on_tick_interval": 5.0,
				"stacking_mode": "separate",
				"max_stacks": 0,
				"missing_hp_ratio": 0.05,
				"flat_amount": 5,
				"duration": 3.0,
				"tick_interval": 0.2,
				"allow_overheal": true,
				"target_self": true,
				"reason": "Restorative Mist"
			}
		}]
	},
	&"weapon_breaker": {
		&"post_attack": [{
			"kind": &"stat_modifier",
			"params": {
				"stat_name": "attack_speed",
				"multiplicative": 1.25,
				"duration": 3.0,
				"max_stacks": 1,
				"target_self": true,
				"stack_behavior": "refresh",
				"requires_target_status": "disarm",
				"status_target": "target",
				"reason": "Weapon Breaker"
			}
		}],
	},
}

const ROLE_CONFIG_DATA := {
	&"tank": {
		"stat_mods": {},
		"passive_on_tick": null,
		"passive_post_take_damage": null,
		#{
		#	"kind": &"post_damage_mana_gain",
		#	"params": {
		#		"damage_ratio": SimConstantsScript.TANK_MANA_GAIN_DAMAGE_RATIO
		#	}
		#}
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
		"passive_on_tick": null, #{"kind": &"mana_regen", "params": {"flat_amount": SimConstantsScript.MAGE_MANA_REGEN_TICK}}
		"passive_post_take_damage": null,
	},
	&"support": {
		"stat_mods": {},
		"passive_on_tick": null,
		"passive_post_take_damage": null,
	},
	&"minion": {
		"stat_mods": {},
		"passive_on_tick": null,
		"passive_post_take_damage": null,
	},
}

const MINION_DATA := {
	&"skeleton": {
		"stats": {
			"unit_id": &"skeleton",
			"name": &"Skeleton",
			"role": &"minion",
			"max_hp": 60.0,
			"attack_damage": 12.0,
			"attack_range": 0.3,
			"attack_speed": 0.85,
			"move_speed": 0.75,
			"armor": 0.05,
			"magic_resist": 0.05,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 0.0,
			"mana_per_attack": 0.0,
			"ability_cd": 0.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"unstable_creation",
			"respawn_time": 0.0,
		},
		"description": "A fragile undead warrior that fights with basic attacks.",
		"ability_desc": "",
		"ultimate_desc": "",
		"passive_desc": "Takes 5 true damage every second.",
		"passive_name": "Unstable Creation",
		"ability_name": "",
		"ultimate_name": "",
		"ability": {},
		"ultimate": {},
		"passive_ids": [&"unstable_creation"],
	},
	&"wolf": {
		"stats": {
			"unit_id": &"wolf",
			"name": &"Wolf",
			"role": &"minion",
			"max_hp": 100.0,
			"attack_damage": 12.0,
			"attack_range": 0.3,
			"attack_speed": 1.3,
			"move_speed": 0.9,
			"armor": 0.08,
			"magic_resist": 0.08,
			"tenacity": 0.0,
			"life_steal": 0.0,
			"mana_cost": 0.0,
			"mana_per_attack": 0.0,
			"ability_cd": 0.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"",
			"respawn_time": 0.0,
		},
		"description": "A swift beast companion that harasses enemies.",
		"ability_desc": "",
		"ultimate_desc": "",
		"passive_desc": "",
		"passive_name": "",
		"ability_name": "",
		"ultimate_name": "",
		"ability": {},
		"ultimate": {},
		"passive_ids": [],
	},
	&"ghoul": {
		"stats": {
			"unit_id": &"ghoul",
			"name": &"Ghoul",
			"role": &"minion",
			"max_hp": 170.0,
			"attack_damage": 15.0,
			"attack_range": 0.3,
			"attack_speed": 0.9,
			"move_speed": 0.5,
			"armor": 0.15,
			"magic_resist": 0.15,
			"tenacity": 0.0,
			"life_steal": 0.15,
			"mana_cost": 0.0,
			"mana_per_attack": 0.0,
			"ability_cd": 0.0,
			"projectile_speed": 0.0,
			"projectile_radius": 0.0,
			"passive_id": &"",
			"respawn_time": 0.0,
		},
		"description": "A rotting undead creature that sustains itself through combat.",
		"ability_desc": "",
		"ultimate_desc": "",
		"passive_desc": "Has innate 15% lifesteal",
		"passive_name": "Bloody Claws",
		"ability_name": "",
		"ultimate_name": "",
		"ability": {},
		"ultimate": {},
		"passive_ids": [],
	},
}

static func build_role_configs() -> Dictionary:
	# Use thread-local cache for multi-threading safety
	var thread_cache := _get_thread_cache()
	if not thread_cache["role_config"].is_empty():
		return thread_cache["role_config"]

	_maybe_install_frozen_specs(thread_cache)
	if not thread_cache["role_config"].is_empty():
		return thread_cache["role_config"]

	for role_id in ROLE_CONFIG_DATA:
		var data: Dictionary = ROLE_CONFIG_DATA[role_id]
		var stat_mods: Dictionary = data.get("stat_mods", {}).duplicate()
		
		var passive_on_tick = null
		if data.get("passive_on_tick") != null:
			passive_on_tick = _build_effect(data.get("passive_on_tick", {}))
		
		var passive_post_take_damage = null
		if data.get("passive_post_take_damage") != null:
			passive_post_take_damage = _build_effect(data.get("passive_post_take_damage", {}))
		
		thread_cache["role_config"][role_id] = RoleConfigSpecScript.new(stat_mods, passive_on_tick, passive_post_take_damage)
	
	return thread_cache["role_config"]

static func build_catalog() -> Dictionary:
	# Use thread-local cache for multi-threading safety
	var thread_cache := _get_thread_cache()
	if not thread_cache["catalog"].is_empty():
		return thread_cache["catalog"]

	_maybe_install_frozen_specs(thread_cache)
	if not thread_cache["catalog"].is_empty():
		return thread_cache["catalog"]

	# Build catalog from CHAMPION_DATA
	for unit_id in CHAMPION_DATA:
		thread_cache["catalog"][unit_id] = _build_champion(CHAMPION_DATA[unit_id])
	
	return thread_cache["catalog"]


static func build_role_by_hero_map() -> Dictionary:
	var cat := build_catalog()
	var out: Dictionary = {}
	for k in cat.keys():
		var spec: Variant = cat[k]
		out[String(k)] = String(spec.stats.role)
	return out


static func build_passive_registry() -> Dictionary:
	# Use thread-local cache for multi-threading safety
	var thread_cache := _get_thread_cache()
	if not thread_cache["passive"].is_empty():
		return thread_cache["passive"]

	_maybe_install_frozen_specs(thread_cache)
	if not thread_cache["passive"].is_empty():
		return thread_cache["passive"]

	for passive_id in PASSIVE_DATA:
		var data: Dictionary = PASSIVE_DATA[passive_id]
		var result: Dictionary = {}
		
		for hook in data:
			var hook_data: Variant = data[hook]
			if hook_data is Array:
				var built_effects: Array = []
				for effect_data in hook_data:
					built_effects.append(_build_effect(effect_data))
				result[hook] = built_effects
			else:
				result[hook] = hook_data
		
		thread_cache["passive"][passive_id] = result
	
	return thread_cache["passive"]

static func get_passive_entry(passive_id: StringName):
	return build_passive_registry().get(passive_id, {})

static func get_champion_ids() -> Array[StringName]:
	var thread_cache := _get_thread_cache()
	_maybe_install_frozen_specs(thread_cache)
	if thread_cache["champion_ids"].is_empty():
		var ids: Array[StringName] = []
		for unit_id in build_catalog().keys():
			ids.append(StringName(String(unit_id)))
		thread_cache["champion_ids"] = ids
	return thread_cache["champion_ids"].duplicate()

static func get_champion(unit_id: StringName):
	return build_catalog().get(unit_id, null)

static func build_minion_catalog() -> Dictionary:
	# Use thread-local cache for multi-threading safety
	var thread_cache := _get_thread_cache()
	if not thread_cache.get("minion", {}).is_empty():
		return thread_cache["minion"]

	_maybe_install_frozen_specs(thread_cache)
	if not thread_cache.get("minion", {}).is_empty():
		return thread_cache["minion"]

	for minion_id in MINION_DATA:
		thread_cache["minion"][minion_id] = _build_champion(MINION_DATA[minion_id])

	return thread_cache["minion"]

static func get_minion(minion_id: StringName):
	return build_minion_catalog().get(minion_id, null)

static func get_effective_stats(hero_id: StringName) -> Dictionary:
	# Thread-safe native core initialization
	if _native_core == null:
		_cache_mutex.lock()
		if _native_core == null:  # Double-check after lock
			if ClassDB.class_exists("TeamfightSimulationCore") and ClassDB.can_instantiate("TeamfightSimulationCore"):
				_native_core = ClassDB.instantiate("TeamfightSimulationCore")
			else:
				_cache_mutex.unlock()
				push_error("Native simulation core unavailable for effective stats")
				return {}
		_cache_mutex.unlock()
	
	if _native_core != null and _native_core.has_method("effective_champion_for"):
		var effective_champion: Dictionary = _native_core.effective_champion_for(hero_id)
		if not effective_champion.is_empty():
			return Dictionary(effective_champion.get("stats", {}))
	
	# Fallback to base stats if native fails
	var champion = get_champion(hero_id)
	if champion == null:
		return {}
	return champion.stats.to_dict().duplicate(true)

static func reload_balance_patches() -> void:
	# Load balance patches from JSON
	var file := FileAccess.open("res://fixtures/goldens/balance_patches.json", FileAccess.READ)
	if file == null:
		push_error("Failed to open balance_patches.json")
		return
	
	var json_string := file.get_as_text()
	file.close()
	var json := JSON.new()
	var parse_result := json.parse(json_string)
	if parse_result != OK:
		push_error("Failed to parse balance_patches.json: %s" % json.get_error_message())
		return
	
	var data: Dictionary = json.data
	var patches: Array = data.get("patches", [])
	
	# Thread-safe native core initialization
	_cache_mutex.lock()
	if _native_core == null:
		if ClassDB.class_exists("TeamfightSimulationCore") and ClassDB.can_instantiate("TeamfightSimulationCore"):
			_native_core = ClassDB.instantiate("TeamfightSimulationCore")
		else:
			_cache_mutex.unlock()
			push_error("Native simulation core unavailable for balance patch reload")
			return
	
	if _native_core != null and _native_core.has_method("set_balance_patches"):
		_native_core.set_balance_patches(patches)
	_cache_mutex.unlock()

static func export_schema_dict() -> Dictionary:
	var schema: Dictionary = {}
	var catalog := build_catalog()
	for unit_id in catalog.keys():
		var champ = catalog[unit_id]
		schema[String(unit_id)] = champ.to_dict()
	return schema
