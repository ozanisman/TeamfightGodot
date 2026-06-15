extends SceneTree

## Full Draft A/B Test Runner
## Compares strategies by running complete snake drafts, then simulating the resulting teams
##
## Usage:
##   godot --headless --script res://scripts/tools/full_draft_ab_test.gd \
##     -- --trials=25 --sims-per-draft=25 --strategies=native,random
##   godot --headless --script res://scripts/tools/full_draft_ab_test.gd \
##     -- --trials=25 --sims-per-draft=25 --matchups=native:native,random:native

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftStrategyRandomPath := "res://scripts/tools/draft_strategy_random.gd"
const DraftStrategyNativePath := "res://scripts/tools/draft_strategy_native.gd"
const DraftStrategyNativeArchetypePath := "res://scripts/tools/draft_strategy_native_archetype.gd"
const DraftStrategyNativeLookaheadPath := "res://scripts/tools/draft_strategy_native_lookahead.gd"
const DraftStrategyNativeLookaheadPickPath := "res://scripts/tools/draft_strategy_native_lookahead_pick.gd"
const DraftStrategyNativeLookaheadBanPath := "res://scripts/tools/draft_strategy_native_lookahead_ban.gd"
const DraftStrategyNativePicksRandomBansPath := "res://scripts/tools/draft_strategy_native_picks_random_bans.gd"
const DraftStrategyRandomPicksNativeBansPath := "res://scripts/tools/draft_strategy_random_picks_native_bans.gd"
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SpawnSpecScript := preload("res://scripts/simulation/spawn_spec.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

const TEAM_SIZE: int = 5

## Draft sequence: side (B=blue, R=red) and action type
var _draft_sequence = [
	{"side": "B", "action": "BAN"},  # Step 0
	{"side": "R", "action": "BAN"},  # Step 1
	{"side": "B", "action": "BAN"},  # Step 2
	{"side": "R", "action": "BAN"},  # Step 3
	{"side": "B", "action": "BAN"},  # Step 4
	{"side": "R", "action": "BAN"},  # Step 5
	{"side": "B", "action": "PICK"}, # Step 6
	{"side": "R", "action": "PICK"}, # Step 7
	{"side": "R", "action": "PICK"}, # Step 8
	{"side": "B", "action": "PICK"}, # Step 9
	{"side": "B", "action": "PICK"}, # Step 10
	{"side": "R", "action": "PICK"}, # Step 11
	{"side": "R", "action": "BAN"},  # Step 12
	{"side": "B", "action": "BAN"},  # Step 13
	{"side": "R", "action": "BAN"},  # Step 14
	{"side": "B", "action": "BAN"},  # Step 15
	{"side": "R", "action": "PICK"}, # Step 16
	{"side": "B", "action": "PICK"}, # Step 17
	{"side": "B", "action": "PICK"}, # Step 18
	{"side": "R", "action": "PICK"}  # Step 19
]

var _stats_dir: String = "res://stats_output_100k"
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

	var trials := maxi(1, int(_extract_argument("--trials=", "25")))
	var sims_per_draft := maxi(1, int(_extract_argument("--sims-per-draft=", "25")))
	var strategies_str := _extract_argument("--strategies=", "native,random")
	var matchups_str := _extract_argument("--matchups=", "")
	var base_seed := int(_extract_argument("--base-seed=", "90000"))

	var strategy_names: Array[String] = []
	var requested_matchups: Array[Dictionary] = []
	if not matchups_str.is_empty():
		for matchup in matchups_str.split(","):
			var sides := matchup.split(":")
			if sides.size() != 2:
				push_error("full_draft_ab_test: invalid matchup '%s'" % matchup)
				await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
				return
			var blue_name := sides[0].strip_edges()
			var red_name := sides[1].strip_edges()
			requested_matchups.append({"blue": blue_name, "red": red_name})
			if not strategy_names.has(blue_name):
				strategy_names.append(blue_name)
			if not strategy_names.has(red_name):
				strategy_names.append(red_name)
	else:
		for s in strategies_str.split(","):
			strategy_names.append(s.strip_edges())
		for i in range(strategy_names.size()):
			for j in range(strategy_names.size()):
				requested_matchups.append({"blue": strategy_names[i], "red": strategy_names[j]})

	_backend = NativeSimulationBackendScript.new()
	if not _backend.is_available():
		push_error("full_draft_ab_test: native backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not _backend.has_method("run_matches_stats"):
		push_error("full_draft_ab_test: backend missing run_matches_stats()")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("full_draft_ab_test: not enough champions")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	print("full_draft_ab_test: trials=%d sims/draft=%d strategies=%s" % [
		trials, sims_per_draft, ",".join(strategy_names)
	])

	# Build strategy instances
	var strategies: Dictionary = {}
	for name in strategy_names:
		match name:
			"random":
				strategies[name] = load(DraftStrategyRandomPath).new()
			"native":
				strategies[name] = load(DraftStrategyNativePath).new(_stats_dir)
			"native_archetype":
				strategies[name] = load(DraftStrategyNativeArchetypePath).new(_stats_dir)
			"archetype_full":
				strategies[name] = load(DraftStrategyNativeArchetypePath).new(_stats_dir, 4, "archetype_full")
			"archetype_light":
				strategies[name] = load(DraftStrategyNativeArchetypePath).new(_stats_dir, 5, "archetype_light")
			"archetype_pick_light":
				strategies[name] = load(DraftStrategyNativeArchetypePath).new(_stats_dir, 6, "archetype_pick_light")
			"archetype_ban_light":
				strategies[name] = load(DraftStrategyNativeArchetypePath).new(_stats_dir, 7, "archetype_ban_light")
			"native_lookahead":
				strategies[name] = load(DraftStrategyNativeLookaheadPath).new(_stats_dir)
			"native_lookahead_pick_only":
				strategies[name] = load(DraftStrategyNativeLookaheadPickPath).new(_stats_dir)
			"native_lookahead_ban_only":
				strategies[name] = load(DraftStrategyNativeLookaheadBanPath).new(_stats_dir)
			"native_picks_random_bans":
				strategies[name] = load(DraftStrategyNativePicksRandomBansPath).new(_stats_dir)
			"random_picks_native_bans":
				strategies[name] = load(DraftStrategyRandomPicksNativeBansPath).new(_stats_dir)
			_:
				push_error("full_draft_ab_test: unknown strategy '%s'" % name)
				await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
				return

	# Pairwise comparisons
	var report_lines = []
	report_lines.append("=== FULL DRAFT A/B TEST RESULTS ===")
	report_lines.append("Trials: %d" % trials)
	report_lines.append("Simulations per draft: %d" % sims_per_draft)
	report_lines.append("Strategies: %s" % ",".join(strategy_names))
	if not matchups_str.is_empty():
		report_lines.append("Matchups: %s" % matchups_str)
	report_lines.append("")

	for matchup_index in range(requested_matchups.size()):
		var strategy_a = String(requested_matchups[matchup_index]["blue"])
		var strategy_b = String(requested_matchups[matchup_index]["red"])

		report_lines.append("--- Matchup: %s (Blue) vs %s (Red) ---" % [strategy_a, strategy_b])

		var results = _run_matchup(
			strategies[strategy_a],
			strategies[strategy_b],
			trials,
			sims_per_draft,
			base_seed + matchup_index * 100,
			champion_ids
		)

		report_lines.append("Blue (%s) wins: %d (%.1f%%)" % [strategy_a, results.blue_wins, results.blue_winrate * 100.0])
		report_lines.append("Red (%s) wins: %d (%.1f%%)" % [strategy_b, results.red_wins, results.red_winrate * 100.0])
		report_lines.append("Draws: %d (%.1f%%)" % [results.draws, results.drawrate * 100.0])
		report_lines.append("Invalid drafts: %d" % results.invalid_drafts)
		report_lines.append("Average duration: %.2f ticks" % results.avg_duration)
		report_lines.append("")

		if results.sample_draft.size() > 0:
			report_lines.append("Sample draft:")
			report_lines.append("  Blue picks: " + str(results.sample_draft.blue_picks))
			report_lines.append("  Red picks: " + str(results.sample_draft.red_picks))
			report_lines.append("  Blue bans: " + str(results.sample_draft.blue_bans))
			report_lines.append("  Red bans: " + str(results.sample_draft.red_bans))
			report_lines.append("")

	report_lines.append("=== TEST COMPLETE ===")
	_write_report(report_lines)
	
	print("Full draft A/B test complete. Report written to full_draft_ab_test_report.txt")
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)


func _run_matchup(strategy_a: RefCounted, strategy_b: RefCounted, trials: int, sims_per_draft: int, base_seed: int, champion_ids: Array[StringName]) -> Dictionary:
	var blue_wins = 0
	var red_wins = 0
	var draws = 0
	var invalid_drafts = 0
	var total_duration = 0.0
	var total_matches = 0
	
	var sample_draft = {}
	
	for trial in range(trials):
		var seed = base_seed + trial
		seed_rng(seed)
		
		if trial % 5 == 0:
			print("Matchup %s vs %s: Trial %d/%d" % [strategy_a.get_strategy_name(), strategy_b.get_strategy_name(), trial, trials])
		
		var available = champion_ids.duplicate()
		available.shuffle()
		
		var blue_picks = []
		var red_picks = []
		var blue_bans = []
		var red_bans = []
		
		var draft_valid = true
		
		# Run full snake draft
		for step_idx in range(_draft_sequence.size()):
			var step_data = _draft_sequence[step_idx]
			var side = step_data.side
			var action = step_data.action
			var draft_step = step_idx
			
			var allies = []
			var enemies = []
			
			if side == "B":
				allies = blue_picks.duplicate()
				enemies = red_picks.duplicate()
			else:
				allies = red_picks.duplicate()
				enemies = blue_picks.duplicate()
			
			var strategy = strategy_a if side == "B" else strategy_b
			var selected_champion = ""
			
			if action == "PICK":
				selected_champion = strategy.recommend_next_pick(allies, enemies, available, draft_step)
			else:
				selected_champion = strategy.recommend_next_ban(allies, enemies, available, draft_step, side)
			
			if selected_champion.is_empty() or not selected_champion in available:
				draft_valid = false
				break
			
			available.erase(selected_champion)
			
			if action == "PICK":
				if side == "B":
					blue_picks.append(selected_champion)
				else:
					red_picks.append(selected_champion)
			else:
				if side == "B":
					blue_bans.append(selected_champion)
				else:
					red_bans.append(selected_champion)
		
		if not draft_valid or blue_picks.size() != TEAM_SIZE or red_picks.size() != TEAM_SIZE:
			invalid_drafts += 1
			continue
		
		# Save sample draft from first valid trial
		if sample_draft.is_empty():
			sample_draft = {
				"blue_picks": blue_picks.duplicate(),
				"red_picks": red_picks.duplicate(),
				"blue_bans": blue_bans.duplicate(),
				"red_bans": red_bans.duplicate()
			}
		
		# Simulate the matchup
		var match_result = _simulate_matchup(blue_picks, red_picks, sims_per_draft, seed)
		
		blue_wins += match_result.blue_wins
		red_wins += match_result.red_wins
		draws += match_result.draws
		total_duration += match_result.total_duration
		total_matches += match_result.total_matches
	
	var total_trials = trials - invalid_drafts
	var blue_winrate = 0.0
	var red_winrate = 0.0
	var drawrate = 0.0
	var avg_duration = 0.0
	
	if total_matches > 0:
		blue_winrate = float(blue_wins) / float(total_matches)
		red_winrate = float(red_wins) / float(total_matches)
		drawrate = float(draws) / float(total_matches)
		avg_duration = total_duration / float(total_matches)
	
	return {
		"blue_wins": blue_wins,
		"red_wins": red_wins,
		"draws": draws,
		"blue_winrate": blue_winrate,
		"red_winrate": red_winrate,
		"drawrate": drawrate,
		"invalid_drafts": invalid_drafts,
		"avg_duration": avg_duration,
		"sample_draft": sample_draft
	}


func _simulate_matchup(blue_team: Array, red_team: Array, sims: int, seed: int) -> Dictionary:
	var blue_wins = 0
	var red_wins = 0
	var draws = 0
	var total_duration = 0.0
	
	# Build units
	var player_units = _build_units(blue_team, &"player")
	var enemy_units = _build_units(red_team, &"enemy")
	
	# Build simulation inputs
	var inputs: Array = []
	for s in range(sims):
		inputs.append(MatchReplayInputScript.new(
			seed + s * 1000, SimConstantsScript.DEFAULT_TICK_RATE,
			player_units, enemy_units
		))
	
	var summaries_var: Variant = _backend.run_matches_stats(inputs)
	if typeof(summaries_var) != TYPE_ARRAY:
		push_error("full_draft_ab_test: run_matches_stats did not return array")
		return {"blue_wins": 0, "red_wins": 0, "draws": 0, "total_duration": 0.0, "total_matches": 0}
	
	var summaries: Array = summaries_var
	if summaries.is_empty():
		return {"blue_wins": 0, "red_wins": 0, "draws": 0, "total_duration": 0.0, "total_matches": 0}
	
	for summary in summaries:
		if summary.has("winner_team"):
			if summary.winner_team == "player":
				blue_wins += 1
			elif summary.winner_team == "enemy":
				red_wins += 1
			else:
				draws += 1
		
		if summary.has("duration"):
			total_duration += summary.duration
	
	return {
		"blue_wins": blue_wins,
		"red_wins": red_wins,
		"draws": draws,
		"total_duration": total_duration,
		"total_matches": sims
	}


func _build_units(ids: Array, team: StringName) -> Array:
	var units: Array = []
	for index in range(ids.size()):
		var pos = SimConstantsScript.spawn_position(index, team)
		units.append(SpawnSpecScript.new(ids[index], team, pos.x, pos.y))
	return units


func seed_rng(seed: int) -> void:
	seed(seed)


func _write_report(lines: Array) -> void:
	var output_path = "res://full_draft_ab_test_report.txt"
	var f = FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f:
		f.store_string("\n".join(lines))
		f.flush()
		f.close()
