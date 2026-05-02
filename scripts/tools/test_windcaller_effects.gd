extends RefCounted

const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"
const SimulationSchemaScript := preload("res://scripts/simulation/simulation_schema.gd")

func test_windcaller_effects():
	print("=== Testing Windcaller Effects ===")
	
	# Load native extension
	var load_status: int = GDExtensionManager.load_extension(NativeExtensionPath)
	if load_status != GDExtensionManager.LOAD_STATUS_OK and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED:
		print("ERROR: Failed to load native extension")
		return false
	
	# Create simulation core
	var core = TeamfightSimulationCore.new()
	
	# Create test units
	var player_unit = SimulationSchemaScript.UnitSnapshot.new()
	player_unit.instance_id = 1
	player_unit.team = "player"
	player_unit.archetype = "windcaller"
	player_unit.pos_x = 5.0
	player_unit.pos_y = 5.0
	player_unit.hp = 80.0
	player_unit.max_hp = 80.0
	player_unit.attack_damage = 14.0
	player_unit.ability_power = 15.0
	player_unit.move_speed = 1.0
	player_unit.tenacity = 0.0
	
	var enemy_unit = SimulationSchemaScript.UnitSnapshot.new()
	enemy_unit.instance_id = 2
	enemy_unit.team = "enemy"
	enemy_unit.archetype = "guardian"
	enemy_unit.pos_x = 6.0
	enemy_unit.pos_y = 5.0
	enemy_unit.hp = 120.0
	enemy_unit.max_hp = 120.0
	enemy_unit.attack_damage = 20.0
	enemy_unit.move_speed = 0.8
	enemy_unit.tenacity = 0.0
	
	# Add units to simulation
	core.begin_match([player_unit, enemy_unit])
	
	# Test ability (single target knockback -> shield)
	print("\n--- Testing Ability: Wind Blast ---")
	var ability_context = SimulationSchemaScript.EffectContext.new()
	ability_context.source_instance_id = 1
	ability_context.target_instance_id = 2
	ability_context.action_kind = "ability"
	ability_context.damage = 0.0  # Shield should be based on knockback, not damage
	
	# Create ability effects: damage + knockback + conditional shield
	var ability_effects = [
		{"kind": "damage", "params": {"damage_multiplier": 1.2, "damage_type": "magic", "reason": "Wind Blast"}},
		{"kind": "knockback", "params": {"distance": 1.5, "direction": "away_from_source", "reason": "Wind Blast"}},
		{"kind": "shield", "params": {"damage_ratio": 0.15, "requires_result_from": "knockback", "requires_field": "knockback_applied", "requires_value": true, "reason": "Gust Protection"}}
	]
	
	var ability_results = core.run_effects(ability_effects, ability_context)
	print("Ability results:", ability_results)
	
	# Check if shield was applied
	var shield_applied = false
	for effect_key in ability_results:
		var result = ability_results[effect_key]
		if result.has("success") and result.get("success", false):
			if effect_key == "shield":
				shield_applied = true
				print("✅ Shield applied successfully!")
			elif effect_key == "knockback":
				if result.get("knockback_applied", false):
					print("✅ Knockback applied successfully!")
	
	if not shield_applied:
		print("❌ Shield was NOT applied")
	
	# Test ultimate (AOE knockback -> shield)
	print("\n--- Testing Ultimate: Tornado ---")
	var ultimate_context = SimulationSchemaScript.EffectContext.new()
	ultimate_context.source_instance_id = 1
	ultimate_context.action_kind = "ultimate"
	ultimate_context.damage = 0.0
	
	# Create ultimate effects: AOE damage + AOE knockback + conditional shield
	var ultimate_effects = [
		{"kind": "self_aoe_damage", "params": {"radius": 2.5, "damage_multiplier": 3.0, "damage_type": "magic", "reason": "Tornado"}},
		{"kind": "self_aoe_knockback", "params": {"radius": 2.5, "distance": 2.5, "direction": "away_from_source", "reason": "Tornado"}},
		{"kind": "shield", "params": {"damage_ratio": 0.15, "requires_result_from": "self_aoe_knockback", "requires_field": "knockback_applied", "requires_value": true, "reason": "Gust Protection"}}
	]
	
	var ultimate_results = core.run_effects(ultimate_effects, ultimate_context)
	print("Ultimate results:", ultimate_results)
	
	# Check if shield was applied
	var ultimate_shield_applied = false
	for effect_key in ultimate_results:
		var result = ultimate_results[effect_key]
		if result.has("success") and result.get("success", false):
			if effect_key == "shield":
				ultimate_shield_applied = true
				print("✅ Ultimate shield applied successfully!")
			elif effect_key == "self_aoe_knockback":
				if result.get("knockback_applied", false):
					print("✅ AOE Knockback applied successfully!")
	
	if not ultimate_shield_applied:
		print("❌ Ultimate shield was NOT applied")
	
	# Get final unit states
	var final_state = core.advance_one_tick()
	print("\n--- Final Unit States ---")
	print("Player HP:", player_unit.hp, "Shield:", final_state.get("player_shield", 0))
	print("Enemy HP:", enemy_unit.hp, "Shield:", final_state.get("enemy_shield", 0))
	
	core.finish_and_summarize()
	
	print("\n=== Test Complete ===")
	return shield_applied and ultimate_shield_applied

# Main entry point
func _init():
	test_windcaller_effects()
	get_tree().quit()
