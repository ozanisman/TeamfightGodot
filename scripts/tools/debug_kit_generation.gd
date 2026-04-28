extends RefCounted

static func test_kit_generation():
	const KitCatalogScript := preload("res://scripts/simulation/kit_catalog.gd")
	
	print("=== Kit Catalog Test ===")
	
	# Clear cache to ensure fresh build
	KitCatalogScript.clear_cache()
	
	# Test kit catalog building
	var catalog = KitCatalogScript.build_kit_catalog()
	print("Built catalog with %d kits" % catalog.size())
	
	for kit_id in catalog.keys():
		print("  - %s" % kit_id)
	
	# Test JSON generation
	var json_data = KitCatalogScript.generate_kit_json_from_gdscript()
	print("Generated JSON with %d kits" % json_data["kits"].size())
	
	# Test JSON file writing
	var success = KitCatalogScript.write_kit_json_to_file()
	print("Write success: %s" % success)
	
	print("=== End Test ===")
