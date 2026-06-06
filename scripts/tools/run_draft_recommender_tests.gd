extends SceneTree

func _initialize() -> void:
	print("Starting draft recommender tests...")
	var core := TeamfightSimulationCore.new()
	var available := [
		"archer",
		"artillery",
		"berserker",
		"cleric",
		"colossus",
		"disarmer",
		"earthbender",
		"swordsman",
	]
	var tests := [
		{
			"name": "baseline_no_context",
			"allies": [],
			"enemies": [],
			"available": available,
		},
		{
			"name": "counter_context",
			"allies": ["cleric", "colossus"],
			"enemies": ["berserker", "swordsman", "disarmer"],
			"available": available,
		},
		{
			"name": "synergy_context",
			"allies": ["archer", "artillery", "cleric"],
			"enemies": ["earthbender", "colossus"],
			"available": available,
		},
	]

	for i in tests.size():
		var test: Dictionary = tests[i]
		print("========================")
		print("TEST CASE %d: %s" % [i + 1, test["name"]])
		print("========================")
		core.debug_print_draft_recommendations(
			test["allies"],
			test["enemies"],
			test["available"],
			3,
			"res://stats_output",
			0.50,
			0.25,
			0.25,
			true
		)
		print("Top 3 summary:")
		var top_names: Array = core.get_draft_recommendation_names(
			test["allies"],
			test["enemies"],
			test["available"],
			3,
			"res://stats_output",
			0.50,
			0.25,
			0.25
		)
		print("  " + ", ".join(top_names))

	print("========================")
	print("STABILITY CHECK: counter_context weight shift")
	print("========================")
	print("Original top 3")
	core.debug_print_draft_recommendations(
		tests[1]["allies"],
		tests[1]["enemies"],
		tests[1]["available"],
		3,
		"res://stats_output",
		0.50,
		0.25,
		0.25,
		true
	)
	print("Shifted top 3")
	core.debug_print_draft_recommendations(
		tests[1]["allies"],
		tests[1]["enemies"],
		tests[1]["available"],
		3,
		"res://stats_output",
		0.35,
		0.20,
		0.45,
		true
	)

	print("========================")
	print("SIGNAL INFLUENCE ANALYSIS")
	print("========================")
	for i in tests.size():
		var test: Dictionary = tests[i]
		print("\nTest case: %s" % test["name"])
		var candidate: String = test["available"][0]  # Test first available champion
		var influence = core.analyze_draft_signal_influence(
			[candidate],
			test["allies"],
			test["enemies"],
			"res://stats_output",
			0.50,
			0.25,
			0.25
		)
		print("  Candidate: %s" % candidate)
		print("  Full score: %.6f" % influence.get("full_score", 0.0))
		print("  Base only score: %.6f" % influence.get("base_only_score", 0.0))
		print("  Synergy removed score: %.6f" % influence.get("synergy_removed_score", 0.0))
		print("  Matchup removed score: %.6f" % influence.get("matchup_removed_score", 0.0))
		print("  Base only delta: %.6f" % influence.get("base_only_delta", 0.0))
		print("  Synergy removed delta: %.6f" % influence.get("synergy_removed_delta", 0.0))
		print("  Matchup removed delta: %.6f" % influence.get("matchup_removed_delta", 0.0))

	print("\n========================")
	print("CONTROLLED EVALUATION")
	print("========================")
	var total_synergy_impact := 0.0
	var total_matchup_impact := 0.0
	var total_synergy_overlap := 0
	var total_matchup_overlap := 0

	for i in tests.size():
		var test: Dictionary = tests[i]
		print("\nCASE: %s" % test["name"])
		var eval = core.run_controlled_draft_evaluation(
			test["allies"],
			test["enemies"],
			test["available"],
			"res://stats_output",
			0.50,
			0.25,
			0.25
		)
		var synergy_impact = eval.get("avg_synergy_impact", 0.0)
		var matchup_impact = eval.get("avg_matchup_impact", 0.0)
		var synergy_overlap = eval.get("top3_overlap_synergy_removed", 0)
		var matchup_overlap = eval.get("top3_overlap_matchup_removed", 0)

		print("Synergy impact avg: %.6f" % synergy_impact)
		print("Matchup impact avg: %.6f" % matchup_impact)
		print("\nTop3 stability:")
		print("- vs synergy removed: %d/3" % synergy_overlap)
		print("- vs matchup removed: %d/3" % matchup_overlap)

		total_synergy_impact += synergy_impact
		total_matchup_impact += matchup_impact
		total_synergy_overlap += synergy_overlap
		total_matchup_overlap += matchup_overlap

	print("\n========================")
	print("AGGREGATED SUMMARY")
	print("========================")
	print("Mean synergy impact: %.6f" % (total_synergy_impact / tests.size()))
	print("Mean matchup impact: %.6f" % (total_matchup_impact / tests.size()))
	print("Mean top3 overlap vs synergy removed: %.2f/3" % (float(total_synergy_overlap) / tests.size()))
	print("Mean top3 overlap vs matchup removed: %.2f/3" % (float(total_matchup_overlap) / tests.size()))

	print("\n========================")
	print("STRESS TESTING")
	print("========================")
	
	# Store scenario reports for aggregated summary
	var scenario_reports := []
	
	for i in tests.size():
		var test: Dictionary = tests[i]
		print("\n========================")
		print("SCENARIO: %s" % test["name"])
		print("========================")
		print("")  # Force flush
		
		var report = core.run_stress_test_with_perturbations(
			test["allies"],
			test["enemies"],
			test["available"],
			"res://stats_output",
			42,  # Fixed seed for reproducibility
			30,  # Number of perturbed scenarios
			0.50,
			0.25,
			0.25,
			1.2,
			1.2
		)
		
		scenario_reports.append({"name": test["name"], "report": report})
		
		# Print baseline reference
		print("\nBaseline ranking:")
		print("  Top-1: %s" % report.get("baseline_top1", ""))
		print("  Top-3: %s" % ", ".join(report.get("baseline_top3", [])))
		
		# Print global distribution metrics
		print("\nGlobal score distribution:")
		print("  Min: %.6f" % report.get("score_min", 0.0))
		print("  Max: %.6f" % report.get("score_max", 0.0))
		print("  Mean: %.6f" % report.get("score_mean", 0.0))
		print("  P50: %.6f" % report.get("score_p50", 0.0))
		print("  P90: %.6f" % report.get("score_p90", 0.0))
		
		# Print rank volatility
		print("\nRank volatility:")
		print("  Avg rank change: %.6f" % report.get("avg_rank_change", 0.0))
		print("  Max rank swing: %d" % report.get("max_rank_swing", 0))
		
		# Print context sensitivity
		print("\nContext sensitivity:")
		print("  Avg score delta from baseline: %.6f" % report.get("context_sensitivity", 0.0))
		
		# Print winner stability
		print("\nWinner stability:")
		print("  Top-1 stability rate: %.2f%%" % report.get("top1_stability_rate", 0.0))
		print("  Top-3 stability rate: %.2f%%" % report.get("top3_stability_rate", 0.0))
		
		# Print per-candidate statistics
		print("\nPer-candidate statistics:")
		var candidate_stats = report.get("candidate_stats", [])
		for stats in candidate_stats:
			print("  %s:" % stats.get("champion", ""))
			print("    Mean score: %.6f (stddev: %.6f)" % [stats.get("mean_score", 0.0), stats.get("score_stddev", 0.0)])
			print("    Mean rank: %.2f (stddev: %.6f)" % [stats.get("mean_rank", 0.0), stats.get("rank_stddev", 0.0)])
			print("    Max rank swing: %d" % stats.get("max_rank_swing", 0))
			print("    Baseline: score=%.6f, rank=%d" % [stats.get("baseline_score", 0.0), stats.get("baseline_rank", 0)])
	
	# Aggregated stress summary
	print("\n========================")
	print("AGGREGATED STRESS SUMMARY")
	print("========================")
	
	if scenario_reports.size() > 0:
		var most_stable = null
		var most_volatile = null
		var highest_sensitivity = null
		
		var best_stability_score = -1.0
		var worst_stability_score = -1.0
		var best_sensitivity = -1.0
		
		for scenario in scenario_reports:
			var report = scenario["report"]
			var name = scenario["name"]
			
			# Composite stability score: higher = more stable
			var stability_score = (report.get("top1_stability_rate", 0.0) + report.get("top3_stability_rate", 0.0)) / 2.0
			stability_score -= report.get("avg_rank_change", 0.0) * 10.0  # Penalize rank volatility
			stability_score -= report.get("context_sensitivity", 0.0) * 10.0  # Penalize score sensitivity
			
			var sensitivity = report.get("context_sensitivity", 0.0)
			
			if most_stable == null or stability_score > best_stability_score:
				most_stable = name
				best_stability_score = stability_score
			
			if most_volatile == null or stability_score < worst_stability_score:
				most_volatile = name
				worst_stability_score = stability_score
			
			if highest_sensitivity == null or sensitivity > best_sensitivity:
				highest_sensitivity = name
				best_sensitivity = sensitivity
		
		print("\nMost stable scenario: %s" % most_stable)
		print("Most volatile scenario: %s" % most_volatile)
		print("Highest sensitivity scenario: %s" % highest_sensitivity)
		
		# Overall system stability score
		var avg_top1_stability = 0.0
		var avg_top3_stability = 0.0
		var avg_rank_change = 0.0
		var avg_sensitivity = 0.0
		
		for scenario in scenario_reports:
			var report = scenario["report"]
			avg_top1_stability += report.get("top1_stability_rate", 0.0)
			avg_top3_stability += report.get("top3_stability_rate", 0.0)
			avg_rank_change += report.get("avg_rank_change", 0.0)
			avg_sensitivity += report.get("context_sensitivity", 0.0)
		
		avg_top1_stability /= scenario_reports.size()
		avg_top3_stability /= scenario_reports.size()
		avg_rank_change /= scenario_reports.size()
		avg_sensitivity /= scenario_reports.size()
		
		var overall_stability = (avg_top1_stability + avg_top3_stability) / 2.0
		overall_stability -= avg_rank_change * 10.0
		overall_stability -= avg_sensitivity * 10.0
		
		print("\nOverall system stability score: %.2f" % overall_stability)
		print("  (Higher = more stable, based on winner stability, rank volatility, and context sensitivity)")

	quit()
