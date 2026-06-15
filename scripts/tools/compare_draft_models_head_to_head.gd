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

# Greedy CSV caches (loaded once in _run)
var _base_power: Dictionary = {}       # champion_id -> win_rate (float)
var _matchup_vs: Dictionary = {}       # champion_id -> opponent_id -> winrate (float)
var _matchup_with: Dictionary = {}   # champion_id -> ally_id -> winrate (float)

# Hybrid strategy instances
var _hybrid_native_picks: RefCounted = null
var _hybrid_random_picks: RefCounted = null


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

	_load_greedy_csvs()

	var HybridNativePicksRandomBans := preload("res://scripts/tools/draft_strategy_native_picks_random_bans.gd")
	var HybridRandomPicksNativeBans := preload("res://scripts/tools/draft_strategy_random_picks_native_bans.gd")
	_hybrid_native_picks = HybridNativePicksRandomBans.new(NATIVE_STATS_DIR)
	_hybrid_random_picks = HybridRandomPicksNativeBans.new(NATIVE_STATS_DIR)

	var models: Array[String] = [
		"native", "random",
		"native_picks_random_bans", "random_picks_native_bans",
		"greedy_base_power", "greedy_synergy", "greedy_counter"
	]
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
	print("")
	print("--- Greedy Scoring Formulas (diagnostic only) ---")
	print("  greedy_base_power:  score = combat_stats.win_rate")
	print("  greedy_synergy:     score = avg(matchup_with.winrate vs context)")
	print("                      fallback to base_power if context is empty")
	print("  greedy_counter:     score = avg(matchup_vs.winrate vs context)")
	print("                      fallback to base_power if context is empty")
	print("  Tie-breaking:       alphabetical sort, first max kept")
	print("  Data source:        %s" % CERTIFIED_STATS_DIR)
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
		"native_picks_random_bans":
			return _get_hybrid_recommendation(_hybrid_native_picks, allies, enemies, available, draft_step, acting_side, is_ban)
		"random_picks_native_bans":
			return _get_hybrid_recommendation(_hybrid_random_picks, allies, enemies, available, draft_step, acting_side, is_ban)
		"greedy_base_power":
			return _get_greedy_base_power_recommendation(available, is_ban)
		"greedy_synergy":
			return _get_greedy_synergy_recommendation(allies, enemies, available, is_ban)
		"greedy_counter":
			return _get_greedy_counter_recommendation(allies, enemies, available, is_ban)
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


func _get_hybrid_recommendation(
	hybrid: RefCounted,
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	draft_step: int,
	acting_side: String,
	is_ban: bool
) -> StringName:
	if available.is_empty():
		return StringName("")
	if is_ban:
		return hybrid.recommend_next_ban(allies, enemies, available, draft_step, acting_side)
	else:
		return hybrid.recommend_next_pick(allies, enemies, available, draft_step)


func _get_greedy_base_power_recommendation(available: Array[StringName], is_ban: bool) -> StringName:
	if available.is_empty():
		return StringName("")
	# Sort alphabetically for deterministic tie-breaking
	var sorted := available.duplicate()
	sorted.sort_custom(func(a: StringName, b: StringName) -> int: return String(a) < String(b))
	var best := StringName(sorted[0])
	var best_score := float(_base_power.get(String(best), 0.5))
	for i in range(1, sorted.size()):
		var candidate := StringName(sorted[i])
		var score := float(_base_power.get(String(candidate), 0.5))
		if score > best_score:
			best_score = score
			best = candidate
	return best


func _get_greedy_synergy_recommendation(
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	is_ban: bool
) -> StringName:
	if available.is_empty():
		return StringName("")
	var context: Array[StringName] = enemies if is_ban else allies
	# Fallback to base_power if no context
	if context.is_empty():
		return _get_greedy_base_power_recommendation(available, is_ban)
	var sorted := available.duplicate()
	sorted.sort_custom(func(a: StringName, b: StringName) -> int: return String(a) < String(b))
	var best := StringName(sorted[0])
	var best_score := _avg_matchup_with(sorted[0], context)
	for i in range(1, sorted.size()):
		var candidate := StringName(sorted[i])
		var score := _avg_matchup_with(candidate, context)
		if score > best_score:
			best_score = score
			best = candidate
	return best


func _get_greedy_counter_recommendation(
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	is_ban: bool
) -> StringName:
	if available.is_empty():
		return StringName("")
	var context: Array[StringName] = allies if is_ban else enemies
	# Fallback to base_power if no context
	if context.is_empty():
		return _get_greedy_base_power_recommendation(available, is_ban)
	var sorted := available.duplicate()
	sorted.sort_custom(func(a: StringName, b: StringName) -> int: return String(a) < String(b))
	var best := StringName(sorted[0])
	var best_score := _avg_matchup_vs(sorted[0], context)
	for i in range(1, sorted.size()):
		var candidate := StringName(sorted[i])
		var score := _avg_matchup_vs(candidate, context)
		if score > best_score:
			best_score = score
			best = candidate
	return best


func _avg_matchup_with(candidate: StringName, allies: Array[StringName]) -> float:
	var c := String(candidate)
	if not _matchup_with.has(c):
		return 0.5
	var total := 0.0
	var count := 0
	for ally in allies:
		var a := String(ally)
		if _matchup_with[c].has(a):
			total += float(_matchup_with[c][a])
			count += 1
	return total / float(count) if count > 0 else 0.5


func _avg_matchup_vs(candidate: StringName, enemies: Array[StringName]) -> float:
	var c := String(candidate)
	if not _matchup_vs.has(c):
		return 0.5
	var total := 0.0
	var count := 0
	for enemy in enemies:
		var e := String(enemy)
		if _matchup_vs[c].has(e):
			total += float(_matchup_vs[c][e])
			count += 1
	return total / float(count) if count > 0 else 0.5


func _load_greedy_csvs() -> void:
	var stats_dir := CERTIFIED_STATS_DIR

	# combat_stats.csv: team_size,hero,role,wins,losses,draws,total_games,win_rate,...
	var combat_path := stats_dir.path_join("combat_stats.csv")
	var f1 := FileAccess.open(combat_path, FileAccess.READ)
	if f1 != null:
		var headers: Array = f1.get_csv_line()
		var hero_idx := headers.find("hero")
		var winrate_idx := headers.find("win_rate")
		while not f1.eof_reached():
			var row: Array = f1.get_csv_line()
			if row.size() > max(hero_idx, winrate_idx):
				_base_power[row[hero_idx]] = float(row[winrate_idx])
		f1.close()

	# matchup_vs.csv: champion,opponent,wins,losses,winrate
	var vs_path := stats_dir.path_join("matchup_vs.csv")
	var f2 := FileAccess.open(vs_path, FileAccess.READ)
	if f2 != null:
		var vs_headers: Array = f2.get_csv_line()
		var c_idx := vs_headers.find("champion")
		var o_idx := vs_headers.find("opponent")
		var wr_idx := vs_headers.find("winrate")
		while not f2.eof_reached():
			var row: Array = f2.get_csv_line()
			if row.size() > max(c_idx, o_idx, wr_idx):
				var champ: String = row[c_idx]
				if not _matchup_vs.has(champ):
					_matchup_vs[champ] = {}
				_matchup_vs[champ][row[o_idx]] = float(row[wr_idx])
		f2.close()

	# matchup_with.csv: champion,ally,wins,losses,winrate
	var with_path := stats_dir.path_join("matchup_with.csv")
	var f3 := FileAccess.open(with_path, FileAccess.READ)
	if f3 != null:
		var with_headers: Array = f3.get_csv_line()
		var ch_idx := with_headers.find("champion")
		var a_idx := with_headers.find("ally")
		var w_idx := with_headers.find("winrate")
		while not f3.eof_reached():
			var row: Array = f3.get_csv_line()
			if row.size() > max(ch_idx, a_idx, w_idx):
				var champ: String = row[ch_idx]
				if not _matchup_with.has(champ):
					_matchup_with[champ] = {}
				_matchup_with[champ][row[a_idx]] = float(row[w_idx])
		f3.close()


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
