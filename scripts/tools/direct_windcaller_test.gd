extends RefCounted

const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"

func test_windcaller_directly():
	print("=== Direct Windcaller Effects Test ===")
	
	# Load native extension
	var load_status = GDExtensionManager.load_extension(NativeExtensionPath)
	if load_status != GDExtensionManager.LOAD_STATUS_OK and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED:
		print("ERROR: Failed to load native extension")
		return
	
	var core = TeamfightSimulationCore.new()
	
	# Create windcaller unit directly
	var windcaller = {
		"instance_id": 1,
		"team": "player", 
		"archetype": "windcaller",
		"pos_x": 5.0,
		"pos_y": 5.0,
		"hp": 80.0,
		"max_hp": 80.0,
		"attack_damage": 14.0,
		"ability_power": 15.0,
		"move_speed": 1.0,
		"tenacity": 0.0,
		"alive": true
	}
	
	var guardian = {
		"instance_id": 2,
		"team": "enemy",
		"archetype": "guardian", 
		"pos_x": 6.0,
		"pos_y": 5.0,
		"hp": 120.0,
		"max_hp": 120.0,
		"attack_damage": 20.0,
		"move_speed": 0.8,
		"tenacity": 0.0,
		"alive": true
	}
	
	# Start match with our units
	var units = [windcaller, guardian]
	core.begin_match(units)
	
	print("Match started with windcaller vs guardian")
	
	# Test ability effects directly
	print("\n--- Testing Windcaller Ability Effects ---")
	
	# Create the exact effect chain from windcaller ability
	var ability_effects = [
		{"kind": "multi", "params": {
			"effects": [
				{"kind": "damage", "params": {"damage_multiplier": 1.2, "damage_type": "magic", "reason": "Wind Blast"}},
				{"kind": "knockback", "params": {"distance": 1.5, "direction": "away_from_source", "reason": "Wind Blast"}},
				{"kind": "shield", "params": {"damage_ratio": 0.15, "requires_result_from": "knockback", "requires_field": "knockback_applied", "requires_value": true, "reason": "Gust Protection"}}
			],
			"reason": "Wind Blast"
		}}
	]
	
	var context = {
		"source_instance_id": 1,
		"target_instance_id": 2,
		"action_kind": "ability",
		"damage": 0.0
	}
	
	var ability_results = core.run_effects(ability_effects, context)
	print("Ability effect results:", ability_results)
	
	# Check for shield generation
	var shield_generated = false
	if ability_results.has("multi"):
		var multi_result = ability_results["multi"]
		if multi_result.has("shield"):
			var shield_result = multi_result["shield"]
			if shield_result.get("success", false):
				shield_generated = true
				print("✅ Shield generated from ability!")
	
	if not shield_generated:
		print("❌ No shield generated from ability")
	
	# Test ultimate effects directly
	print("\n--- Testing Windcaller Ultimate Effects ---")
	
	var ultimate_effects = [
		{"kind": "multi", "params": {
			"effects": [
				{"kind": "self_aoe_damage", "params": {"radius": 2.5, "damage_multiplier": 3.0, "damage_type": "magic", "reason": "Tornado"}},
				{"kind": "self_aoe_knockback", "params": {"radius": 2.5, "distance": 2.5, "direction": "away_from_source", "reason": "Tornado"}},
				{"kind": "shield", "params": {"damage_ratio": 0.15, "requires_result_from": "self_aoe_knockback", "requires_field": "knockback_applied", "requires_value": true, "reason": "Gust Protection"}}
			],
			"reason": "Tornado"
		}}
	]
	
	var ultimate_context = {
		"source_instance_id": 1,
		"action_kind": "ultimate",
		"damage": 0.0
	}
	
	var ultimate_results = core.run_effects(ultimate_effects, ultimate_context)
	print("Ultimate effect results:", ultimate_results)
	
	# Check for shield generation
	var ultimate_shield_generated = false
	if ultimate_results.has("multi"):
		var multi_result = ultimate_results["multi"]
		if multi_result.has("shield"):
			var shield_result = multi_result["shield"]
			if shield_result.get("success", false):
				ultimate_shield_generated = true
				print("✅ Shield generated from ultimate!")
	
	if not ultimate_shield_generated:
		print("❌ No shield generated from ultimate")
	
	# Run a few ticks to see combat
	print("\n--- Running Combat Simulation ---")
	for i in range(5):
		var tick_result = core.advance_one_tick()
		print("Tick", i+1, "result:", tick_result)
	
	var final_summary = core.finish_and_summarize()
	print("\n--- Final Match Summary ---")
	print(final_summary)
	
	print("\n=== Test Complete ===")
	print("Ability Shield:", shield_generated ? "✅ YES" : "❌ NO")
	print("Ultimate Shield:", ultimate_shield_generated ? "✅ YES" : "❌ NO")

func _init():
	test_windcaller_directly()
	get_tree().quit()
