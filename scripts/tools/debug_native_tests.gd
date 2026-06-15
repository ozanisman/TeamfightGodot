extends SceneTree

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
	var backend := NativeSimulationBackendScript.new()
	
	var output_lines := []
	output_lines.append("=== Running Native Debug Tests ===")
	
	if not backend.is_available():
		output_lines.append("ERROR: Native backend unavailable")
		_write_output(output_lines)
		quit(1)
		return
	
	var stats_dir := "res://stats_output_100k"
	
	output_lines.append("\n1. Testing draft AI stats loading...")
	if backend.has_method("debug_test_draft_ai_stats"):
		backend.call("debug_test_draft_ai_stats", stats_dir)
		output_lines.append("   Stats test completed")
	else:
		output_lines.append("ERROR: debug_test_draft_ai_stats not found")
	
	output_lines.append("\n2. Testing draft AI pick evaluation...")
	if backend.has_method("debug_test_draft_ai_pick_eval"):
		backend.call("debug_test_draft_ai_pick_eval", stats_dir)
		output_lines.append("   Pick eval test completed")
	else:
		output_lines.append("ERROR: debug_test_draft_ai_pick_eval not found")
	
	output_lines.append("\n3. Testing draft AI pick recommendations...")
	if backend.has_method("debug_test_draft_ai_pick_recommendations"):
		backend.call("debug_test_draft_ai_pick_recommendations", stats_dir)
		output_lines.append("   Pick recommendations test completed")
	else:
		output_lines.append("ERROR: debug_test_draft_ai_pick_recommendations not found")
	
	output_lines.append("\n4. Testing draft AI ban evaluation...")
	if backend.has_method("debug_test_draft_ai_ban_eval"):
		backend.call("debug_test_draft_ai_ban_eval", stats_dir)
		output_lines.append("   Ban eval test completed")
	else:
		output_lines.append("ERROR: debug_test_draft_ai_ban_eval not found")
	
	output_lines.append("\n5. Testing draft AI ban recommendations...")
	if backend.has_method("debug_test_draft_ai_ban_recommendations"):
		backend.call("debug_test_draft_ai_ban_recommendations", stats_dir)
		output_lines.append("   Ban recommendations test completed")
	else:
		output_lines.append("ERROR: debug_test_draft_ai_ban_recommendations not found")
	
	output_lines.append("\n=== Debug Tests Complete ===")
	_write_output(output_lines)
	quit(0)

func _write_output(lines: Array) -> void:
	var f := FileAccess.open("user://native_debug_output.txt", FileAccess.WRITE)
	if f:
		f.store_string("\n".join(lines))
		f.close()
		print("Output written to user://native_debug_output.txt")
