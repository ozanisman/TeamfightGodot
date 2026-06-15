extends SceneTree

## Full Draft Ablation Test Runner
## Measures contribution of picks vs bans by testing mixed strategies
##
## Usage:
##   godot --headless --script res://scripts/tools/full_draft_ablation_test.gd \
##     -- --trials=25 --sims-per-draft=25

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftStrategyRandomPath := preload("res://scripts/tools/draft_strategy_random.gd")
const DraftStrategyNativePath := preload("res://scripts/tools/draft_strategy_native.gd")
const DraftStrategyNativePicksRandomBansPath := preload("res://scripts/tools/draft_strategy_native_picks_random_bans.gd")
const DraftStrategyRandomPicksNativeBansPath := preload("res://scripts/tools/draft_strategy_random_picks_native_bans.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SpawnSpecScript := preload("res://scripts/simulation/spawn_spec.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

const TEAM_SIZE: int = 5

## Draft order configurations
var _draft_orders: Dictionary = {
	"current": [
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
	],
	"mirrored_current": [
		{"side": "R", "action": "BAN"},  # Step 0
		{"side": "B", "action": "BAN"},  # Step 1
		{"side": "R", "action": "BAN"},  # Step 2
		{"side": "B", "action": "BAN"},  # Step 3
		{"side": "R", "action": "BAN"},  # Step 4
		{"side": "B", "action": "BAN"},  # Step 5
		{"side": "R", "action": "PICK"}, # Step 6
		{"side": "B", "action": "PICK"}, # Step 7
		{"side": "B", "action": "PICK"}, # Step 8
		{"side": "R", "action": "PICK"}, # Step 9
		{"side": "R", "action": "PICK"}, # Step 10
		{"side": "B", "action": "PICK"}, # Step 11
		{"side": "B", "action": "BAN"},  # Step 12
		{"side": "R", "action": "BAN"},  # Step 13
		{"side": "B", "action": "BAN"},  # Step 14
		{"side": "R", "action": "BAN"},  # Step 15
		{"side": "B", "action": "PICK"}, # Step 16
		{"side": "R", "action": "PICK"}, # Step 17
		{"side": "R", "action": "PICK"}, # Step 18
		{"side": "B", "action": "PICK"}  # Step 19
	],
	"red_first_pick_variant": [
		{"side": "B", "action": "BAN"},  # Step 0
		{"side": "R", "action": "BAN"},  # Step 1
		{"side": "B", "action": "BAN"},  # Step 2
		{"side": "R", "action": "BAN"},  # Step 3
		{"side": "B", "action": "BAN"},  # Step 4
		{"side": "R", "action": "BAN"},  # Step 5
		{"side": "R", "action": "PICK"}, # Step 6 - Red gets first pick
		{"side": "B", "action": "PICK"}, # Step 7
		{"side": "B", "action": "PICK"}, # Step 8
		{"side": "R", "action": "PICK"}, # Step 9
		{"side": "R", "action": "PICK"}, # Step 10
		{"side": "B", "action": "PICK"}, # Step 11
		{"side": "R", "action": "BAN"},  # Step 12
		{"side": "B", "action": "BAN"},  # Step 13
		{"side": "R", "action": "BAN"},  # Step 14
		{"side": "B", "action": "BAN"},  # Step 15
		{"side": "B", "action": "PICK"}, # Step 16 - Blue gets late pick
		{"side": "R", "action": "PICK"}, # Step 17
		{"side": "R", "action": "PICK"}, # Step 18
		{"side": "B", "action": "PICK"}  # Step 19
	],
	"no_final_blue_pair_variant": [
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
		{"side": "R", "action": "PICK"}, # Step 18 - Split final pair
		{"side": "B", "action": "PICK"}  # Step 19
	]
}

## Draft sequence: side (B=blue, R=red) and action type (default: current)
var _draft_sequence = []

var _stats_dir: String = "res://model_stats/stats_output_100k"
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

	var trials := maxi(1, int(_extract_argument("--trials=", "50")))
	var sims_per_draft := maxi(1, int(_extract_argument("--sims-per-draft=", "25")))
	var base_seed := int(_extract_argument("--base-seed=", "90000"))
	var weight_profile_name := _extract_argument("--weight-profile=", "current")
	var draft_order_name := _extract_argument("--draft-order=", "current")

	# Select draft order
	if draft_order_name in _draft_orders:
		_draft_sequence = _draft_orders[draft_order_name]
	else:
		push_error("Unknown draft order: %s. Available: %s" % [draft_order_name, ", ".join(_draft_orders.keys())])
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	# Define weight profiles (multipliers applied to phase-specific weights)
	var weight_profiles = {
		"current": {},
		"denial_heavier": {"denial_value_multiplier": 1.25},
		"denial_lighter": {"denial_value_multiplier": 0.75},
		"enemy_specific_heavier": {"enemy_synergy_multiplier": 1.25, "counters_my_team_multiplier": 1.25, "fills_enemy_role_need_multiplier": 1.25},
		"fallback_heavier": {"early_fallback_multiplier": 0.40},
		"fallback_lighter": {"early_fallback_multiplier": 0.20}
	}

	var weight_overrides = weight_profiles.get(weight_profile_name, {})

	_backend = NativeSimulationBackendScript.new()
	if not _backend.is_available():
		push_error("full_draft_ablation_test: native backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not _backend.has_method("run_matches_stats"):
		push_error("full_draft_ablation_test: backend missing run_matches_stats()")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	# Guard: validate native stats files exist
	var required_files := ["combat_stats.csv", "matchup_with.csv", "matchup_vs.csv"]
	for f in required_files:
		var path := _stats_dir.path_join(f)
		if not FileAccess.file_exists(path):
			push_error("full_draft_ablation_test: missing required stats file '%s'" % path)
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return
	print("full_draft_ablation_test: required stats files found in %s" % _stats_dir)

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("full_draft_ablation_test: not enough champions")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	print("full_draft_ablation_test: trials=%d sims/draft=%d weight_profile=%s" % [trials, sims_per_draft, weight_profile_name])

	# Build strategy instances
	var strategies: Dictionary = {}
	strategies["native_full"] = DraftStrategyNativePath.new(_stats_dir)
	strategies["native_full"].set_weight_overrides(weight_overrides)
	strategies["native_picks_random_bans"] = DraftStrategyNativePicksRandomBansPath.new(_stats_dir)
	strategies["native_picks_random_bans"].set_weight_overrides(weight_overrides)
	strategies["random_picks_native_bans"] = DraftStrategyRandomPicksNativeBansPath.new(_stats_dir)
	strategies["random_picks_native_bans"].set_weight_overrides(weight_overrides)
	strategies["random_full"] = DraftStrategyRandomPath.new()

	# Required matchups for draft order bias audit (side-swapped)
	var matchups = [
		["native_full", "native_full"],  # Self-play bias
		["random_full", "random_full"],  # Random self-play bias
		["native_full", "random_full"],  # Native vs random
		["random_full", "native_full"]   # Random vs native
	]

	var report_lines = []
	report_lines.append("=== DRAFT ORDER BIAS AUDIT RESULTS ===")
	report_lines.append("Draft order: %s" % draft_order_name)
	report_lines.append("Trials: %d" % trials)
	report_lines.append("Simulations per draft: %d" % sims_per_draft)
	report_lines.append("")

	for matchup_idx in range(matchups.size()):
		var strategy_a_name = matchups[matchup_idx][0]
		var strategy_b_name = matchups[matchup_idx][1]

		report_lines.append("--- Matchup: %s (Blue) vs %s (Red) ---" % [strategy_a_name, strategy_b_name])

		var results = _run_matchup(
			strategies[strategy_a_name],
			strategies[strategy_b_name],
			trials,
			sims_per_draft,
			base_seed + matchup_idx * 10000,
			champion_ids
		)

		report_lines.append("Blue (%s) wins: %d (%.1f%%)" % [strategy_a_name, results.blue_wins, results.blue_winrate * 100.0])
		report_lines.append("Red (%s) wins: %d (%.1f%%)" % [strategy_b_name, results.red_wins, results.red_winrate * 100.0])
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

	report_lines.append("=== ANALYSIS ===")
	report_lines.append("Comparing win rates to determine contribution of picks vs bans:")
	report_lines.append("- native_full vs random_full: baseline native advantage")
	report_lines.append("- native_picks_random_bans vs random_full: picks-only contribution")
	report_lines.append("- random_picks_native_bans vs random_full: bans-only contribution")
	report_lines.append("- native_full vs native_picks_random_bans: bans contribution")
	report_lines.append("- native_full vs random_picks_native_bans: picks contribution")
	report_lines.append("")
	report_lines.append("=== TEST COMPLETE ===")
	_write_report(report_lines)

	print("Full draft ablation test complete. Report written to full_draft_ablation_report.txt")
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
		push_error("full_draft_ablation_test: run_matches_stats did not return array")
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
	var output_path = "res://logs/full_draft_ablation_report.txt"
	var f = FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f:
		f.store_string("\n".join(lines))
		f.flush()
		f.close()
