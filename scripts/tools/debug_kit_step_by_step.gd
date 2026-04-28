extends RefCounted

static func main():
	print("=== Step-by-Step Kit Debug ===")
	
	# Test 1: Check if we can access KIT_DATA
	const KitCatalogScript := preload("res://scripts/simulation/kit_catalog.gd")
	print("1. KIT_DATA size: %d" % KitCatalogScript.KIT_DATA.size())
	
	# Test 2: Try to create a KitSpec
	const KitSpecScript := preload("res://scripts/simulation/kit_spec.gd")
	var kit = KitSpecScript.new()
	print("2. KitSpec created successfully")
	
	# Test 3: Try to create an EffectSpec
	const EffectSpecScript := preload("res://scripts/simulation/effect_spec.gd")
	var effect = EffectSpecScript.new("projectile", {"damage_multiplier": 1.0}, true)
	print("3. EffectSpec created successfully")
	
	# Test 4: Try to build a simple kit
	var simple_kit_data = {
		"ability": {
			"kind": "projectile",
			"params": {"damage_multiplier": 1.0},
			"requires_target_in_range": true
		}
	}
	
	kit.ability = effect
	print("4. Kit ability set successfully")
	
	# Test 5: Try to convert to dict
	var dict = kit.to_dict()
	print("5. Kit to_dict successful, keys: %s" % dict.keys())
	
	print("=== End Debug ===")
