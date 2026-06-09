extends SceneTree

## Training data generator for partial draft prediction model.
##
## Samples partial draft states at each depth, completes them with rollouts,
## and labels them with expected win probability from the certified model.

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

const TEAM_SIZE: int = 5
const DEFAULT_OUTPUT := "res://stats_output_partial/partial_draft_training_smoke.csv"


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

	var states_per_depth := maxi(1, int(_extract_argument("--states-per-depth=", "25")))
	var rollouts_per_state := maxi(1, int(_extract_argument("--rollouts-per-state=", "20")))
	var base_seed := int(_extract_argument("--base-seed=", "80000"))
	var stats_dir := _extract_argument("--stats-dir=", "res://stats_output")
	var output_path := _extract_argument("--output=", DEFAULT_OUTPUT)

	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("generate_partial_draft_training_data: native simulation backend unavailable")
		quit(1)
		return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("generate_partial_draft_training_data: not enough champions for distinct 5v5 drafts")
		quit(1)
		return

	print("generate_partial_draft_training_data: states/depth=%d rollouts/state=%d stats_dir=%s" % [
		states_per_depth, rollouts_per_state, stats_dir
	])

	var csv_lines: Array[String] = [
		"depth,allies,enemies,expected_win,rollout_count"
	]
	var total_states := 0

	for depth in range(1, TEAM_SIZE):
		print("Processing depth %d..." % depth)
		for state_index in range(states_per_depth):
			var state := _sample_state(champion_ids, depth, base_seed + total_states)
			var allies: Array[StringName] = state["allies"]
			var enemies: Array[StringName] = state["enemies"]
			var available: Array[StringName] = state["available"]

			var expected_win := _rollout_label_state(
				backend, allies, enemies, available, rollouts_per_state, base_seed, total_states, stats_dir
			)

			csv_lines.append("%d,%s,%s,%.6f,%d" % [
				depth,
				"|".join(allies),
				"|".join(enemies),
				expected_win,
				rollouts_per_state
			])

			total_states += 1
			if (state_index + 1) % 10 == 0:
				print("  State %d/%d at depth %d" % [state_index + 1, states_per_depth, depth])

	var f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f == null:
		push_error("generate_partial_draft_training_data: could not open %s" % output_path)
		quit(1)
		return
	f.store_string("\n".join(csv_lines) + "\n")
	f.close()
	if backend.has_method("clear"):
		backend.call("clear")

	print("")
	print("================ PARTIAL DRAFT TRAINING DATA GENERATED ================")
	print("total_states=%d (depths 1-%d)" % [total_states, TEAM_SIZE - 1])
	print("states_per_depth=%d rollouts_per_state=%d" % [states_per_depth, rollouts_per_state])
	print("CSV written to: %s" % output_path)
	print("=======================================================================")
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


func _rollout_label_state(backend: RefCounted, allies: Array[StringName], enemies: Array[StringName], available: Array[StringName], rollouts_per_state: int, base_seed: int, state_index: int, stats_dir: String) -> float:
	var sum: float = 0.0
	var valid_rollouts: int = 0
	for rollout_index in range(rollouts_per_state):
		var completion := _complete_random_rollout(
			allies, enemies, available, base_seed + state_index * 10000 + rollout_index
		)
		if completion["allies"].size() != TEAM_SIZE or completion["enemies"].size() != TEAM_SIZE:
			continue
		var prediction: Dictionary = backend.predict_draft_winner(
			completion["allies"], completion["enemies"], stats_dir
		)
		if prediction.is_empty():
			continue
		sum += float(prediction.get("team1_prob", 0.5))
		valid_rollouts += 1
	if valid_rollouts == 0:
		return 0.5
	return sum / float(valid_rollouts)


func _complete_random_rollout(allies: Array[StringName], enemies: Array[StringName], available: Array[StringName], seed: int) -> Dictionary:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var completed_allies := allies.duplicate()
	var completed_enemies := enemies.duplicate()
	var pool: Array[StringName] = available.duplicate()
	_shuffle(pool, rng)
	while completed_allies.size() < TEAM_SIZE and not pool.is_empty():
		completed_allies.append(pool.pop_back())
	while completed_enemies.size() < TEAM_SIZE and not pool.is_empty():
		completed_enemies.append(pool.pop_back())
	return {"allies": completed_allies, "enemies": completed_enemies}


func _shuffle(values: Array, rng: RandomNumberGenerator) -> void:
	for i in range(values.size() - 1, 0, -1):
		var j := rng.randi_range(0, i)
		var tmp = values[i]
		values[i] = values[j]
		values[j] = tmp
