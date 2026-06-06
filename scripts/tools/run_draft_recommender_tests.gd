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
	quit()
