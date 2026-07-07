class_name DraftHarnessCore
extends RefCounted
## Shared full-draft execution and match simulation helpers for validation harnesses
## and self-play stats generation (Workstream D.1).

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SpawnSpecScript := preload("res://scripts/simulation/spawn_spec.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const DraftAiConfigScript := preload("res://scripts/tools/draft_ai_config.gd")

const DraftStrategyRandomPath := "res://scripts/tools/draft_strategy_random.gd"
const DraftStrategyNativePath := "res://scripts/tools/draft_strategy_native.gd"
const DraftStrategyBasePowerOnlyPath := "res://scripts/tools/draft_strategy_base_power_only.gd"
const DraftStrategyNativeAblationPath := "res://scripts/tools/draft_strategy_native_ablation.gd"
const DraftStrategyNativeSoftmaxPath := "res://scripts/tools/draft_strategy_native_softmax.gd"
const DraftStrategyNativeLookaheadPath := "res://scripts/tools/draft_strategy_native_lookahead.gd"
const DraftStrategyNativeLookaheadPickPath := "res://scripts/tools/draft_strategy_native_lookahead_pick.gd"
const DraftStrategyNativeLookaheadBanPath := "res://scripts/tools/draft_strategy_native_lookahead_ban.gd"
const DraftStrategyNativeLookaheadSoftmaxPath := "res://scripts/tools/draft_strategy_native_lookahead_softmax.gd"
const DraftStrategyNativeLearnedScorerPath := "res://scripts/tools/draft_strategy_native_learned_scorer.gd"


static func build_strategy(name: String, stats_dir: String):
	match name:
		"native_full":
			return load(DraftStrategyNativePath).new(stats_dir)
		"random":
			return load(DraftStrategyRandomPath).new()
		"base_power_only":
			return load(DraftStrategyBasePowerOnlyPath).new(stats_dir)
		"native_softmax":
			return load(DraftStrategyNativeSoftmaxPath).new(stats_dir, "normal")
		"native_softmax_easy":
			return load(DraftStrategyNativeSoftmaxPath).new(stats_dir, "easy")
		"native_softmax_hard":
			return load(DraftStrategyNativeSoftmaxPath).new(stats_dir, "hard")
		"native_softmax_safe":
			return load(DraftStrategyNativeSoftmaxPath).new(stats_dir, "normal", DraftAiConfigScript.RISK_SAFE_CONFIG_PATH, "native_softmax_safe")
		"native_softmax_ceiling":
			return load(DraftStrategyNativeSoftmaxPath).new(stats_dir, "normal", DraftAiConfigScript.RISK_CEILING_CONFIG_PATH, "native_softmax_ceiling")
		"native_softmax_counter_heavy":
			return load(DraftStrategyNativeSoftmaxPath).new(stats_dir, "normal", DraftAiConfigScript.COUNTER_HEAVY_CONFIG_PATH, "native_softmax_counter_heavy")
		"native_lookahead":
			return load(DraftStrategyNativeLookaheadPath).new(stats_dir)
		"native_lookahead_pick_only":
			return load(DraftStrategyNativeLookaheadPickPath).new(stats_dir)
		"native_lookahead_ban_only":
			return load(DraftStrategyNativeLookaheadBanPath).new(stats_dir)
		"native_lookahead_softmax":
			return load(DraftStrategyNativeLookaheadSoftmaxPath).new(stats_dir)
		"native_learned_scorer":
			var learned_scorer = load(DraftStrategyNativeLearnedScorerPath).new(stats_dir)
			if learned_scorer.has_method("is_ready") and not bool(learned_scorer.call("is_ready")):
				push_error("DraftHarnessCore: native_learned_scorer unavailable: %s" % String(learned_scorer.call("last_error")))
				return null
			return learned_scorer
		_:
			if name.begins_with("native_ablation_"):
				var variant: String = name.substr("native_ablation_".length())
				var ablation = load(DraftStrategyNativeAblationPath).new(stats_dir, variant)
				if ablation._pick_zero.is_empty() and ablation._ban_zero.is_empty():
					push_error("DraftHarnessCore: unknown ablation variant '%s'" % variant)
					return null
				return ablation
			push_error("DraftHarnessCore: unknown strategy '%s'" % name)
			return null


static func build_strategy_map(names: Array[String], stats_dir: String) -> Dictionary:
	var out: Dictionary = {}
	for name in names:
		out[name] = build_strategy(name, stats_dir)
	return out


static func validate_strategy_map(map: Dictionary, side: String, context: String = "DraftHarnessCore") -> bool:
	var valid: bool = true
	for name in map.keys():
		if map[name] == null:
			push_error("%s: failed to build %s strategy '%s'" % [context, side, name])
			valid = false
	return valid


static func normalize_dir_path(path: String) -> String:
	var normalized: String = path.strip_edges()
	while normalized.ends_with("/"):
		normalized = normalized.substr(0, normalized.length() - 1)
	return normalized


static func dedupe_strategy_names(names: Array[String]) -> Array[String]:
	var seen: Dictionary = {}
	var out: Array[String] = []
	for name in names:
		if seen.has(name):
			continue
		seen[name] = true
		out.append(name)
	return out


static func validate_strategy_names_nonempty(names: Array[String], side: String, context: String) -> bool:
	if names.is_empty():
		push_error("%s: at least one %s strategy required" % [context, side])
		return false
	return true


static func shuffle_pool(rng: RandomNumberGenerator, arr: Array) -> void:
	for i in range(arr.size() - 1, 0, -1):
		var j: int = rng.randi() % (i + 1)
		var tmp = arr[i]
		arr[i] = arr[j]
		arr[j] = tmp


static func build_units(ids: Array, team: StringName) -> Array:
	var units: Array = []
	for index in range(ids.size()):
		var pos: Vector2 = SimConstantsScript.spawn_position(index, team)
		units.append(SpawnSpecScript.new(ids[index], team, pos.x, pos.y))
	return units


static func simulate_teams(backend: RefCounted, blue_team: Array, red_team: Array, sims: int, seed: int) -> Array:
	if backend == null or not backend.has_method("run_matches_stats"):
		push_error("DraftHarnessCore: backend missing run_matches_stats()")
		return []

	var player_units: Array = build_units(blue_team, &"player")
	var enemy_units: Array = build_units(red_team, &"enemy")

	var inputs: Array = []
	for s in range(sims):
		inputs.append(MatchReplayInputScript.new(
			seed + s * 1000, SimConstantsScript.DEFAULT_TICK_RATE,
			player_units, enemy_units
		))

	var summaries_var: Variant = backend.run_matches_stats(inputs)
	if typeof(summaries_var) != TYPE_ARRAY:
		push_error("DraftHarnessCore: run_matches_stats returned non-array")
		return []
	return summaries_var


static func summarize_sim_results(summaries: Array) -> Dictionary:
	var blue_wins: int = 0
	var red_wins: int = 0
	var draws: int = 0
	for summary in summaries:
		var winner_value: Variant
		if summary is Dictionary:
			winner_value = summary.get("winner_team", "")
		else:
			winner_value = summary.winner_team
		var winner_team: String = String(winner_value)
		if winner_team == "player":
			blue_wins += 1
		elif winner_team == "enemy":
			red_wins += 1
		else:
			draws += 1
	var total: int = blue_wins + red_wins + draws
	if total == 0:
		total = 1
	return {
		"blue_wins": blue_wins,
		"red_wins": red_wins,
		"draws": draws,
		"blue_winrate": float(blue_wins) / float(total),
		"red_winrate": float(red_wins) / float(total),
		"drawrate": float(draws) / float(total),
	}


static func enrich_summary_teams(summary: Variant, blue_picks: Array, red_picks: Array) -> Variant:
	if not summary is Dictionary:
		return summary
	var enriched: Dictionary = (summary as Dictionary).duplicate(true)
	if Array(enriched.get("player_comp", [])).is_empty() and not blue_picks.is_empty():
		enriched["player_comp"] = _string_array(blue_picks)
	if Array(enriched.get("enemy_comp", [])).is_empty() and not red_picks.is_empty():
		enriched["enemy_comp"] = _string_array(red_picks)
	return enriched


static func record_matchup_from_summary(
	matchup_tracker: Object,
	summary: Variant,
	blue_picks: Array = [],
	red_picks: Array = []
) -> void:
	if matchup_tracker == null or not matchup_tracker.has_method("process_match_result"):
		return

	var enriched: Variant = enrich_summary_teams(summary, blue_picks, red_picks)
	var winners: Array[StringName] = []
	var losers: Array[StringName] = []
	var player_comp: Array = []
	var enemy_comp: Array = []
	var winner_team: StringName = &""

	if enriched is Dictionary:
		var summary_dict: Dictionary = enriched
		winner_team = StringName(String(summary_dict.get("winner_team", "")))
		player_comp = Array(summary_dict.get("player_comp", []))
		enemy_comp = Array(summary_dict.get("enemy_comp", []))
	else:
		winner_team = enriched.winner_team
		player_comp = Array(enriched.player_comp)
		enemy_comp = Array(enriched.enemy_comp)

	if winner_team == &"player":
		winners = _string_name_array(player_comp)
		losers = _string_name_array(enemy_comp)
	elif winner_team == &"enemy":
		winners = _string_name_array(enemy_comp)
		losers = _string_name_array(player_comp)

	if not winners.is_empty() and not losers.is_empty():
		matchup_tracker.process_match_result(winners, losers)


static func _string_array(values: Array) -> Array[String]:
	var out: Array[String] = []
	for value in values:
		out.append(String(value))
	return out


static func _string_name_array(values: Array) -> Array[StringName]:
	var out: Array[StringName] = []
	for value in values:
		out.append(StringName(String(value)))
	return out


static func run_full_draft(
	available: Array[StringName],
	blue_strat: Object,
	red_strat: Object,
	backend: RefCounted,
	stats_dir: String,
	sims_per_draft: int,
	draft_seed: int,
	include_diagnostics: bool = false,
	include_decision_state: bool = false
) -> Dictionary:
	var blue_picks: Array[StringName] = []
	var red_picks: Array[StringName] = []
	var blue_bans: Array[StringName] = []
	var red_bans: Array[StringName] = []
	var step_records: Array[Dictionary] = []
	var sequence: Array = SimConstantsScript.DRAFT_SEQUENCE

	for step_index in range(sequence.size()):
		var turn: String = sequence[step_index]
		var side: String = turn.substr(0, 1)
		var action: String = turn.substr(2)
		var allies: Array[StringName] = blue_picks if side == "B" else red_picks
		var enemies: Array[StringName] = red_picks if side == "B" else blue_picks
		var strategy = blue_strat if side == "B" else red_strat
		var acting_side: String = "blue" if side == "B" else "red"
		var blue_picks_before: Array[StringName] = blue_picks.duplicate()
		var red_picks_before: Array[StringName] = red_picks.duplicate()
		var blue_bans_before: Array[StringName] = blue_bans.duplicate()
		var red_bans_before: Array[StringName] = red_bans.duplicate()
		var legal_pool_before: Array[StringName] = available.duplicate()
		if strategy.has_method("set_draft_context"):
			strategy.call(
				"set_draft_context",
				blue_picks_before,
				red_picks_before,
				blue_bans_before,
				red_bans_before,
				acting_side
			)

		var chosen: StringName
		if action == "PICK":
			chosen = strategy.recommend_next_pick(allies, enemies, available, step_index)
		else:
			chosen = strategy.recommend_next_ban(allies, enemies, available, step_index, acting_side, {})

		if chosen.is_empty() or not chosen in available:
			push_error("DraftHarnessCore: invalid selection at step %d side %s" % [step_index, side])
			chosen = available[0]

		var diag: Dictionary = {}
		if include_diagnostics:
			diag = diagnostic_for_chosen(
				backend, stats_dir, action, allies, enemies, available, step_index, acting_side, chosen
			)

		var step_record: Dictionary = {
			"step_index": step_index,
			"side": side,
			"action": action,
			"chosen": chosen,
			"diagnostic": diag,
		}
		if include_decision_state:
			step_record["acting_side"] = acting_side
			step_record["blue_picks_before"] = blue_picks_before
			step_record["red_picks_before"] = red_picks_before
			step_record["blue_bans_before"] = blue_bans_before
			step_record["red_bans_before"] = red_bans_before
			step_record["legal_pool"] = legal_pool_before
		step_records.append(step_record)

		available.erase(chosen)
		if action == "PICK":
			if side == "B":
				blue_picks.append(chosen)
			else:
				red_picks.append(chosen)
		else:
			if side == "B":
				blue_bans.append(chosen)
			else:
				red_bans.append(chosen)

	var summaries: Array = simulate_teams(backend, blue_picks, red_picks, sims_per_draft, draft_seed)
	var sim: Dictionary = summarize_sim_results(summaries)

	return {
		"blue_picks": blue_picks,
		"red_picks": red_picks,
		"blue_bans": blue_bans,
		"red_bans": red_bans,
		"step_records": step_records,
		"summaries": summaries,
		"sim": sim,
	}


static func diagnostic_for_chosen(
	backend: RefCounted,
	stats_dir: String,
	action: String,
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	step_index: int,
	acting_side: String,
	chosen: StringName
) -> Dictionary:
	var diag: Dictionary = {
		"base_power": 0.0,
		"ally_synergy": 0.0,
		"enemy_counter_value": 0.0,
		"counter_risk": 0.0,
		"role_fit": 0.0,
		"comp_fit": 0.0,
		"total_score": 0.0,
		"phase_label": "",
		"candidate_role": "",
		"comp_fingerprint": "",
	}
	if backend == null or not backend.is_available():
		return diag

	var recommendations: Array
	if action == "PICK":
		recommendations = backend.get_draft_ai_pick_recommendations(
			stats_dir, available, allies, enemies, available.size(), step_index
		)
	else:
		recommendations = backend.get_draft_ai_ban_recommendations(
			stats_dir, available, allies, enemies, available.size(), step_index, acting_side, {}
		)

	for rec in recommendations:
		if StringName(rec.get("candidate", "")) == chosen:
			if action == "PICK":
				diag["base_power"] = float(rec.get("base_power", 0.0))
				diag["ally_synergy"] = float(rec.get("ally_synergy", 0.0))
				diag["enemy_counter_value"] = float(rec.get("enemy_counter_value", 0.0))
				diag["counter_risk"] = float(rec.get("counter_risk", 0.0))
				diag["role_fit"] = float(rec.get("role_fit", 0.0))
				diag["comp_fit"] = float(rec.get("comp_fit", 0.0))
				diag["comp_fingerprint"] = String(rec.get("comp_fingerprint", ""))
			else:
				diag["base_power"] = float(rec.get("enemy_pick_value", 0.0))
				diag["ally_synergy"] = float(rec.get("enemy_synergy", 0.0))
				diag["enemy_counter_value"] = float(rec.get("counters_my_team", 0.0))
				diag["counter_risk"] = 0.0
				diag["role_fit"] = float(rec.get("fills_enemy_role_need", 0.0))
				diag["comp_fit"] = float(rec.get("enemy_comp_fit", 0.0))
				diag["comp_fingerprint"] = String(rec.get("enemy_comp_fingerprint", ""))
			diag["total_score"] = float(rec.get("total_score", 0.0))
			diag["phase_label"] = String(rec.get("phase_label", ""))
			diag["candidate_role"] = String(rec.get("candidate_role", ""))
			break
	return diag
