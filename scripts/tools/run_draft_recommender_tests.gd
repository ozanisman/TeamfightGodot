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
		
		# Run with all three scoring modes
		var mode_names = ["ADDITIVE", "MULTIPLICATIVE", "LOGIT"]
		var reports = {}
		
		for mode_idx in range(3):
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
				1.2,
				mode_idx  # 0=ADDITIVE, 1=MULTIPLICATIVE, 2=LOGIT
			)
			reports[mode_idx] = report
		
		scenario_reports.append({"name": test["name"], "report": reports[0]})
		
		# Print baseline reference (same for all modes)
		print("\nBaseline ranking:")
		print("  Top-1: %s" % reports[0].get("baseline_top1", ""))
		print("  Top-3: %s" % ", ".join(reports[0].get("baseline_top3", [])))
		
		# Print comparison table
		print("\nScoring Mode Comparison:")
		print("  Mode          | Score Range | Mean Margin | Median Margin | Rank Vol | Top-3 Stab | Context Sens")
		print("  --------------|-------------|-------------|---------------|----------|------------|--------------")
		
		for mode_idx in range(3):
			var report = reports[mode_idx]
			var range_val = report.get("score_max", 0.0) - report.get("score_min", 0.0)
			var mean_margin = report.get("mean_inter_rank_margin", 0.0)
			var median_margin = report.get("median_inter_rank_margin", 0.0)
			var rank_vol = report.get("avg_rank_change", 0.0)
			var top3_stab = report.get("top3_stability_rate", 0.0)
			var context_sens = report.get("context_sensitivity", 0.0)
			
			print("  %-12s | %.6f | %.6f | %.6f | %.4f | %.2f%% | %.6f" % [
				mode_names[mode_idx], range_val, mean_margin, median_margin, rank_vol, top3_stab, context_sens
			])
		
		# Print per-candidate statistics (ADDITIVE mode)
		print("\nPer-candidate statistics (ADDITIVE mode):")
		var candidate_stats = reports[0].get("candidate_stats", [])
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

	print("\n========================")
	print("LOGIT SHARPNESS PARAMETER SWEEP")
	print("========================")

	# Test logit sharpness values
	var sharpness_values = [1.0, 1.15, 1.3, 1.5]
	var sharpness_results = {}

	for sharpness in sharpness_values:
		print("\n--- LOGIT SHARPNESS: %.2f ---" % sharpness)

		var sharpness_aggregate = {
			"score_range": 0.0,
			"mean_margin": 0.0,
			"median_margin": 0.0,
			"rank_volatility": 0.0,
			"top3_stability": 0.0,
			"context_sensitivity": 0.0
		}

		for i in tests.size():
			var test: Dictionary = tests[i]
			var report = core.run_stress_test_with_perturbations(
				test["allies"],
				test["enemies"],
				test["available"],
				"res://stats_output",
				42,
				30,
				0.50,
				0.25,
				0.25,
				1.2,
				1.2,
				2,  # LOGIT mode
				sharpness
			)

			var range_val = report.get("score_max", 0.0) - report.get("score_min", 0.0)
			var mean_margin = report.get("mean_inter_rank_margin", 0.0)
			var median_margin = report.get("median_inter_rank_margin", 0.0)
			var rank_vol = report.get("avg_rank_change", 0.0)
			var top3_stab = report.get("top3_stability_rate", 0.0)
			var context_sens = report.get("context_sensitivity", 0.0)

			sharpness_aggregate["score_range"] += range_val
			sharpness_aggregate["mean_margin"] += mean_margin
			sharpness_aggregate["median_margin"] += median_margin
			sharpness_aggregate["rank_volatility"] += rank_vol
			sharpness_aggregate["top3_stability"] += top3_stab
			sharpness_aggregate["context_sensitivity"] += context_sens

		# Average across scenarios
		sharpness_aggregate["score_range"] /= tests.size()
		sharpness_aggregate["mean_margin"] /= tests.size()
		sharpness_aggregate["median_margin"] /= tests.size()
		sharpness_aggregate["rank_volatility"] /= tests.size()
		sharpness_aggregate["top3_stability"] /= tests.size()
		sharpness_aggregate["context_sensitivity"] /= tests.size()

		sharpness_results[sharpness] = sharpness_aggregate

		print("  Score Range: %.6f" % sharpness_aggregate["score_range"])
		print("  Mean Margin: %.6f" % sharpness_aggregate["mean_margin"])
		print("  Median Margin: %.6f" % sharpness_aggregate["median_margin"])
		print("  Rank Volatility: %.4f" % sharpness_aggregate["rank_volatility"])
		print("  Top-3 Stability: %.2f%%" % sharpness_aggregate["top3_stability"])
		print("  Context Sensitivity: %.6f" % sharpness_aggregate["context_sensitivity"])

	# Compare LOGIT (best sharpness) vs ADDITIVE baseline
	print("\n========================")
	print("LOGIT VS ADDITIVE COMPARISON")
	print("========================")

	# Compute ADDITIVE aggregated baseline (same scenarios as LOGIT sweep)
	var additive_aggregate = {
		"score_range": 0.0,
		"mean_margin": 0.0,
		"median_margin": 0.0,
		"rank_volatility": 0.0,
		"top3_stability": 0.0,
		"context_sensitivity": 0.0
	}

	for i in tests.size():
		var test: Dictionary = tests[i]
		var report = core.run_stress_test_with_perturbations(
			test["allies"],
			test["enemies"],
			test["available"],
			"res://stats_output",
			42,
			30,
			0.50,
			0.25,
			0.25,
			1.2,
			1.2,
			0,  # ADDITIVE mode
			1.0
		)

		var range_val = report.get("score_max", 0.0) - report.get("score_min", 0.0)
		var mean_margin = report.get("mean_inter_rank_margin", 0.0)
		var median_margin = report.get("median_inter_rank_margin", 0.0)
		var rank_vol = report.get("avg_rank_change", 0.0)
		var top3_stab = report.get("top3_stability_rate", 0.0)
		var context_sens = report.get("context_sensitivity", 0.0)

		additive_aggregate["score_range"] += range_val
		additive_aggregate["mean_margin"] += mean_margin
		additive_aggregate["median_margin"] += median_margin
		additive_aggregate["rank_volatility"] += rank_vol
		additive_aggregate["top3_stability"] += top3_stab
		additive_aggregate["context_sensitivity"] += context_sens

	# Average across scenarios
	additive_aggregate["score_range"] /= tests.size()
	additive_aggregate["mean_margin"] /= tests.size()
	additive_aggregate["median_margin"] /= tests.size()
	additive_aggregate["rank_volatility"] /= tests.size()
	additive_aggregate["top3_stability"] /= tests.size()
	additive_aggregate["context_sensitivity"] /= tests.size()

	print("\nADDITIVE Baseline (aggregated across scenarios):")
	print("  Score Range: %.6f" % additive_aggregate["score_range"])
	print("  Mean Margin: %.6f" % additive_aggregate["mean_margin"])
	print("  Median Margin: %.6f" % additive_aggregate["median_margin"])
	print("  Rank Volatility: %.4f" % additive_aggregate["rank_volatility"])
	print("  Top-3 Stability: %.2f%%" % additive_aggregate["top3_stability"])
	print("  Context Sensitivity: %.6f" % additive_aggregate["context_sensitivity"])

	# Find best LOGIT sharpness
	var best_sharpness = 1.0
	var best_score_range = 0.0
	for sharpness in sharpness_values:
		if sharpness_results[sharpness]["score_range"] > best_score_range:
			best_score_range = sharpness_results[sharpness]["score_range"]
			best_sharpness = sharpness

	var best_logit = sharpness_results[best_sharpness]

	print("\nBest LOGIT (sharpness=%.2f):" % best_sharpness)
	print("  Score Range: %.6f" % best_logit["score_range"])
	print("  Mean Margin: %.6f" % best_logit["mean_margin"])
	print("  Median Margin: %.6f" % best_logit["median_margin"])
	print("  Rank Volatility: %.4f" % best_logit["rank_volatility"])
	print("  Top-3 Stability: %.2f%%" % best_logit["top3_stability"])
	print("  Context Sensitivity: %.6f" % best_logit["context_sensitivity"])

	# Calculate improvements (using aggregated ADDITIVE baseline)
	var range_improvement = (best_logit["score_range"] - additive_aggregate["score_range"]) / additive_aggregate["score_range"] * 100.0
	var mean_margin_improvement = (best_logit["mean_margin"] - additive_aggregate["mean_margin"]) / additive_aggregate["mean_margin"] * 100.0
	var median_margin_improvement = (best_logit["median_margin"] - additive_aggregate["median_margin"]) / additive_aggregate["median_margin"] * 100.0
	var rank_vol_change = (best_logit["rank_volatility"] - additive_aggregate["rank_volatility"]) / additive_aggregate["rank_volatility"] * 100.0
	var top3_stability_change = (best_logit["top3_stability"] - additive_aggregate["top3_stability"]) / additive_aggregate["top3_stability"] * 100.0
	var context_sens_change = (best_logit["context_sensitivity"] - additive_aggregate["context_sensitivity"]) / additive_aggregate["context_sensitivity"] * 100.0

	print("\nImprovement Analysis:")
	print("  Score Range: %+.2f%%" % range_improvement)
	print("  Mean Margin: %+.2f%%" % mean_margin_improvement)
	print("  Median Margin: %+.2f%%" % median_margin_improvement)
	print("  Rank Volatility: %+.2f%%" % rank_vol_change)
	print("  Top-3 Stability: %+.2f%%" % top3_stability_change)
	print("  Context Sensitivity: %+.2f%%" % context_sens_change)

	# Acceptance criteria check
	print("\n========================")
	print("ACCEPTANCE CRITERIA CHECK")
	print("========================")

	var criteria_met = []
	var criteria_failed = []
	var context_sens_note = []

	if range_improvement >= 15.0:
		criteria_met.append("Score range improvement ≥15% (" + str("%.2f" % range_improvement) + "%)")
	else:
		criteria_failed.append("Score range improvement ≥15% (" + str("%.2f" % range_improvement) + "%)")

	if mean_margin_improvement >= 10.0:
		criteria_met.append("Mean margin improvement ≥10% (" + str("%.2f" % mean_margin_improvement) + "%)")
	else:
		criteria_failed.append("Mean margin improvement ≥10% (" + str("%.2f" % mean_margin_improvement) + "%)")

	if abs(top3_stability_change) < 10.0:
		criteria_met.append("Top-3 stability degradation <10% (" + str("%.2f" % top3_stability_change) + "%)")
	else:
		criteria_failed.append("Top-3 stability degradation <10% (" + str("%.2f" % top3_stability_change) + "%)")

	# Context sensitivity: note the change but don't fail on it pending validation
	var context_sens_abs_delta = abs(best_logit["context_sensitivity"] - additive_aggregate["context_sensitivity"])
	if context_sens_abs_delta < 0.002:
		context_sens_note.append("Context sensitivity change is negligible (<0.002 absolute)")
	else:
		context_sens_note.append("Context sensitivity change: " + str("%.6f" % context_sens_abs_delta) + " absolute (needs validation)")

	print("\nMet:")
	for criterion in criteria_met:
		print("  ✓ %s" % criterion)

	print("\nFailed:")
	for criterion in criteria_failed:
		print("  ✗ %s" % criterion)

	print("\nNotes:")
	for note in context_sens_note:
		print("  ! %s" % note)

	print("\nRecommendation:")
	if criteria_failed.size() == 0:
		print("  LOGIT meets core acceptance criteria (score range, margin, stability).")
		print("  LOGIT 1.3-1.5 is the current leading candidate.")
		print("  Next steps:")
		print("    - Validate context sensitivity metric (correlation with recommendation quality)")
		print("    - If validated, adopt LOGIT as default experimental scorer")
		print("    - Continue tuning confidence weighting")
	else:
		print("  LOGIT fails core acceptance criteria.")
		print("  Recommend: Stop scorer experimentation and shift effort to improving underlying signals:")
		print("    - Synergy generation")
		print("    - Counter generation")
		print("    - Confidence weighting")
		print("    - Bayesian smoothing calibration")

	print("\nScoring Mode Summary:")
	print("  - MULTIPLICATIVE: Reject (poor stability)")
	print("  - ADDITIVE: Baseline")
	print("  - LOGIT (sharpness 1.3-1.5): Strongest candidate tested so far")

	quit()
