extends RefCounted

const KitSpecScript := preload("res://scripts/simulation/kit_spec.gd")
const EffectSpecScript := preload("res://scripts/simulation/effect_spec.gd")

static func main():
	print("=== Kit Spec Test ===")
	
	# Test creating a KitSpec
	var kit = KitSpecScript.new()
	print("Created KitSpec successfully")
	
	# Test creating an EffectSpec
	var effect_data = {
		"kind": "projectile",
		"params": {
			"damage_multiplier": 1.0,
			"damage_type": "physical",
			"reason": "Test"
		},
		"requires_target_in_range": true
	}
	
	var effect = EffectSpecScript.new("projectile", effect_data["params"], true)
	print("Created EffectSpec successfully")
	
	# Test setting the effect on the kit
	kit.ability = effect
	print("Set ability on kit successfully")
	
	# Test converting to dict
	var kit_dict = kit.to_dict()
	print("Kit dict keys: %s" % kit_dict.keys())
	
	print("=== End Test ===")
