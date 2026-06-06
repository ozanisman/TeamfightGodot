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

	quit()
