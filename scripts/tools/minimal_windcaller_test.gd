# Minimal test to verify windcaller effects work
extends RefCounted

func test():
	print("=== Minimal Windcaller Test ===")
	
	# Load extension
	var status = GDExtensionManager.load_extension("res://teamfight_simulation_core.gdextension")
	print("Extension status:", status)
	
	var core = TeamfightSimulationCore.new()
	print("Core created:", core != null)
	
	# Test single target knockback + shield chain
	print("\n--- Testing Single Target Chain ---")
	var effects1 = [
		{"kind": "knockback", "params": {"distance": 1.5, "reason": "Test"}},
		{"kind": "shield", "params": {"damage_ratio": 0.15, "requires_result_from": "knockback", "requires_field": "knockback_applied", "requires_value": true, "reason": "Test"}}
	]
	
	var results1 = core.run_effects(effects1, {})
	print("Single target results:", results1)
	
	# Test AOE knockback + shield chain  
	print("\n--- Testing AOE Chain ---")
	var effects2 = [
		{"kind": "self_aoe_knockback", "params": {"radius": 2.5, "distance": 2.5, "reason": "Test"}},
		{"kind": "shield", "params": {"damage_ratio": 0.15, "requires_result_from": "self_aoe_knockback", "requires_field": "knockback_applied", "requires_value": true, "reason": "Test"}}
	]
	
	var results2 = core.run_effects(effects2, {})
	print("AOE results:", results2)
	
	print("=== Test Complete ===")

func _init():
	test()
	get_tree().quit()
