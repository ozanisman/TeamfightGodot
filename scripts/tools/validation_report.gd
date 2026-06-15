extends SceneTree

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
	const DraftStrategyNativeScript := preload("res://scripts/tools/draft_strategy_native.gd")
	
	var report := []
	report.append("=== Native Draft Strategy Validation Report ===")
	
	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		report.append("ERROR: Native backend unavailable")
		_save_report(report)
		quit(1)
		return
	
	report.append("Backend available: OK")
	
	var stats_dir := "res://stats_output_100k"
	report.append("Stats directory: " + stats_dir)
	
	# Test 1: Direct API call
	report.append("\n--- Test 1: Direct API Call ---")
	var available := ["swordsman", "cleric", "colossus", "frost_mage"]
	var allies := ["archer"]
	var enemies := ["berserker"]
	
	var api_results := backend.get_draft_ai_pick_recommendations(stats_dir, available, allies, enemies, 3)
	report.append("API returned " + str(api_results.size()) + " recommendations")
	
	for i in range(mini(3, api_results.size())):
		var rec = api_results[i]
		report.append(str(i + 1) + ". " + str(rec.candidate) + " (score: " + str(rec.total_score) + ")")
		report.append("   base_power: " + str(rec.base_power))
		report.append("   ally_synergy: " + str(rec.ally_synergy))
		report.append("   enemy_counter_value: " + str(rec.enemy_counter_value))
		report.append("   counter_risk: " + str(rec.counter_risk))
	
	# Test 2: Strategy pick recommendation
	report.append("\n--- Test 2: Strategy Pick Recommendation ---")
	var strategy := DraftStrategyNativeScript.new(stats_dir)
	var pick := strategy.recommend_next_pick(allies, enemies, available)
	report.append("Strategy recommended pick: " + str(pick))
	report.append("Pick in available: " + str(pick in available))
	
	# Test 3: Strategy ban recommendation
	report.append("\n--- Test 3: Strategy Ban Recommendation ---")
	var ban := strategy.recommend_next_ban(allies, enemies, available)
	report.append("Strategy recommended ban: " + str(ban))
	report.append("Ban in available: " + str(ban in available))
	
	# Test 4: Empty state handling
	report.append("\n--- Test 4: Empty State Handling ---")
	var empty_pick := strategy.recommend_next_pick([], [], available)
	report.append("Empty state pick: " + str(empty_pick))
	report.append("Empty pick in available: " + str(empty_pick in available))
	
	# Test 5: Deterministic behavior
	report.append("\n--- Test 5: Deterministic Behavior ---")
	var pick_a := strategy.recommend_next_pick(allies, enemies, available)
	var pick_b := strategy.recommend_next_pick(allies, enemies, available)
	report.append("First pick: " + str(pick_a))
	report.append("Second pick: " + str(pick_b))
	report.append("Deterministic: " + str(pick_a == pick_b))
	
	# Test 6: Ban API
	report.append("\n--- Test 6: Ban API ---")
	var ban_results := backend.get_draft_ai_ban_recommendations(stats_dir, available, allies, enemies, 3)
	report.append("Ban API returned " + str(ban_results.size()) + " recommendations")
	
	for i in range(mini(3, ban_results.size())):
		var rec = ban_results[i]
		report.append(str(i + 1) + ". " + str(rec.candidate) + " (score: " + str(rec.total_score) + ")")
		report.append("   enemy_pick_value: " + str(rec.enemy_pick_value))
		report.append("   enemy_synergy: " + str(rec.enemy_synergy))
		report.append("   counters_my_team: " + str(rec.counters_my_team))
		report.append("   own_pick_value_penalty: " + str(rec.own_pick_value_penalty))
	
	report.append("\n=== Validation Complete ===")
	_save_report(report)
	quit(0)

func _save_report(lines: Array) -> void:
	var f := FileAccess.open("user://native_validation_report.txt", FileAccess.WRITE)
	if f:
		f.store_string("\n".join(lines))
		f.close()
		print("Report saved to user://native_validation_report.txt")
	else:
		print("Failed to save report")
	
	# Also try project root
	var f2 := FileAccess.open("native_validation_report.txt", FileAccess.WRITE)
	if f2:
		f2.store_string("\n".join(lines))
		f2.close()
		print("Report saved to native_validation_report.txt")
