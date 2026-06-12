extends SceneTree

## Analyzes variance in winrate estimates from random completion strategy.
##
## For a fixed draft state + champion pick, varies only the random completion
## seed to measure how much completion strategy affects winrate estimates.

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

const TEAM_SIZE: int = 5


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	ChampionCatalogScript.build_catalog()
	ChampionCatalogScript.build_role_configs()
	ChampionCatalogScript.build_passive_registry()
	ChampionCatalogScript.build_minion_catalog()
	ChampionCatalogScript.freeze_built_specs_for_worker_reuse()

	var state_count := maxi(1, int(_extract_argument("--states=", "25")))
	var completion_variations := maxi(1, int(_extract_argument("--completion-variations=", "100")))
	var draft_depth := clampi(int(_extract_argument("--draft-depth=", "4")), 0, TEAM_SIZE - 1)
	var base_seed := int(_extract_argument("--base-seed=", "80000"))
	var stats_dir := _extract_argument("--stats-dir=", "res://model_stats/certified_pairwise_testing_250k")
	var output_path := _extract_argument("--output=", "res://model_stats/certified_pairwise_testing_250k/completion_variance.csv")

	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("analyze_completion_variance: native simulation backend unavailable")
		quit(1)
		return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("analyze_completion_variance: not enough champions for distinct 5v5 drafts")
		quit(1)
		return

	print("analyze_completion_variance: states=%d completion_variations=%d draft_depth=%d" % [
		state_count, completion_variations, draft_depth
	])

	var csv_lines: Array[String] = [
		"state_index,champion,completion_seed,winrate"
	]

	for state_index in range(state_count):
		var state := _sample_state(champion_ids, draft_depth, base_seed + state_index)
		var allies: Array[StringName] = state["allies"]
		var enemies: Array[StringName] = state["enemies"]
		var available: Array[StringName] = state["available"]
		
		# Pick first available champion for analysis
		var test_champion: StringName = available[0]
		
		for var_index in range(completion_variations):
			var completion_seed := base_seed + state_index * 10000 + var_index
			var winrate := _evaluate_single_completion(
				backend, allies, enemies, available, test_champion, completion_seed
			)
			csv_lines.append("%d,%s,%d,%.6f" % [state_index, test_champion, var_index, winrate])
		
		if (state_index + 1) % 5 == 0:
			print("  State %d/%d" % [state_index + 1, state_count])

	var global_path := ProjectSettings.globalize_path(output_path)
	print("Writing to: %s" % global_path)
	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f == null:
		push_error("analyze_completion_variance: could not open %s" % output_path)
		quit(1)
		return
	f.store_string("\n".join(csv_lines) + "\n")
	f.close()
	if backend.has_method("clear"):
		backend.call("clear")

	print("")
	print("================ COMPLETION VARIANCE ANALYSIS ================")
	print("states=%d completion_variations=%d draft_depth=%d" % [state_count, completion_variations, draft_depth])
	print("CSV written to: %s" % output_path)
	print("Use analysis script to compute variance statistics")
	print("============================================================")
	quit(0)


func _sample_state(champion_ids: Array[StringName], draft_depth: int, seed: int) -> Dictionary:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var pool := champion_ids.duplicate()
	_shuffle(pool, rng)
	var player: Array[StringName] = []
	var enemy: Array[StringName] = []
	for i in range(draft_depth):
		player.append(pool.pop_back())
		enemy.append(pool.pop_back())
	var picking_player := (seed % 2) == 0
	return {
		"allies": player if picking_player else enemy,
		"enemies": enemy if picking_player else player,
		"available": pool,
		"picking_team": "player" if picking_player else "enemy",
	}


func _evaluate_single_completion(
	backend: RefCounted,
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	pick: StringName,
	seed: int
) -> float:
	# Complete the draft: add pick to allies, then random-fill rest
	var completed_allies := allies.duplicate()
	var completed_enemies := enemies.duplicate()
	completed_allies.append(pick)

	var pool: Array[StringName] = []
	for id in available:
		if id != pick:
			pool.append(id)

	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	_shuffle(pool, rng)
	while completed_allies.size() < TEAM_SIZE and not pool.is_empty():
		completed_allies.append(pool.pop_back())
	while completed_enemies.size() < TEAM_SIZE and not pool.is_empty():
		completed_enemies.append(pool.pop_back())

	# Use certified model to predict win probability
	var prediction: Dictionary = backend.predict_draft_winner(
		completed_allies, completed_enemies, "res://model_stats/certified_pairwise_testing_250k"
	)
	return float(prediction.get("team1_prob", 0.5))


func _shuffle(array: Array, rng: RandomNumberGenerator) -> void:
	for i in range(array.size() - 1, 0, -1):
		var j := rng.randi_range(0, i)
		array.swap(i, j)
