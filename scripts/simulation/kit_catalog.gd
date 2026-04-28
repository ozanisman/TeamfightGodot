class_name KitCatalog
extends RefCounted

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const EffectSpecScript := preload("res://scripts/simulation/effect_spec.gd")
const KitSpecScript := preload("res://scripts/simulation/kit_spec.gd")

static var _kit_cache: Dictionary = {}
static var _kit_ids_cache: Array[StringName] = []

# Kit definitions for ability, ultimate, and passive swaps
const KIT_DATA := {
	# Balance suite test kits
	"balance_suite_weak_artillery_ability": {
		"ability": {
			"kind": "projectile",
			"params": {
				"damage_multiplier": 0.05,
				"damage_type": "physical",
				"radius_override": null,
				"reason": "Balance suite kit_id test",
				"speed_override": null,
				"stun_duration": 0.8
			},
			"requires_target_in_range": true
		}
	},
	
	# Example ability swap kits
	"artillery_nuke_ability": {
		"ability": {
			"kind": "projectile",
			"params": {
				"damage_multiplier": 3.0,
				"damage_type": "magic",
				"radius_override": 2.0,
				"reason": "Artillery Nuke",
				"speed_override": 3.0,
				"stun_duration": 1.5
			},
			"requires_target_in_range": true
		}
	},
	
	# Example ultimate swap kits
	"tank_aoe_ultimate": {
		"ultimate": {
			"kind": "multi",
			"params": {
				"effects": [
					{
						"kind": "self_aoe_damage",
						"params": {
							"damage_type": "physical",
							"flat_amount": 200.0,
							"radius": 3.0,
							"reason": "Tank AOE Ultimate"
						},
						"requires_target_in_range": false
					},
					{
						"kind": "stun",
						"params": {
							"duration": 2.0,
							"reason": "Tank AOE Ultimate"
						},
						"requires_target_in_range": false
					}
				],
				"reason": "Tank AOE Ultimate"
			},
			"requires_target_in_range": false
		}
	},
	
	# Example passive swap kits
	"assassin_dodge_passive": {
		"passive_ids": ["agility", "technique"]
	},
	
	# Combined kits with multiple components
	"support_heal_ultimate": {
		"ultimate": {
			"kind": "multi",
			"params": {
				"effects": [
					{
						"kind": "self_aoe_heal",
						"params": {
							"heal_ratio": 0.3,
							"radius": 2.5,
							"reason": "Support Heal Ultimate"
						},
						"requires_target_in_range": false
					},
					{
						"kind": "mana_restore",
						"params": {
							"flat_amount": 50.0,
							"reason": "Support Heal Ultimate"
						},
						"requires_target_in_range": false
					}
				],
				"reason": "Support Heal Ultimate"
			},
			"requires_target_in_range": false
		},
		"passive_ids": ["rejuvenation"]
	}
}

static func _build_effect(data: Dictionary) -> EffectSpecScript:
	var params: Dictionary = data["params"].duplicate()
	var requires_target_in_range: bool = bool(data.get("requires_target_in_range", true))
	
	# Handle nested effects
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

static func build_kit_catalog() -> Dictionary:
	if not _kit_cache.is_empty():
		return _kit_cache.duplicate(true)
	
	var catalog: Dictionary = {}
	
	for kit_id in KIT_DATA.keys():
		var kit_data: Dictionary = KIT_DATA[kit_id]
		var kit := KitSpecScript.new()
		
		# Build ability if present
		if kit_data.has("ability"):
			kit.ability = _build_effect(kit_data["ability"])
		
		# Build ultimate if present
		if kit_data.has("ultimate"):
			kit.ultimate = _build_effect(kit_data["ultimate"])
		
		# Set passive IDs if present
		if kit_data.has("passive_ids"):
			kit.passive_ids = kit_data["passive_ids"]
		
		catalog[kit_id] = kit
	
	_kit_cache = catalog
	_kit_ids_cache = catalog.keys()
	
	return catalog.duplicate(true)

static func get_kit_ids() -> Array[StringName]:
	if _kit_ids_cache.is_empty():
		build_kit_catalog()
	return _kit_ids_cache.duplicate()

static func get_kit(kit_id: StringName):
	return build_kit_catalog().get(kit_id, null)

static func clear_cache() -> void:
	_kit_cache.clear()
	_kit_ids_cache.clear()

static func generate_kit_json_from_gdscript() -> Dictionary:
	var catalog := build_kit_catalog()
	
	# Convert to JSON format expected by native core
	var result: Dictionary = {
		"kits": {}
	}
	
	for kit_id in catalog.keys():
		var kit = catalog[kit_id]
		result["kits"][String(kit_id)] = kit.to_dict()
	
	return result

static func write_kit_json_to_file(output_path: String = "res://fixtures/goldens/champion_kits.json") -> bool:
	var kit_data := generate_kit_json_from_gdscript()
	var json_string := JSON.stringify(kit_data, "\t")
	if json_string.is_empty():
		push_error("Failed to stringify kit data.")
		return false
	
	var file := FileAccess.open(output_path, FileAccess.WRITE)
	if file == null:
		push_error("Failed to open kit file for writing: %s" % output_path)
		return false
	
	file.store_string(json_string)
	file.close()
	
	print("Kit schema exported successfully to: %s" % output_path)
	return true
