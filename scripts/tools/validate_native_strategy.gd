extends SceneTree

## Quick validation script for native draft strategy
## Tests basic functionality without full A/B test infrastructure

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftStrategyNativeScript := preload("res://scripts/tools/draft_strategy_native.gd")

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	print("=== Native Draft Strategy Validation ===")
	
	var stats_dir := "res://stats_output_100k"
	print("Stats directory: %s" % stats_dir)
	
	# Test 1: Backend availability
	print("\n1. Testing backend availability...")
	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("Native backend unavailable - cannot continue")
		quit(1)
		return
	print("   Backend available: OK")
	
	# Test 2: Strategy instantiation
	print("\n2. Testing strategy instantiation...")
	var strategy := DraftStrategyNativeScript.new(stats_dir)
	if strategy == null:
		push_error("Strategy instantiation failed")
		quit(1)
		return
	print("   Strategy instantiated: OK")
	
	# Test 3: Pick recommendation with normal state
	print("\n3. Testing pick recommendation (normal state)...")
	var available := ["swordsman", "cleric", "colossus", "frost_mage", "berserker"]
	var allies := ["archer"]
	var enemies := []
	var pick1 := strategy.recommend_next_pick(allies, enemies, available)
	if pick1.is_empty():
		push_error("Pick recommendation returned empty")
		quit(1)
		return
	if pick1 not in available:
		push_error("Pick recommendation returned unavailable champion: %s" % pick1)
		quit(1)
		return
	print("   Recommended pick: %s (in available: %s)" % [pick1, pick1 in available])
	
	# Test 4: Pick recommendation with empty allies/enemies
	print("\n4. Testing pick recommendation (empty allies/enemies)...")
	var pick2 := strategy.recommend_next_pick([], [], available)
	if pick2.is_empty():
		push_error("Pick recommendation with empty state returned empty")
		quit(1)
		return
	if pick2 not in available:
		push_error("Pick recommendation returned unavailable champion: %s" % pick2)
		quit(1)
		return
	print("   Recommended pick: %s (in available: %s)" % [pick2, pick2 in available])
	
	# Test 5: Ban recommendation
	print("\n5. Testing ban recommendation...")
	var ban1 := strategy.recommend_next_ban(allies, enemies, available)
	if ban1.is_empty():
		push_error("Ban recommendation returned empty")
		quit(1)
		return
	if ban1 not in available:
		push_error("Ban recommendation returned unavailable champion: %s" % ban1)
		quit(1)
		return
	print("   Recommended ban: %s (in available: %s)" % [ban1, ban1 in available])
	
	# Test 6: Deterministic recommendations
	print("\n6. Testing deterministic recommendations...")
	var pick3a := strategy.recommend_next_pick(allies, enemies, available)
	var pick3b := strategy.recommend_next_pick(allies, enemies, available)
	if pick3a != pick3b:
		push_error("Recommendations not deterministic: %s vs %s" % [pick3a, pick3b])
		quit(1)
		return
	print("   Deterministic: OK (both returned %s)" % pick3a)
	
	# Test 7: Direct API call for breakdown
	print("\n7. Testing direct API with breakdown...")
	var api_results := backend.get_draft_ai_pick_recommendations(stats_dir, available, allies, enemies, 3)
	if api_results.is_empty():
		push_error("Direct API returned empty results")
		quit(1)
		return
	print("   Got %d recommendations" % api_results.size())
	for i in range(mini(3, api_results.size())):
		var rec = api_results[i]
		print("   %d. %s (score: %.4f)" % [i + 1, rec.candidate, rec.total_score])
		print("      base_power: %.4f, ally_synergy: %.4f, enemy_counter_value: %.4f, counter_risk: %.4f" % [
			rec.base_power, rec.ally_synergy, rec.enemy_counter_value, rec.counter_risk
		])
	
	print("\n=== All Validation Tests Passed ===")
	quit(0)
