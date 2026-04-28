extends RefCounted

const KitCatalogScript := preload("res://scripts/simulation/kit_catalog.gd")

func _init():
	print("Manual Kit Catalog Test")
	
	# Clear cache to ensure fresh build
	KitCatalogScript.clear_cache()
	
	# Test kit catalog building
	var catalog = KitCatalogScript.build_kit_catalog()
	print("Built catalog with %d kits" % catalog.size())
	
	for kit_id in catalog.keys():
		print("Kit: %s" % kit_id)
	
	# Test JSON generation
	var json_data = KitCatalogScript.generate_kit_json_from_gdscript()
	print("Generated JSON with kits: %s" % json_data["kits"].keys())
	
	# Test JSON file writing
	var success = KitCatalogScript.write_kit_json_to_file()
	print("Write success: %s" % success)
