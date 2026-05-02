# Simple test for conditional effects
extends RefCounted

func test():
	print("=== Testing Conditional Effects ===")
	
	# Load native extension
	var load_status = GDExtensionManager.load_extension("res://teamfight_simulation_core.gdextension")
	if load_status != GDExtensionManager.LOAD_STATUS_OK and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED:
		print("ERROR: Failed to load native extension")
		return
	
	var core = TeamfightSimulationCore.new()
	
	# Test 1: Single target knockback with conditional shield
	print("\n--- Test 1: Single Target Knockback + Shield ---")
	var effects1 = [
		{"kind": "knockback", "params": {"distance": 1.5, "direction": "away_from_source", "reason": "Test Knockback"}},
		{"kind": "shield", "params": {"damage_ratio": 0.15, "requires_result_from": "knockback", "requires_field": "knockback_applied", "requires_value": true, "reason": "Test Shield"}}
	]
	
	var results1 = core.run_effects(effects1, {})
	print("Results 1:", results1)
	
	# Test 2: AOE knockback with conditional shield
	print("\n--- Test 2: AOE Knockback + Shield ---")
	var effects2 = [
		{"kind": "self_aoe_knockback", "params": {"radius": 2.5, "distance": 2.5, "direction": "away_from_source", "reason": "Test AOE Knockback"}},
		{"kind": "shield", "params": {"damage_ratio": 0.15, "requires_result_from": "self_aoe_knockback", "requires_field": "knockback_applied", "requires_value": true, "reason": "Test Shield"}}
	]
	
	var results2 = core.run_effects(effects2, {})
	print("Results 2:", results2)
	
	print("\n=== Test Complete ===")

func _init():
	test()
