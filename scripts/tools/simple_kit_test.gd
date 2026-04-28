extends RefCounted

const KitCatalogScript := preload("res://scripts/simulation/kit_catalog.gd")

static func main():
	print("=== Simple Kit Test ===")
	
	# Test just the KIT_DATA
	print("KIT_DATA has %d entries" % KitCatalogScript.KIT_DATA.size())
	for kit_id in KitCatalogScript.KIT_DATA.keys():
		print("  - %s" % kit_id)
	
	# Test building catalog
	var catalog = KitCatalogScript.build_kit_catalog()
	print("Built catalog with %d kits" % catalog.size())
	
	print("=== End Test ===")
