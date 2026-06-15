extends SceneTree

# Diagnostic script for partial role composition scoring
# Tests comp fields for different team sizes to confirm partial comp lookup works

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

const STATS_DIR := "res://model_stats/stats_output_100k"

func _init() -> void:
	print("=== Partial Composition Scoring Diagnostic ===")
	print()

	print("Step 1: Loading backend...")
	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		print("ERROR: Native backend unavailable")
		quit()
		return
	print("Backend loaded successfully")
	print()

	# Use hardcoded champion names for testing
	var sample_champions := ["colossus", "wizard", "berserker", "swordsman", "paladin"]
	print("Sample champions: ", sample_champions)
	print()

	# Test 1: Pick recommendations with varying ally counts
	print("--- Test 1: Pick Recommendations with Varying Ally Counts ---")
	print()

	for i in range(5):
		var ally_count := i
		var allies := sample_champions.slice(0, ally_count)
		var enemies := sample_champions.slice(ally_count + 1, 5)
		var available := sample_champions.slice(ally_count, ally_count + 1)

		if available.is_empty():
			continue

		print("Test 1.%d: %d allies + candidate" % [i + 1, ally_count])

		var recommendations := backend.get_draft_ai_pick_recommendations(
			STATS_DIR,
			available,
			allies,
			enemies,
			1,  # max_results
			0   # draft_step
		)

		if not recommendations.is_empty():
			var rec: Dictionary = recommendations[0]
			print("  Candidate: ", rec.get("candidate", ""))
			print("  comp_fit (centered_value): ", rec.get("comp_fit", 0.0))
			print("  comp_samples: ", rec.get("comp_samples", 0))
			print("  comp_match_count: ", rec.get("comp_match_count", 0))
			print("  comp_fingerprint: ", rec.get("comp_fingerprint", ""))
		else:
			print("  No recommendations returned")
		print()

	# Test 2: Ban recommendations with varying enemy counts
	print("--- Test 2: Ban Recommendations with Varying Enemy Counts ---")
	print()

	for i in range(5):
		var enemy_count := i
		var allies := sample_champions.slice(0, 1)
		var enemies := sample_champions.slice(1, 1 + enemy_count)
		var available := sample_champions.slice(1 + enemy_count, 2 + enemy_count)

		if available.is_empty():
			continue

		print("Test 2.%d: %d enemies + candidate" % [i + 1, enemy_count])

		var recommendations := backend.get_draft_ai_ban_recommendations(
			STATS_DIR,
			available,
			allies,
			enemies,
			1,  # max_results
			0,  # draft_step
			"blue",  # acting_side
			{}  # weight_overrides
		)

		if not recommendations.is_empty():
			var rec: Dictionary = recommendations[0]
			print("  Candidate: ", rec.get("candidate", ""))
			print("  enemy_comp_fit (centered_value): ", rec.get("enemy_comp_fit", 0.0))
			print("  enemy_comp_samples: ", rec.get("enemy_comp_samples", 0))
			print("  enemy_comp_match_count: ", rec.get("enemy_comp_match_count", 0))
			print("  enemy_comp_fingerprint: ", rec.get("enemy_comp_fingerprint", ""))
		else:
			print("  No recommendations returned")
		print()

	# Test 3: Exact 5-role composition
	print("--- Test 3: Exact 5-Role Composition ---")
	print()

	var full_team := sample_champions.slice(0, 5)
	var empty_allies := []
	var empty_enemies := []
	var single_available := sample_champions.slice(0, 1)

	print("Test 3.1: 4 allies + candidate (5 total roles)")

	var full_rec := backend.get_draft_ai_pick_recommendations(
		STATS_DIR,
		single_available,
		full_team.slice(0, 4),
		empty_enemies,
		1,
		0
	)

	if not full_rec.is_empty():
		var rec: Dictionary = full_rec[0]
		print("  comp_fit: ", rec.get("comp_fit", 0.0))
		print("  comp_samples: ", rec.get("comp_samples", 0))
		print("  comp_match_count: ", rec.get("comp_match_count", 0))
		print("  comp_fingerprint: ", rec.get("comp_fingerprint", ""))
	else:
		print("  No recommendations returned")
	print()

	# Test 4: Unknown role case
	print("--- Test 4: Unknown Role Case ---")
	print()

	print("Test 4.1: Empty role list")
	var empty_roles := []
	var empty_available := sample_champions.slice(0, 1)
	var empty_rec := backend.get_draft_ai_pick_recommendations(
		STATS_DIR,
		empty_available,
		empty_roles,
		empty_enemies,
		1,
		0
	)

	if not empty_rec.is_empty():
		var rec: Dictionary = empty_rec[0]
		print("  comp_fit: ", rec.get("comp_fit", 0.0))
		print("  comp_samples: ", rec.get("comp_samples", 0))
		print("  comp_match_count: ", rec.get("comp_match_count", 0))
		print("  comp_fingerprint: ", rec.get("comp_fingerprint", ""))
	else:
		print("  No recommendations returned")
	print()

	# Test 5: Duplicate role case (simplified - just use first 2 champions)
	print("--- Test 5: Duplicate Role Case ---")
	print()

	print("Test 5.1: Using first 2 champions (may or may not have same role)")
	var dup_available := [sample_champions[0]]
	var dup_allies := [sample_champions[1]]
	print("  Champions: ", dup_available, " + ", dup_allies)

	var dup_rec := backend.get_draft_ai_pick_recommendations(
		STATS_DIR,
		dup_available,
		dup_allies,
		empty_enemies,
		1,
		0
	)

	if not dup_rec.is_empty():
		var rec: Dictionary = dup_rec[0]
		print("  comp_fit: ", rec.get("comp_fit", 0.0))
		print("  comp_samples: ", rec.get("comp_samples", 0))
		print("  comp_match_count: ", rec.get("comp_match_count", 0))
		print("  comp_fingerprint: ", rec.get("comp_fingerprint", ""))
	else:
		print("  No recommendations returned")
	print()

	print("=== Diagnostic Complete ===")

	# Write output to file for verification
	var file := FileAccess.open("res://logs/partial_comp_diagnostic_output.txt", FileAccess.WRITE)
	if file:
		file.store_string("Diagnostic complete - see log for details")
		file.close()

	quit()

func _get_role_list(champions: Array, backend: NativeSimulationBackendScript) -> Array:
	var roles := []
	# Skip role_for call since it doesn't exist on NativeSimulationBackend
	# The fingerprint field in recommendations already contains the role information
	return roles
