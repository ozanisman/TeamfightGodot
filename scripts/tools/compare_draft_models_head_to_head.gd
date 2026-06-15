extends SceneTree

## Head-to-head round-robin tournament of draft models.
## Each pair of models plays 20 full drafts (10 on each side), then 5v5 combat.

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SpawnSpecScript := preload("res://scripts/simulation/spawn_spec.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

const TEAM_SIZE: int = 5
const NATIVE_STATS_DIR := "res://model_stats/stats_output_100k"
const CERTIFIED_STATS_DIR := "res://model_stats/certified_pairwise_testing_250k"

var _backend: RefCounted = null
var _champion_ids: Array[StringName] = []


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

	var seeds := maxi(1, int(_extract_argument("--seeds=", "20")))
	var sims_per_draft := maxi(1, int(_extract_argument("--sims-per-draft=", "3")))
	var output_path := _extract_argument("--output=", "res://model_stats/draft_head_to_head.csv")

	_backend = NativeSimulationBackendScript.new()
	if not _backend.is_available():
		push_error("head_to_head: native backend unavailable")
		quit(1)
		return

	_champion_ids = ChampionCatalogScript.get_champion_ids()
	if _champion_ids.size() < TEAM_SIZE * 2 + 6:
		push_error("head_to_head: not enough champions")
		quit(1)
		return

	var models: Array[String] = ["native", "certified", "draft_aware", "random"]
	var pairs: Array[Array] = []
	var only_pair := _extract_argument("--pair=", "")
	if only_pair.is_empty():
		for i in range(models.size()):
			for j in range(i + 1, models.size()):
				pairs.append([models[i], models[j]])
	else:
		var parts := only_pair.split(",")
		if parts.size() == 2 and parts[0] in models and parts[1] in models:
			pairs.append([String(parts[0]), String(parts[1])])
		else:
			push_error("head_to_head: invalid --pair=%s, expected model_a,model_b" % only_pair)
			quit(1)
			return

	print("head_to_head: seeds=%d sims_per_draft=%d pairs=%d total_matches=%d" % [
		seeds, sims_per_draft, pairs.size(), pairs.size() * seeds
	])

	var csv_lines: Array[String] = [
		"pair_index,model_a,model_b,seed,blue_model,red_model,blue_wins,total_decisive,red_winrate"
	]
	var model_totals: Dictionary = {}
	for m in models:
		model_totals[m] = {"wins": 0.0, "matches": 0}

	for pair_index in range(pairs.size()):
		var pair: Array = pairs[pair_index]
		var model_a: String = pair[0]
		var model_b: String = pair[1]
		print("  [%d/%d] %s vs %s" % [pair_index + 1, pairs.size(), model_a, model_b])

		for seed_offset in range(seeds):
			var seed := seed_offset
			# Alternate which model is blue
			var blue_model: String = model_a if (seed_offset % 2 == 0) else model_b
			var red_model: String = model_b if (seed_offset % 2 == 0) else model_a

			var draft := _run_full_draft(blue_model, red_model, seed)
			var blue_team: Array[StringName] = draft["blue"]
			var red_team: Array[StringName] = draft["red"]

			var result := _simulate_match(blue_team, red_team, sims_per_draft, seed)
			var blue_wins: float = result["blue_wins"]
			var total_decisive: float = result["total"]
			var blue_winrate: float = blue_wins / total_decisive if total_decisive > 0 else 0.5
			var red_winrate: float = 1.0 - blue_winrate

			model_totals[blue_model]["wins"] += blue_winrate
			model_totals[blue_model]["matches"] += 1.0
			model_totals[red_model]["wins"] += red_winrate
			model_totals[red_model]["matches"] += 1.0

			csv_lines.append("%d,%s,%s,%d,%s,%s,%.2f,%.0f,%.4f" % [
				pair_index, model_a, model_b, seed, blue_model, red_model,
				blue_wins, total_decisive, red_winrate
			])

		if (pair_index + 1) % 2 == 0 or pair_index == pairs.size() - 1:
			_print_interim_summary(model_totals, models)

	# Write CSV
	var f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f == null:
		push_error("head_to_head: could not open %s" % output_path)
		quit(1)
		return
	f.store_string("\n".join(csv_lines) + "\n")
	f.close()

	if _backend.has_method("clear"):
		_backend.call("clear")

	print("")
	print("================ HEAD-TO-HEAD FINAL SUMMARY ================")
	for m in models:
		var t: Dictionary = model_totals[m]
		var wr: float = float(t["wins"]) / float(t["matches"]) * 100.0 if t["matches"] > 0 else 0.0
		print("  %-15s %.1f%%  (%d matches)" % [m, wr, int(t["matches"])])
	print("CSV written to: %s" % output_path)
	print("============================================================")
	quit(0)


func _run_full_draft(blue_model: String, red_model: String, seed: int) -> Dictionary:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var pool := _champion_ids.duplicate()
	_shuffle(pool, rng)

	var blue_team: Array[StringName] = []
	var red_team: Array[StringName] = []
	var blue_bans: Array[StringName] = []
	var red_bans: Array[StringName] = []

	var sequence := _generate_draft_sequence()
	for step in range(sequence.size()):
		var turn: Dictionary = sequence[step]
		var acting_side: String = turn["side"]
		var is_ban: bool = String(turn["action"]) == "BAN"
		var model_name: String = blue_model if acting_side == "B" else red_model

		var allies: Array[StringName] = blue_team if acting_side == "B" else red_team
		var enemies: Array[StringName] = red_team if acting_side == "B" else blue_team
		var available: Array[StringName] = []
		for id in pool:
			if id not in blue_team and id not in red_team and id not in blue_bans and id not in red_bans:
				available.append(id)

		var pick: StringName = _get_recommendation(
			model_name, allies, enemies, available, step, acting_side, is_ban
		)

		if is_ban:
			if acting_side == "B":
				blue_bans.append(pick)
			else:
				red_bans.append(pick)
		else:
			if acting_side == "B":
				blue_team.append(pick)
			else:
				red_team.append(pick)

	return {"blue": blue_team, "red": red_team}


func _get_recommendation(
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
		var best_pick := StringName(available[0])
		var best_score := -1.0
		for candidate in available:
			var hypothetical_allies := allies.duplicate()
			hypothetical_allies.append(candidate)
			var result: Dictionary = _backend.predict_draft_winner(
				hypothetical_allies, enemies, CERTIFIED_STATS_DIR,
				0.50, 0.25, 0.25, 0.25, 0.0, 10.0, false,
				1.2, 1.2, 4, 0.0, 0.0
			)
			var score := float(result.get("team1_prob", 0.5))
			if score > best_score:
				best_score = score
				best_pick = StringName(candidate)
		return best_pick
	else:
		var best_ban := StringName(available[0])
		var best_enemy_score := -1.0
		for candidate in available:
			var hypothetical_enemies := enemies.duplicate()
			hypothetical_enemies.append(candidate)
			var result: Dictionary = _backend.predict_draft_winner(
				hypothetical_enemies, allies, CERTIFIED_STATS_DIR,
				0.50, 0.25, 0.25, 0.25, 0.0, 10.0, false,
				1.2, 1.2, 4, 0.0, 0.0
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


func _simulate_match(blue_team: Array[StringName], red_team: Array[StringName], sims: int, seed: int) -> Dictionary:
	if blue_team.size() != TEAM_SIZE or red_team.size() != TEAM_SIZE:
		return {"blue_wins": 0.0, "total": 0.0}
	var player_units := _build_units(blue_team, &"player")
	var enemy_units := _build_units(red_team, &"enemy")
	var inputs: Array = []
	inputs.resize(sims)
	for s in range(sims):
		inputs[s] = MatchReplayInputScript.new(seed + s, SimConstantsScript.DEFAULT_TICK_RATE, player_units, enemy_units)
	var summaries_var: Variant = _backend.run_matches_stats(inputs)
	if typeof(summaries_var) != TYPE_ARRAY:
		return {"blue_wins": 0.0, "total": 0.0}
	var summaries: Array = summaries_var
	if summaries.is_empty():
		return {"blue_wins": 0.0, "total": 0.0}
	var wins := 0
	var decisive := 0
	for summary in summaries:
		var winner: String = String(Dictionary(summary).get("winner_team", ""))
		if winner == "player":
			wins += 1
			decisive += 1
		elif winner == "enemy":
			decisive += 1
	return {"blue_wins": float(wins), "total": float(decisive)}


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


func _shuffle(values: Array, rng: RandomNumberGenerator) -> void:
	for i in range(values.size() - 1, 0, -1):
		var j := rng.randi_range(0, i)
		var tmp = values[i]
		values[i] = values[j]
		values[j] = tmp


func _print_interim_summary(model_totals: Dictionary, models: Array[String]) -> void:
	print("  --- interim ---")
	for m in models:
		var t: Dictionary = model_totals[m]
		if t["matches"] > 0:
			var wr: float = float(t["wins"]) / float(t["matches"]) * 100.0
			print("    %-15s %.1f%%  (%d matches)" % [m, wr, int(t["matches"])])
