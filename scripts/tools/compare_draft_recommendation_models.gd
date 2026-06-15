extends SceneTree

## Compares draft recommendation models (native full, certified pairwise, draft aware, random)
## against a rollout oracle on sampled partial draft states.
## Evaluates both pick and ban phases.

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SpawnSpecScript := preload("res://scripts/simulation/spawn_spec.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

const TEAM_SIZE: int = 5

const NATIVE_STATS_DIR := "res://model_stats/stats_output_100k"
const CERTIFIED_STATS_DIR := "res://model_stats/certified_pairwise_testing_250k"

var _backend: RefCounted = null


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	OS.set_environment("TEAMFIGHT_STATS_EXPORT_MINIMAL", "1")
	ChampionCatalogScript.build_catalog()
	ChampionCatalogScript.build_role_configs()
	ChampionCatalogScript.build_passive_registry()
	ChampionCatalogScript.build_minion_catalog()
	ChampionCatalogScript.freeze_built_specs_for_worker_reuse()

	var state_count := maxi(1, int(_extract_argument("--states=", "50")))
	var rollouts := maxi(1, int(_extract_argument("--rollouts=", "20")))
	var draft_depth := clampi(int(_extract_argument("--draft-depth=", "3")), 0, TEAM_SIZE - 1)
	var base_seed := int(_extract_argument("--base-seed=", "120000"))
	var output_path := _extract_argument("--output=", "res://model_stats/draft_model_comparison.csv")

	_backend = NativeSimulationBackendScript.new()
	if not _backend.is_available():
		push_error("compare_draft_models: native backend unavailable")
		quit(1)
		return
	if not _backend.has_method("run_matches_stats"):
		push_error("compare_draft_models: backend missing run_matches_stats()")
		quit(1)
		return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2 + draft_depth * 2:
		push_error("compare_draft_models: not enough champions")
		quit(1)
		return

	print("compare_draft_models: states=%d rollouts=%d draft_depth=%d" % [state_count, rollouts, draft_depth])

	var csv_lines: Array[String] = [
		"state_index,phase,draft_step,allies,enemies,available_n,model_name,model_pick,oracle_pick,model_score,oracle_score,regret,top1_match"
	]

	var model_names: Array[String] = ["native", "certified", "draft_aware", "random"]
	var totals: Dictionary = {}
	for name in model_names:
		totals[name] = {"score_sum": 0.0, "regret_sum": 0.0, "top1_count": 0, "count": 0}

	for state_index in range(state_count):
		var state := _sample_state(champion_ids, draft_depth, base_seed + state_index)
		var allies: Array[StringName] = state["allies"]
		var enemies: Array[StringName] = state["enemies"]
		var available: Array[StringName] = state["available"]
		var picking_team: String = state["picking_team"]

		# Simulate full draft sequence up to a random step to get pick and ban states
		var full_sequence := _generate_draft_sequence()
		var step := _pick_random_step(full_sequence, base_seed + state_index, allies, enemies, available)
		if step < 0:
			continue

		var turn := full_sequence[step]
		var is_ban: bool = String(turn["action"]) == "BAN"
		var acting_side: String = turn["side"]

		# Determine allies/enemies from acting side perspective
		var actor_allies: Array[StringName] = allies if acting_side == "B" else enemies
		var actor_enemies: Array[StringName] = enemies if acting_side == "B" else allies

		# Run oracle
		var oracle_result: Dictionary
		if is_ban:
			oracle_result = _oracle_ban(actor_allies, actor_enemies, available, rollouts, base_seed + state_index * 10000 + step)
		else:
			oracle_result = _oracle_pick(actor_allies, actor_enemies, available, rollouts, base_seed + state_index * 10000 + step)
		var oracle_best: StringName = oracle_result["best"]
		var oracle_scores: Dictionary = oracle_result["scores"]
		var oracle_best_score: float = float(oracle_scores.get(oracle_best, 0.5))

		# Query each model
		for model_name in model_names:
			var model_pick: StringName = _get_model_recommendation(
				model_name, actor_allies, actor_enemies, available, step, acting_side, is_ban
			)
			var model_score: float = float(oracle_scores.get(model_pick, 0.5))
			var regret: float = oracle_best_score - model_score
			var top1_match := 1 if model_pick == oracle_best else 0

			totals[model_name]["score_sum"] += model_score
			totals[model_name]["regret_sum"] += regret
			totals[model_name]["top1_count"] += top1_match
			totals[model_name]["count"] += 1

			csv_lines.append("%d,%s,%d,%s,%s,%d,%s,%s,%s,%.6f,%.6f,%.6f,%d" % [
				state_index,
				"ban" if is_ban else "pick",
				step,
				"|".join(actor_allies),
				"|".join(actor_enemies),
				available.size(),
				model_name,
				String(model_pick),
				String(oracle_best),
				model_score,
				oracle_best_score,
				regret,
				top1_match
			])

		if (state_index + 1) % 10 == 0:
			print("  ... %d/%d states done" % [state_index + 1, state_count])

	# Write CSV
	var f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f == null:
		push_error("compare_draft_models: could not open %s" % output_path)
		quit(1)
		return
	f.store_string("\n".join(csv_lines) + "\n")
	f.close()

	if _backend.has_method("clear"):
		_backend.call("clear")

	# Print summary
	_print_summary(totals, state_count, model_names)
	print("CSV written to: %s" % output_path)
	quit(0)


func _get_model_recommendation(
	model_name: String,
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	draft_step: int,
	acting_side: String,
	is_ban: bool
) -> StringName:
	match model_name:
		"native":
			return _get_native_recommendation(allies, enemies, available, draft_step, acting_side, is_ban)
		"certified":
			return _get_certified_recommendation(allies, enemies, available, is_ban)
		"draft_aware":
			return _get_draft_aware_recommendation(allies, enemies, available, is_ban)
		"random":
			return _get_random_recommendation(available)
		_:
			return _get_random_recommendation(available)


func _get_native_recommendation(
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	draft_step: int,
	acting_side: String,
	is_ban: bool
) -> StringName:
	if available.is_empty():
		return StringName("")
	var recommendations: Array
	if is_ban:
		recommendations = _backend.get_draft_ai_ban_recommendations(
			NATIVE_STATS_DIR, available, allies, enemies, 1, draft_step, acting_side, {}, 0
		)
	else:
		recommendations = _backend.get_draft_ai_pick_recommendations(
			NATIVE_STATS_DIR, available, allies, enemies, 1, draft_step, 0
		)
	if recommendations.is_empty():
		return StringName(available[0])
	return StringName(Dictionary(recommendations[0]).get("candidate", available[0]))


func _get_certified_recommendation(
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	is_ban: bool
) -> StringName:
	if available.is_empty():
		return StringName("")
	if not is_ban:
		var rows: Array = _backend.get_draft_recommendations_with_breakdowns(
			allies, enemies, available, available.size(), CERTIFIED_STATS_DIR, 0.50, 0.25, 0.25
		)
		if rows.is_empty():
			return StringName(available[0])
		return StringName(Dictionary(rows[0]).get("champion", available[0]))
	else:
		# Ban: iterative enemy-pick scoring. Ban the champion that most helps enemy.
		var best_ban := StringName(available[0])
		var best_enemy_score := -1.0
		for candidate in available:
			var hypothetical_enemies := enemies.duplicate()
			hypothetical_enemies.append(candidate)
			var result: Dictionary = _backend.predict_draft_winner(
				hypothetical_enemies, allies, CERTIFIED_STATS_DIR
			)
			var enemy_score := float(result.get("team1_prob", 0.5))
			if enemy_score > best_enemy_score:
				best_enemy_score = enemy_score
				best_ban = StringName(candidate)
		return best_ban


func _get_draft_aware_recommendation(
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	is_ban: bool
) -> StringName:
	if available.is_empty():
		return StringName("")
	if not is_ban:
		# Iterative: score each candidate with predict_draft_winner(mode=4)
		var best_pick := StringName(available[0])
		var best_score := -1.0
		for candidate in available:
			var hypothetical_allies := allies.duplicate()
			hypothetical_allies.append(candidate)
			var result: Dictionary = _backend.predict_draft_winner(
				hypothetical_allies, enemies, CERTIFIED_STATS_DIR,
				0.50, 0.25, 0.25, 0.25, 0.0, 10.0, false,
				1.2, 1.2, 4, 0.0, 0.0  # scoring_mode=4 = DRAFT_AWARE
			)
			var score := float(result.get("team1_prob", 0.5))
			if score > best_score:
				best_score = score
				best_pick = StringName(candidate)
		return best_pick
	else:
		# Ban: iterative enemy-pick scoring with draft-aware mode
		var best_ban := StringName(available[0])
		var best_enemy_score := -1.0
		for candidate in available:
			var hypothetical_enemies := enemies.duplicate()
			hypothetical_enemies.append(candidate)
			var result: Dictionary = _backend.predict_draft_winner(
				hypothetical_enemies, allies, CERTIFIED_STATS_DIR,
				0.50, 0.25, 0.25, 0.25, 0.0, 10.0, false,
				1.2, 1.2, 4, 0.0, 0.0  # scoring_mode=4
			)
			var enemy_score := float(result.get("team1_prob", 0.5))
			if enemy_score > best_enemy_score:
				best_enemy_score = enemy_score
				best_ban = StringName(candidate)
		return best_ban


func _get_random_recommendation(available: Array[StringName]) -> StringName:
	if available.is_empty():
		return StringName("")
	var idx := randi() % available.size()
	return StringName(available[idx])


func _oracle_pick(
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	rollouts: int,
	seed: int
) -> Dictionary:
	var scores: Dictionary = {}
	for candidate_index in range(available.size()):
		var candidate: StringName = available[candidate_index]
		var avg_score := _evaluate_pick(allies, enemies, available, candidate, rollouts, seed + candidate_index * 1000)
		scores[candidate] = avg_score

	var best_candidate := StringName(available[0])
	var best_score := -1.0
	for candidate in available:
		var s: float = float(scores.get(candidate, 0.0))
		if s > best_score:
			best_score = s
			best_candidate = StringName(candidate)
	return {"best": best_candidate, "scores": scores}


func _oracle_ban(
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	rollouts: int,
	seed: int
) -> Dictionary:
	# For each ban candidate, remove it, then opponent picks their best from reduced pool,
	# then random-complete and simulate. Best ban = the one that gives us highest winrate.
	var scores: Dictionary = {}
	for candidate_index in range(available.size()):
		var candidate: StringName = available[candidate_index]
		var reduced_available: Array[StringName] = []
		for id in available:
			if id != candidate:
				reduced_available.append(id)

		# Opponent's best pick from reduced pool
		var opponent_pick_result := _oracle_pick(enemies, allies, reduced_available, rollouts, seed + candidate_index * 1000)
		var opponent_pick: StringName = opponent_pick_result["best"]

		# Complete the draft
		var completed_enemies := enemies.duplicate()
		completed_enemies.append(opponent_pick)
		var pool: Array[StringName] = []
		for id in reduced_available:
			if id != opponent_pick:
				pool.append(id)

		var rng := RandomNumberGenerator.new()
		rng.seed = seed + candidate_index * 1000 + 999
		_shuffle(pool, rng)
		var completed_allies := allies.duplicate()
		while completed_allies.size() < TEAM_SIZE and not pool.is_empty():
			completed_allies.append(pool.pop_back())
		while completed_enemies.size() < TEAM_SIZE and not pool.is_empty():
			completed_enemies.append(pool.pop_back())

		var winrate := _simulate_5v5(completed_allies, completed_enemies, rollouts, seed + candidate_index * 1000 + 500)
		scores[candidate] = winrate

	var best_candidate := StringName(available[0])
	var best_score := -1.0
	for candidate in available:
		var s: float = float(scores.get(candidate, 0.0))
		if s > best_score:
			best_score = s
			best_candidate = StringName(candidate)
	return {"best": best_candidate, "scores": scores}


func _evaluate_pick(
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	candidate: StringName,
	rollouts: int,
	seed: int
) -> float:
	var completed_allies := allies.duplicate()
	completed_allies.append(candidate)
	var pool: Array[StringName] = []
	for id in available:
		if id != candidate:
			pool.append(id)
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	_shuffle(pool, rng)
	while completed_allies.size() < TEAM_SIZE and not pool.is_empty():
		completed_allies.append(pool.pop_back())
	var completed_enemies := enemies.duplicate()
	while completed_enemies.size() < TEAM_SIZE and not pool.is_empty():
		completed_enemies.append(pool.pop_back())
	return _simulate_5v5(completed_allies, completed_enemies, rollouts, seed + 1)


func _simulate_5v5(allies: Array[StringName], enemies: Array[StringName], rollouts: int, seed: int) -> float:
	if allies.size() != TEAM_SIZE or enemies.size() != TEAM_SIZE:
		return 0.5
	var player_units := _build_units(allies, &"player")
	var enemy_units := _build_units(enemies, &"enemy")
	var inputs: Array = []
	inputs.resize(rollouts)
	for s in range(rollouts):
		inputs[s] = MatchReplayInputScript.new(seed + s, SimConstantsScript.DEFAULT_TICK_RATE, player_units, enemy_units)
	var summaries_var: Variant = _backend.run_matches_stats(inputs)
	if typeof(summaries_var) != TYPE_ARRAY:
		return 0.5
	var summaries: Array = summaries_var
	if summaries.is_empty():
		return 0.5
	var wins := 0
	var decisive := 0
	for summary in summaries:
		var winner: String = String(Dictionary(summary).get("winner_team", ""))
		if winner == "player":
			wins += 1
			decisive += 1
		elif winner == "enemy":
			decisive += 1
	return float(wins) / float(decisive) if decisive > 0 else 0.5


func _build_units(ids: Array[StringName], team: StringName) -> Array:
	var units: Array = []
	for index in range(ids.size()):
		var pos := SimConstantsScript.spawn_position(index, team)
		units.append(SpawnSpecScript.new(ids[index], team, pos.x, pos.y))
	return units


func _generate_draft_sequence() -> Array[Dictionary]:
	return [
		{"side": "B", "action": "BAN"},  {"side": "R", "action": "BAN"},
		{"side": "B", "action": "BAN"},  {"side": "R", "action": "BAN"},
		{"side": "B", "action": "BAN"},  {"side": "R", "action": "BAN"},
		{"side": "B", "action": "PICK"}, {"side": "R", "action": "PICK"},
		{"side": "R", "action": "PICK"}, {"side": "B", "action": "PICK"},
		{"side": "B", "action": "PICK"}, {"side": "R", "action": "PICK"},
		{"side": "R", "action": "BAN"},  {"side": "B", "action": "BAN"},
		{"side": "R", "action": "BAN"},  {"side": "B", "action": "BAN"},
		{"side": "R", "action": "PICK"}, {"side": "B", "action": "PICK"},
		{"side": "B", "action": "PICK"}, {"side": "R", "action": "PICK"}
	]


func _pick_random_step(
	sequence: Array[Dictionary],
	seed: int,
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName]
) -> int:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	# Try up to 50 times to find a valid step where the acting side has a non-empty available pool
	for _attempt in range(50):
		var step := rng.randi_range(0, sequence.size() - 1)
		var turn := sequence[step]
		var acting_side: String = turn["side"]
		var is_ban: bool = String(turn["action"]) == "BAN"
		var actor_allies := allies if acting_side == "B" else enemies
		var actor_enemies := enemies if acting_side == "B" else allies
		if available.is_empty():
			continue
		# Check if this is a realistic step given current team sizes
		var valid := _is_step_valid(step, actor_allies, actor_enemies, is_ban)
		if valid:
			return step
	return -1


func _is_step_valid(step: int, allies: Array[StringName], enemies: Array[StringName], is_ban: bool) -> bool:
	# Simple heuristic: in early game, bans are valid; picks are valid if team has room
	if is_ban:
		return true
	return allies.size() < TEAM_SIZE


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
		"picking_team": "player" if picking_player else "enemy"
	}


func _shuffle(values: Array, rng: RandomNumberGenerator) -> void:
	for i in range(values.size() - 1, 0, -1):
		var j := rng.randi_range(0, i)
		var tmp = values[i]
		values[i] = values[j]
		values[j] = tmp


func _print_summary(totals: Dictionary, state_count: int, model_names: Array[String]) -> void:
	print("")
	print("================ MODEL COMPARISON SUMMARY ================")
	print("states=%d" % state_count)
	for name in model_names:
		var t: Dictionary = totals[name]
		var n: float = float(t["count"])
		if n == 0:
			print("  %s: no data" % name)
			continue
		var avg_win: float = float(t["score_sum"]) / n
		var avg_regret: float = float(t["regret_sum"]) / n
		var top1_pct := float(t["top1_count"]) / n * 100.0
		print("  %-15s avg_win=%5.1f%% regret=%5.2fpp top1=%5.1f%% n=%d" % [
			name, avg_win * 100.0, avg_regret * 100.0, top1_pct, int(n)
		])
	print("===========================================================")
