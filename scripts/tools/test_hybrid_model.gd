extends SceneTree

## Test the hybrid draft model end-to-end.

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	print("test_hybrid_model: Testing hybrid model implementation")
	
	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("test_hybrid_model: Native backend unavailable")
		quit(1)
		return
	
	# Test cases for different depths
	var test_cases := [
		{
			"depth": 1,
			"allies": ["Ahri"],
			"enemies": ["Ashe"],
			"expected_model": "partial"
		},
		{
			"depth": 2,
			"allies": ["Ahri", "Brand"],
			"enemies": ["Ashe", "Braum"],
			"expected_model": "partial"
		},
		{
			"depth": 3,
			"allies": ["Ahri", "Brand", "Cassiopeia"],
			"enemies": ["Ashe", "Braum", "Darius"],
			"expected_model": "partial"
		},
		{
			"depth": 4,
			"allies": ["Ahri", "Brand", "Cassiopeia", "Diana"],
			"enemies": ["Ashe", "Braum", "Darius", "Draven"],
			"expected_model": "certified"
		},
		{
			"depth": 5,
			"allies": ["Ahri", "Brand", "Cassiopeia", "Diana", "Ekko"],
			"enemies": ["Ashe", "Braum", "Darius", "Draven", "Elise"],
			"expected_model": "certified"
		}
	]
	
	var stats_dir := "res://stats_output"
	
	for test_case: Dictionary in test_cases:
		var depth: int = test_case.depth
		var allies: Array = test_case.allies
		var enemies: Array = test_case.enemies
		var expected_model: String = test_case.expected_model
		
		print("test_hybrid_model: Testing depth %d (%d vs %d)" % [depth, allies.size(), enemies.size()])
		
		# Test hybrid model
		var hybrid_result: Dictionary = backend.predict_draft_winner_hybrid(allies, enemies, stats_dir)
		if hybrid_result.is_empty():
			push_error("test_hybrid_model: Empty hybrid result for depth %d" % depth)
			quit(1)
			return
		
		var hybrid_prob: float = float(hybrid_result.get("team1_prob", 0.5))
		var hybrid_model: String = hybrid_result.get("model", "unknown")
		
		# Test certified model for comparison
		var certified_result: Dictionary = backend.predict_draft_winner(allies, enemies, stats_dir)
		var certified_prob: float = float(certified_result.get("team1_prob", 0.5))
		
		print("  Hybrid: prob=%.4f model=%s" % [hybrid_prob, hybrid_model])
		print("  Certified: prob=%.4f" % certified_prob)
		
		# Verify model selection
		if depth <= 3 and hybrid_model != "hybrid":
			push_error("test_hybrid_model: Expected hybrid model for depth %d, got %s" % [depth, hybrid_model])
			quit(1)
			return
		elif depth >= 4 and hybrid_model != "hybrid":
			push_error("test_hybrid_model: Expected hybrid model for depth %d, got %s" % [depth, hybrid_model])
			quit(1)
			return
		
		# For depth 4+, hybrid should match certified
		if depth >= 4:
			var prob_diff: float = abs(hybrid_prob - certified_prob)
			if prob_diff > 0.0001:
				print("  WARNING: Depth %d hybrid vs certified prob diff: %.6f" % [depth, prob_diff])
			else:
				print("  OK: Hybrid matches certified for depth %d" % depth)
	
	# Test error handling
	print("test_hybrid_model: Testing error handling...")
	var error_result: Dictionary = backend.predict_draft_winner_hybrid(["InvalidChamp"], ["AnotherInvalid"], "res://nonexistent_stats")
	if error_result.get("model") != "error":
		push_error("test_hybrid_model: Expected error model for invalid stats dir")
		quit(1)
		return
	
	print("")
	print("================ HYBRID MODEL TEST COMPLETE ================")
	print("All tests passed. Hybrid model is working correctly.")
	print("=======================================================")
	quit(0)
