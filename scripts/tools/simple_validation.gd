extends SceneTree

func _init() -> void:
	print("Starting simple validation...")
	call_deferred("_run")

func _run() -> void:
	print("Test 1: Backend load")
	const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
	var backend := NativeSimulationBackendScript.new()
	print("Backend created: %s" % (backend != null))
	print("Backend available: %s" % backend.is_available())
	
	print("\nTest 2: Direct API call")
	var stats_dir := "res://stats_output_100k"
	var available := ["swordsman", "cleric", "colossus"]
	var allies := ["archer"]
	var enemies := ["berserker"]
	
	print("Calling get_draft_ai_pick_recommendations...")
	var results := backend.get_draft_ai_pick_recommendations(stats_dir, available, allies, enemies, 3)
	print("Results size: %d" % results.size())
	
	if not results.is_empty():
		for i in range(results.size()):
			var rec = results[i]
			print("%d. %s (score: %.4f)" % [i + 1, rec.candidate, rec.total_score])
	
	print("\nDone")
	quit(0)
