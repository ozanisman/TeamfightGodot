extends SceneTree

## Ban Weight Calibration Test Runner
## Tests different ban scoring weight profiles to find optimal configuration
##
## Usage:
##   godot --headless --script res://scripts/tools/ban_weight_calibration.gd \
##     -- --trials=50 --sims-per-draft=25

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

## Weight profiles to test
var _weight_profiles = {
	"current": {
		"early_fallback_multiplier": 0.30
	},
	"denial_heavier": {
		"denial_value_weight": 0.9375,  # +25% from 0.75 in phase 1
		"early_fallback_multiplier": 0.30
	},
	"denial_lighter": {
		"denial_value_weight": 0.5625,  # -25% from 0.75 in phase 1
		"early_fallback_multiplier": 0.30
	},
	"enemy_specific_heavier": {
		"enemy_synergy_weight": 0.1875,  # +25% from 0.15 in phase 1
		"counters_my_team_weight": 0.25,  # +25% from 0.20 in phase 1
		"fills_enemy_role_need_weight": 0.0625,  # +25% from 0.05 in phase 1
		"early_fallback_multiplier": 0.30
	},
	"fallback_heavier": {
		"early_fallback_multiplier": 0.40
	},
	"fallback_lighter": {
		"early_fallback_multiplier": 0.20
	}
}


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

	_backend = NativeSimulationBackendScript.new()
	if not _backend.is_available():
		push_error("ban_weight_calibration: native backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not _backend.has_method("run_matches_stats"):
		push_error("ban_weight_calibration: backend missing run_matches_stats()")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("ban_weight_calibration: not enough champions")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	print("ban_weight_calibration: trials=%d sims/draft=%d" % [trials, sims_per_draft])

	var report_lines = []
	report_lines.append("=== BAN WEIGHT CALIBRATION RESULTS ===")
	report_lines.append("Trials: %d" % trials)
	report_lines.append("Simulations per draft: %d" % sims_per_draft)
	report_lines.append("")

	var profile_names = _weight_profiles.keys()
	
	for profile_idx in range(profile_names.size()):
		var profile_name = profile_names[profile_idx]
		var weight_overrides = _weight_profiles[profile_name]
		
		report_lines.append("--- Profile: %s ---" % profile_name)
		report_lines.append("Weight overrides: %s" % str(weight_overrides))
		report_lines.append("")
		
		# Build strategy instances with this profile
		var strategies: Dictionary = {}
		strategies["native_full"] = DraftStrategyNativePath.new(_stats_dir)
		strategies["native_full"].set_weight_overrides(weight_overrides)
		strategies["native_picks_random_bans"] = DraftStrategyNativePicksRandomBansPath.new(_stats_dir)
		strategies["native_picks_random_bans"].set_weight_overrides(weight_overrides)
		strategies["random_picks_native_bans"] = DraftStrategyRandomPicksNativeBansPath.new(_stats_dir)
		strategies["random_picks_native_bans"].set_weight_overrides(weight_overrides)
		strategies["random_full"] = DraftStrategyRandomPath.new()
		
		# Required matchups for this profile
		var matchups = [
			["native_full", "random_full"],
			["native_full", "native_picks_random_bans"],
			["random_picks_native_bans", "random_full"]
		]
		
		for matchup_idx in range(matchups.size()):
			var strategy_a_name = matchups[matchup_idx][0]
			var strategy_b_name = matchups[matchup_idx][1]
			
			report_lines.append("Matchup: %s (Blue) vs %s (Red)" % [strategy_a_name, strategy_b_name])
			
			var results = _run_matchup(
				strategies[strategy_a_name],
				strategies[strategy_b_name],
				trials,
				sims_per_draft,
				base_seed + profile_idx * 10000 + matchup_idx * 1000,
				champion_ids,
				weight_overrides
			)
			
			report_lines.append("  Blue wins: %d (%.1f%%)" % [results.blue_wins, results.blue_winrate * 100.0])
			report_lines.append("  Red wins: %d (%.1f%%)" % [results.red_wins, results.red_winrate * 100.0])
			report_lines.append("  Invalid drafts: %d" % results.invalid_drafts)
			
			if results.sample_draft.size() > 0:
				report_lines.append("  Sample bans (first 6): " + str(results.sample_draft.blue_bans.slice(0, 6)))
			report_lines.append("")
		
		report_lines.append("")

	report_lines.append("=== TEST COMPLETE ===")
	
	var report_file = FileAccess.open("user://ban_weight_calibration_report.txt", FileAccess.WRITE)
	if report_file:
		for line in report_lines:
			report_file.store_line(line)
		report_file.close()
		print("Report written to ban_weight_calibration_report.txt")
	else:
		push_error("Failed to write report file")
	
	# Print summary to console
	for line in report_lines:
		print(line)
	
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)


func _run_matchup(strategy_a, strategy_b, trials, sims_per_draft, seed, champion_ids, weight_overrides):
	var blue_wins = 0
	var red_wins = 0
	var invalid_drafts = 0
	var total_duration = 0.0
	var sample_draft = {}
	
	for trial in range(trials):
		var rng = RandomNumberGenerator.new()
		rng.seed = seed + trial
		
		var available = champion_ids.duplicate()
		available.shuffle()
		
		var blue_picks: Array[StringName] = []
		var red_picks: Array[StringName] = []
		var blue_bans: Array[StringName] = []
		var red_bans: Array[StringName] = []
		
		var draft_valid = true
		
		for step_idx in range(_draft_sequence.size()):
			var step = _draft_sequence[step_idx]
			var side = step.side
			var action = step.action
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
				var side_str = "blue" if side == "B" else "red"
				# Pass weight_overrides to native strategies
				if strategy.has_method("set_weight_overrides"):
					strategy.set_weight_overrides(weight_overrides)
				selected_champion = strategy.recommend_next_ban(allies, enemies, available, draft_step, side_str)
			
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
		
		# Run simulations
		var blue_wins_this_draft = 0
		var red_wins_this_draft = 0
		
		for sim in range(sims_per_draft):
			var replay_input = MatchReplayInputScript.new()
			replay_input.set_draft_sequence(blue_picks, red_picks, blue_bans, red_bans)
			
			var spawn_spec = SpawnSpecScript.new()
			spawn_spec.set_teams(blue_picks, red_picks)
			
			var match_result = _backend.run_matches_stats(
				_stats_dir,
				[spawn_spec],
				[replay_input],
				1,
				rng.randi()
			)
			
			if match_result.size() > 0:
				var result = match_result[0]
				if result.has("blue_wins"):
					blue_wins_this_draft += result.blue_wins
				if result.has("red_wins"):
					red_wins_this_draft += result.red_wins
				if result.has("duration"):
					total_duration += result.duration
		
		blue_wins += blue_wins_this_draft
		red_wins += red_wins_this_draft
		
		# Save sample draft from first valid trial
		if sample_draft.size() == 0 and draft_valid:
			sample_draft = {
				"blue_picks": blue_picks,
				"red_picks": red_picks,
				"blue_bans": blue_bans,
				"red_bans": red_bans
			}
	
	var total_sims = trials * sims_per_draft
	return {
		"blue_wins": blue_wins,
		"red_wins": red_wins,
		"blue_winrate": float(blue_wins) / total_sims if total_sims > 0 else 0.0,
		"red_winrate": float(red_wins) / total_sims if total_sims > 0 else 0.0,
		"draws": 0,
		"drawrate": 0.0,
		"invalid_drafts": invalid_drafts,
		"avg_duration": total_duration / total_sims if total_sims > 0 else 0.0,
		"sample_draft": sample_draft
	}
