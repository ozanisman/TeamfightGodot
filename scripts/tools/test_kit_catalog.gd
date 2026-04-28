extends RefCounted

const KitCatalogScript := preload("res://scripts/simulation/kit_catalog.gd")

static func main():
	print("Testing Kit Catalog...")
	
	# Test kit catalog building
	var catalog = KitCatalogScript.build_kit_catalog()
	print("Built catalog with %d kits" % catalog.size())
	
	# Test JSON generation
	var json_data = KitCatalogScript.generate_kit_json_from_gdscript()
	print("Generated JSON with kits: %s" % json_data["kits"].keys())
	
	# Test JSON file writing
	var success = KitCatalogScript.write_kit_json_to_file()
	if success:
		print("✅ Kit catalog test completed successfully!")
	else:
		print("❌ Kit catalog test failed!")
	
	# Test individual kit access
	var test_kit = KitCatalogScript.get_kit("balance_suite_weak_artillery_ability")
	if test_kit != null:
		print("✅ Successfully retrieved test kit")
	else:
		print("❌ Failed to retrieve test kit")
