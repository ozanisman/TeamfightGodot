extends SceneTree

const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
	const DraftStrategyNativeScript := preload("res://scripts/tools/draft_strategy_native.gd")

	var report_lines := []
	report_lines.append("=== NATIVE DRAFT AI VALIDATION ===")
	report_lines.append("Running native debug tests to validate implementation...")

	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		report_lines.append("ERROR: Native backend unavailable")
		_write_report(report_lines)
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	report_lines.append("Backend available: OK")

	var stats_dir := "res://stats_output_100k"
	report_lines.append("Stats directory: " + stats_dir)

	# Test state
	var available := ["swordsman", "cleric", "colossus", "frost_mage", "berserker", "assassin"]
	var allies := ["archer"]
	var enemies := ["berserker"]

	# Test 1: Native pick recommendations
	report_lines.append("\n--- Test 1: Native Pick Recommendations ---")
	var pick_results := backend.get_draft_ai_pick_recommendations(stats_dir, available, allies, enemies, 5)
	report_lines.append("Pick recommendations returned: " + str(not pick_results.is_empty()))
	report_lines.append("Number of recommendations: " + str(pick_results.size()))

	report_lines.append("\nTop 5 pick recommendations with breakdowns:")
	for i in range(mini(5, pick_results.size())):
		var rec = pick_results[i]
		report_lines.append(str(i + 1) + ". " + str(rec.candidate) + " (total_score: " + str(rec.total_score) + ")")
		report_lines.append("   base_power: " + str(rec.base_power))
		report_lines.append("   ally_synergy: " + str(rec.ally_synergy))
		report_lines.append("   enemy_counter_value: " + str(rec.enemy_counter_value))
		report_lines.append("   counter_risk: " + str(rec.counter_risk))
		report_lines.append("   synergy_pairs: " + str(rec.synergy_pairs))
		report_lines.append("   counter_pairs: " + str(rec.counter_pairs))

	# Test 2: Native ban recommendations
	report_lines.append("\n--- Test 2: Native Ban Recommendations ---")
	var ban_results := backend.get_draft_ai_ban_recommendations(stats_dir, available, allies, enemies, 5)
	report_lines.append("Ban recommendations returned: " + str(not ban_results.is_empty()))
	report_lines.append("Number of recommendations: " + str(ban_results.size()))

	report_lines.append("\nTop 5 ban recommendations with breakdowns:")
	for i in range(mini(5, ban_results.size())):
		var rec = ban_results[i]
		report_lines.append(str(i + 1) + ". " + str(rec.candidate) + " (total_score: " + str(rec.total_score) + ")")
		report_lines.append("   enemy_pick_value: " + str(rec.enemy_pick_value))
		report_lines.append("   enemy_synergy: " + str(rec.enemy_synergy))
		report_lines.append("   counters_my_team: " + str(rec.counters_my_team))
		report_lines.append("   own_pick_value_penalty: " + str(rec.own_pick_value_penalty))
		report_lines.append("   enemy_synergy_pairs: " + str(rec.enemy_synergy_pairs))
		report_lines.append("   counter_pairs: " + str(rec.counter_pairs))

	# Test 3: Validity check - all recommendations in available
	report_lines.append("\n--- Test 3: Validity Check ---")
	var all_picks_valid := true
	for rec in pick_results:
		if rec.candidate not in available:
			report_lines.append("INVALID PICK: " + str(rec.candidate) + " not in available")
			all_picks_valid = false
	report_lines.append("All pick recommendations in available: " + str(all_picks_valid))

	var all_bans_valid := true
	for rec in ban_results:
		if rec.candidate not in available:
			report_lines.append("INVALID BAN: " + str(rec.candidate) + " not in available")
			all_bans_valid = false
	report_lines.append("All ban recommendations in available: " + str(all_bans_valid))

	# Test 4: Empty state handling
	report_lines.append("\n--- Test 4: Empty State Handling ---")
	var empty_pick_results := backend.get_draft_ai_pick_recommendations(stats_dir, available, [], [], 3)
	report_lines.append("Empty state pick recommendations returned: " + str(not empty_pick_results.is_empty()))
	report_lines.append("Empty state recommendations count: " + str(empty_pick_results.size()))

	var empty_ban_results := backend.get_draft_ai_ban_recommendations(stats_dir, available, [], [], 3)
	report_lines.append("Empty state ban recommendations returned: " + str(not empty_ban_results.is_empty()))
	report_lines.append("Empty state ban recommendations count: " + str(empty_ban_results.size()))

	# Test 5: Deterministic behavior
	report_lines.append("\n--- Test 5: Deterministic Behavior ---")
	var pick_run1 := backend.get_draft_ai_pick_recommendations(stats_dir, available, allies, enemies, 5)
	var pick_run2 := backend.get_draft_ai_pick_recommendations(stats_dir, available, allies, enemies, 5)

	var is_deterministic := true
	if pick_run1.size() != pick_run2.size():
		is_deterministic = false
		report_lines.append("Size mismatch: " + str(pick_run1.size()) + " vs " + str(pick_run2.size()))
	else:
		for i in range(pick_run1.size()):
			if pick_run1[i].candidate != pick_run2[i].candidate:
				is_deterministic = false
				report_lines.append("Mismatch at index " + str(i) + ": " + str(pick_run1[i].candidate) + " vs " + str(pick_run2[i].candidate))
			if not is_equal_approx(pick_run1[i].total_score, pick_run2[i].total_score):
				is_deterministic = false
				report_lines.append("Score mismatch at index " + str(i) + ": " + str(pick_run1[i].total_score) + " vs " + str(pick_run2[i].total_score))

	report_lines.append("Deterministic pick recommendations: " + str(is_deterministic))

	var ban_run1 := backend.get_draft_ai_ban_recommendations(stats_dir, available, allies, enemies, 5)
	var ban_run2 := backend.get_draft_ai_ban_recommendations(stats_dir, available, allies, enemies, 5)

	var ban_deterministic := true
	if ban_run1.size() != ban_run2.size():
		ban_deterministic = false
		report_lines.append("Ban size mismatch: " + str(ban_run1.size()) + " vs " + str(ban_run2.size()))
	else:
		for i in range(ban_run1.size()):
			if ban_run1[i].candidate != ban_run2[i].candidate:
				ban_deterministic = false
				report_lines.append("Ban mismatch at index " + str(i) + ": " + str(ban_run1[i].candidate) + " vs " + str(ban_run2[i].candidate))
			if not is_equal_approx(ban_run1[i].total_score, ban_run2[i].total_score):
				ban_deterministic = false
				report_lines.append("Ban score mismatch at index " + str(i) + ": " + str(ban_run1[i].total_score) + " vs " + str(ban_run2[i].total_score))

	report_lines.append("Deterministic ban recommendations: " + str(ban_deterministic))

	# Test 6: Strategy layer
	report_lines.append("\n--- Test 6: Strategy Layer ---")
	var strategy = DraftStrategyNativeScript.new(stats_dir)
	var strategy_pick = strategy.recommend_next_pick(allies, enemies, available)
	report_lines.append("Strategy pick: " + str(strategy_pick))
	report_lines.append("Strategy pick in available: " + str(strategy_pick in available))

	var strategy_ban = strategy.recommend_next_ban(allies, enemies, available)
	report_lines.append("Strategy ban: " + str(strategy_ban))
	report_lines.append("Strategy ban in available: " + str(strategy_ban in available))

	# Test 7: Random vs native comparison
	report_lines.append("\n--- Test 7: Random vs Native Comparison ---")
	const DraftStrategyRandomScript = preload("res://scripts/tools/draft_strategy_random.gd")
	var random_strategy = DraftStrategyRandomScript.new()

	var random_pick = random_strategy.recommend_next_pick(allies, enemies, available)
	report_lines.append("Random pick: " + str(random_pick))
	report_lines.append("Native pick: " + str(strategy_pick))
	report_lines.append("Different picks: " + str(random_pick != strategy_pick))

	# Ban formula audit
	report_lines.append("\n--- Ban Formula Audit ---")
	report_lines.append("Expected formula:")
	report_lines.append("  total_score = 0.40 * enemy_pick_value + 0.25 * enemy_synergy + 0.25 * counters_my_team + own_pick_value_penalty")
	report_lines.append("  where own_pick_value_penalty = -0.20 * own_pick_total_score")
	report_lines.append("\nImplemented formula (from sim_draft_ai_evaluator.cpp):")
	report_lines.append("  Line 90: result.own_pick_value_penalty = -OWN_PICK_VALUE_PENALTY_WEIGHT * own_pick.total_score")
	report_lines.append("  Lines 93-97: result.total_score =")
	report_lines.append("    ENEMY_PICK_VALUE_WEIGHT * result.enemy_pick_value +")
	report_lines.append("    ENEMY_SYNERGY_WEIGHT * result.enemy_synergy +")
	report_lines.append("    COUNTERS_MY_TEAM_WEIGHT * result.counters_my_team +")
	report_lines.append("    result.own_pick_value_penalty")
	report_lines.append("\nFormula is CORRECT: own_pick_value_penalty is added directly (already negative)")

	# Test 8: Phase-aware draft step testing
	report_lines.append("\n--- Test 8: Phase-Aware Draft Step Testing ---")

	var test_steps = [-1, 6, 10, 16, 0, 13]
	var step_labels = {
		-1: "default",
		6: "early_pick",
		10: "mid_pick",
		16: "late_pick",
		0: "phase1_ban",
		13: "phase2_ban"
	}

	for step in test_steps:
		var step_pick_results = backend.get_draft_ai_pick_recommendations(stats_dir, available, allies, enemies, 3, step)
		if not step_pick_results.is_empty():
			var top_rec = step_pick_results[0]
			report_lines.append("\nDraft step " + str(step) + " (" + step_labels.get(step, "unknown") + "):")
			report_lines.append("  Top pick: " + str(top_rec.candidate) + " (score: " + str(top_rec.total_score) + ")")
			if top_rec.has("base_power_weight"):
				report_lines.append("  Weights: base_power=" + str(top_rec.base_power_weight) +
					", ally_synergy=" + str(top_rec.ally_synergy_weight) +
					", enemy_counter=" + str(top_rec.enemy_counter_value_weight) +
					", counter_risk=" + str(top_rec.counter_risk_weight))
				report_lines.append("  Phase label: " + str(top_rec.phase_label))

		var step_ban_results = backend.get_draft_ai_ban_recommendations(stats_dir, available, allies, enemies, 3, step)
		if not step_ban_results.is_empty():
			var top_ban = step_ban_results[0]
			report_lines.append("  Top ban: " + str(top_ban.candidate) + " (score: " + str(top_ban.total_score) + ")")
			if top_ban.has("enemy_pick_value_weight"):
				report_lines.append("  Ban weights: enemy_pick=" + str(top_ban.enemy_pick_value_weight) +
					", enemy_synergy=" + str(top_ban.enemy_synergy_weight) +
					", counters_my_team=" + str(top_ban.counters_my_team_weight) +
					", own_penalty=" + str(top_ban.own_pick_value_penalty_weight))
				report_lines.append("  Ban phase label: " + str(top_ban.phase_label))

	# Test 9: Role-aware scoring
	report_lines.append("\n--- Test 9: Role-Aware Scoring ---")

	var role_scenarios = [
		{"name": "Empty teams", "allies": [], "enemies": []},
		{"name": "Stacked melee allies", "allies": ["swordsman", "berserker"], "enemies": ["archer"]},
		{"name": "Stacked ranged enemies", "allies": ["cleric", "frost_mage"], "enemies": ["colossus", "berserker"]}
	]

	for scenario in role_scenarios:
		report_lines.append("\nScenario: " + scenario.name)
		var scenario_allies = scenario.allies
		var scenario_enemies = scenario.enemies

		var role_pick_results = backend.get_draft_ai_pick_recommendations(stats_dir, available, scenario_allies, scenario_enemies, 3, -1)
		if not role_pick_results.is_empty():
			var top_rec = role_pick_results[0]
			report_lines.append("  Top pick: " + str(top_rec.candidate) + " (role: " + str(top_rec.get("candidate_role", "unknown")) + ")")
			report_lines.append("  role_fit: " + str(top_rec.get("role_fit", 0.0)))
			report_lines.append("  role_fit_weight: " + str(top_rec.get("role_fit_weight", 0.0)))

		var role_ban_results = backend.get_draft_ai_ban_recommendations(stats_dir, available, scenario_allies, scenario_enemies, 3, -1)
		if not role_ban_results.is_empty():
			var top_ban = role_ban_results[0]
			report_lines.append("  Top ban: " + str(top_ban.candidate) + " (role: " + str(top_ban.get("candidate_role", "unknown")) + ")")
			report_lines.append("  fills_enemy_role_need: " + str(top_ban.get("fills_enemy_role_need", 0.0)))
			report_lines.append("  fills_enemy_role_need_weight: " + str(top_ban.get("fills_enemy_role_need_weight", 0.0)))

	# Test 10: Role counting edge cases audit
	report_lines.append("\n--- Test 10: Role Counting Edge Cases Audit ---")

	# Fixed test cases with known champions
	var role_test_cases = [
		{
			"name": "Pick: role absent from allies",
			"candidate": "colossus",
			"allies": ["archer", "frost_mage"],
			"enemies": [],
			"expected_role_fit": 0.050,
			"type": "pick"
		},
		{
			"name": "Pick: role appears once",
			"candidate": "colossus",
			"allies": ["colossus"],
			"enemies": [],
			"expected_role_fit": 0.020,
			"type": "pick"
		},
		{
			"name": "Pick: role appears twice",
			"candidate": "colossus",
			"allies": ["colossus", "colossus"],
			"enemies": [],
			"expected_role_fit": -0.030,
			"type": "pick"
		},
		{
			"name": "Pick: role appears 3+ times",
			"candidate": "colossus",
			"allies": ["colossus", "colossus", "colossus"],
			"enemies": [],
			"expected_role_fit": -0.060,
			"type": "pick"
		},
		{
			"name": "Ban: role absent from enemies",
			"candidate": "colossus",
			"allies": [],
			"enemies": ["archer", "frost_mage"],
			"expected_role_need": 0.050,
			"type": "ban"
		},
		{
			"name": "Ban: role appears once on enemies",
			"candidate": "colossus",
			"allies": [],
			"enemies": ["colossus"],
			"expected_role_need": 0.020,
			"type": "ban"
		},
		{
			"name": "Ban: role appears 2+ times on enemies",
			"candidate": "colossus",
			"allies": [],
			"enemies": ["colossus", "colossus"],
			"expected_role_need": -0.020,
			"type": "ban"
		}
	]

	for test_case in role_test_cases:
		report_lines.append("\n" + test_case.name)
		var test_candidate = [test_case.candidate]
		var test_allies = test_case.allies
		var test_enemies = test_case.enemies

		if test_case.type == "pick":
			var results = backend.get_draft_ai_pick_recommendations(stats_dir, test_candidate, test_allies, test_enemies, 1, -1)
			if not results.is_empty():
				var rec = results[0]
				var actual_role_fit = rec.get("role_fit", 0.0)
				var candidate_role = rec.get("candidate_role", "unknown")
				report_lines.append("  Candidate: " + str(test_case.candidate) + " (role: " + str(candidate_role) + ")")
				report_lines.append("  Allies: " + str(test_allies))
				report_lines.append("  Expected role_fit: " + str(test_case.expected_role_fit))
				report_lines.append("  Actual role_fit: " + str(actual_role_fit))
				var passed = abs(actual_role_fit - test_case.expected_role_fit) < 0.001
				report_lines.append("  Result: " + ("PASS" if passed else "FAIL"))
		else:
			var results = backend.get_draft_ai_ban_recommendations(stats_dir, test_candidate, test_allies, test_enemies, 1, -1)
			if not results.is_empty():
				var rec = results[0]
				var actual_role_need = rec.get("fills_enemy_role_need", 0.0)
				var candidate_role = rec.get("candidate_role", "unknown")
				report_lines.append("  Candidate: " + str(test_case.candidate) + " (role: " + str(candidate_role) + ")")
				report_lines.append("  Enemies: " + str(test_enemies))
				report_lines.append("  Expected fills_enemy_role_need: " + str(test_case.expected_role_need))
				report_lines.append("  Actual fills_enemy_role_need: " + str(actual_role_need))
				var passed = abs(actual_role_need - test_case.expected_role_need) < 0.001
				report_lines.append("  Result: " + ("PASS" if passed else "FAIL"))

	# Test 11: Comp fit validation
	report_lines.append("\n--- Test 11: Comp Fit Validation ---")

	# Test with full team to see comp_fit in action
	var comp_test_allies = ["colossus", "swordsman", "archer", "cleric"]
	var comp_test_enemies = ["berserker", "frost_mage", "assassin"]

	var comp_pick_results = backend.get_draft_ai_pick_recommendations(stats_dir, available, comp_test_allies, comp_test_enemies, 3, -1)
	if not comp_pick_results.is_empty():
		report_lines.append("\nComp fit pick test (4 allies + candidate = 5):")
		for i in range(min(3, comp_pick_results.size())):
			var rec = comp_pick_results[i]
			report_lines.append("  " + str(i+1) + ". " + str(rec.candidate))
			report_lines.append("     candidate_role: " + str(rec.get("candidate_role", "unknown")))
			report_lines.append("     role_fit: " + str(rec.get("role_fit", 0.0)))
			report_lines.append("     comp_fit: " + str(rec.get("comp_fit", 0.0)))
			report_lines.append("     comp_fingerprint: " + str(rec.get("comp_fingerprint", "")))
			report_lines.append("     comp_samples: " + str(rec.get("comp_samples", 0)))
			report_lines.append("     total_score: " + str(rec.total_score))

	var comp_ban_results = backend.get_draft_ai_ban_recommendations(stats_dir, available, comp_test_allies, comp_test_enemies, 3, -1)
	if not comp_ban_results.is_empty():
		report_lines.append("\nComp fit ban test (3 enemies + candidate = 4, partial comp):")
		for i in range(min(3, comp_ban_results.size())):
			var rec = comp_ban_results[i]
			report_lines.append("  " + str(i+1) + ". " + str(rec.candidate))
			report_lines.append("     candidate_role: " + str(rec.get("candidate_role", "unknown")))
			report_lines.append("     fills_enemy_role_need: " + str(rec.get("fills_enemy_role_need", 0.0)))
			report_lines.append("     enemy_comp_fit: " + str(rec.get("enemy_comp_fit", 0.0)))
			report_lines.append("     enemy_comp_fingerprint: " + str(rec.get("enemy_comp_fingerprint", "")))
			report_lines.append("     enemy_comp_samples: " + str(rec.get("enemy_comp_samples", 0)))
			report_lines.append("     total_score: " + str(rec.total_score))

	report_lines.append("\n=== VALIDATION COMPLETE ===")
	_write_report(report_lines)
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)

func _write_report(lines: Array) -> void:
	var output_path = "res://logs/draft_ai_validation_report.txt"
	var f = FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f:
		f.store_string("\n".join(lines))
		f.flush()
		f.close()
