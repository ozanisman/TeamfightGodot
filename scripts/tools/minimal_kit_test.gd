extends RefCounted

static func generate_minimal_kit_json() -> Dictionary:
	# Create a simple kit structure without complex objects
	var kits = {
		"test_ability": {
			"ability": {
				"kind": "projectile",
				"params": {
					"damage_ratio": 1.0,
					"damage_type": "physical",
					"reason": "Test"
				},
				"requires_target_in_range": true
			}
		},
		"test_ultimate": {
			"ultimate": {
					"kind": "multi_effect",
				"params": {
					"effects": [
						{
							"kind": "damage",
							"params": {
								"damage_ratio": 2.0,
								"reason": "Test Ultimate"
							},
							"requires_target_in_range": true
						}
					],
					"reason": "Test Ultimate"
				},
				"requires_target_in_range": false
			}
		},
		"test_passive": {
			"passive_ids": ["agility", "technique"]
		}
	}
	
	return {"kits": kits}

static func write_minimal_kit_json() -> bool:
	var kit_data = generate_minimal_kit_json()
	var json_string = JSON.stringify(kit_data, "\t")
	
	if json_string.is_empty():
		print("Failed to stringify kit data")
		return false
	
	var file = FileAccess.open("res://fixtures/goldens/champion_kits.json", FileAccess.WRITE)
	if file == null:
		print("Failed to open file for writing")
		return false
	
	file.store_string(json_string)
	file.close()
	
	print("Minimal kit JSON written successfully")
	print("Generated %d kits" % kit_data["kits"].size())
	for kit_id in kit_data["kits"].keys():
		print("  - %s" % kit_id)
	
	return true

static func main():
	print("=== Minimal Kit Test ===")
	var success = write_minimal_kit_json()
	print("Success: %s" % success)
	print("=== End Test ===")
