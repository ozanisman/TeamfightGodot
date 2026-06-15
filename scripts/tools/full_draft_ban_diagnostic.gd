extends SceneTree

## Full Draft Ban Diagnostic Runner
## Diagnoses why native bans underperform by reporting detailed scoring breakdown
##
## Usage:
##   godot --headless --script res://scripts/tools/full_draft_ban_diagnostic.gd \
##     -- --trials=5 --sims-per-draft=10

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftStrategyRandomPath := preload("res://scripts/tools/draft_strategy_random.gd")
const DraftStrategyNativePath := preload("res://scripts/tools/draft_strategy_native.gd")
const DraftStrategyNativePicksRandomBansPath := preload("res://scripts/tools/draft_strategy_native_picks_random_bans.gd")
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
var _diagnostic_lines: Array = []


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

	var trials := maxi(1, int(_extract_argument("--trials=", "5")))
	var sims_per_draft := maxi(1, int(_extract_argument("--sims-per-draft=", "10")))
	var base_seed := int(_extract_argument("--base-seed=", "90000"))

	_backend = NativeSimulationBackendScript.new()
	if not _backend.is_available():
		push_error("full_draft_ban_diagnostic: native backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not _backend.has_method("run_matches_stats"):
		push_error("full_draft_ban_diagnostic: backend missing run_matches_stats()")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("full_draft_ban_diagnostic: not enough champions")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	print("full_draft_ban_diagnostic: trials=%d sims/draft=%d" % [trials, sims_per_draft])

	# Build strategy instances
	var strategies: Dictionary = {}
	strategies["native_full"] = DraftStrategyNativePath.new(_stats_dir)
	strategies["native_picks_random_bans"] = DraftStrategyNativePicksRandomBansPath.new(_stats_dir)
	strategies["random_full"] = DraftStrategyRandomPath.new()

	# Required matchups
	var matchups = [
		["native_full", "random_full"],
		["native_full", "native_picks_random_bans"]
	]

	_diagnostic_lines.append("=== FULL DRAFT BAN DIAGNOSTIC RESULTS ===")
	_diagnostic_lines.append("Trials: %d" % trials)
	_diagnostic_lines.append("Simulations per draft: %d" % sims_per_draft)
	_diagnostic_lines.append("")

	for matchup_idx in range(matchups.size()):
		var strategy_a_name = matchups[matchup_idx][0]
		var strategy_b_name = matchups[matchup_idx][1]
		
		_diagnostic_lines.append("--- Matchup: %s (Blue) vs %s (Red) ---" % [strategy_a_name, strategy_b_name])
		_diagnostic_lines.append("")
		
		_run_matchup_with_diagnostics(
			strategies[strategy_a_name], 
			strategies[strategy_b_name], 
			trials, 
			sims_per_draft, 
			base_seed + matchup_idx * 10000,
			champion_ids,
			strategy_a_name,
			strategy_b_name
		)
		
		_diagnostic_lines.append("")

	_write_diagnostic_report()
	
	print("Full draft ban diagnostic complete. Report written to full_draft_ban_diagnostic_report.txt")
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)


func _run_matchup_with_diagnostics(strategy_a: RefCounted, strategy_b: RefCounted, trials: int, sims_per_draft: int, base_seed: int, champion_ids: Array[StringName], strategy_a_name: String, strategy_b_name: String) -> void:
	for trial in range(trials):
		var seed = base_seed + trial
		seed_rng(seed)
		
		print("Matchup %s vs %s: Trial %d/%d" % [strategy_a_name, strategy_b_name, trial, trials])
		
		var available = champion_ids.duplicate()
		available.shuffle()
		
		var blue_picks = []
		var red_picks = []
		var blue_bans = []
		var red_bans = []
		
		# Run full snake draft with diagnostics
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
			
			# Diagnose native bans
			if action == "BAN" and strategy.get_strategy_name().begins_with("native"):
				_diagnostic_lines.append("Trial %d, Step %d (%s %s):" % [trial, step_idx, side, action])
				_diagnose_native_ban(strategy, allies, enemies, available, draft_step, side)
			
			if action == "PICK":
				selected_champion = strategy.recommend_next_pick(allies, enemies, available, draft_step)
			else:
				selected_champion = strategy.recommend_next_ban(allies, enemies, available, draft_step, side)
			
			if selected_champion.is_empty() or not selected_champion in available:
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


func _diagnose_native_ban(strategy: RefCounted, allies: Array, enemies: Array, available: Array, draft_step: int, side: String) -> void:
	# Get ban recommendations with breakdowns from native backend
	if not _backend.has_method("get_draft_ai_ban_recommendations"):
		_diagnostic_lines.append("  ERROR: Backend missing get_draft_ai_ban_recommendations")
		return
	
	var allies_names = []
	for a in allies:
		allies_names.append(str(a))
	
	var enemies_names = []
	for e in enemies:
		enemies_names.append(str(e))
	
	var available_names = []
	for a in available:
		available_names.append(str(a))
	
	var recommendations_var: Variant = _backend.get_draft_ai_ban_recommendations(
		_stats_dir, available_names, allies_names, enemies_names, 10, draft_step, side
	)
	
	if typeof(recommendations_var) != TYPE_ARRAY:
		_diagnostic_lines.append("  ERROR: get_draft_ai_ban_recommendations did not return array")
		return
	
	var recommendations: Array = recommendations_var
	if recommendations.is_empty():
		_diagnostic_lines.append("  ERROR: No recommendations returned")
		return
	
	# Get top pick recommendations for comparison
	var our_pick_recs_var: Variant = _backend.get_draft_ai_pick_recommendations(
		_stats_dir, available_names, allies_names, enemies_names, 10, draft_step
	)
	
	var enemy_pick_recs_var: Variant = _backend.get_draft_ai_pick_recommendations(
		_stats_dir, available_names, enemies_names, allies_names, 10, draft_step
	)
	
	# Report top ban candidate
	var top_ban = recommendations[0]
	_diagnostic_lines.append("  Banned: %s" % str(top_ban.candidate))
	_diagnostic_lines.append("  Ban score: %.4f" % top_ban.total_score)
	
	_diagnostic_lines.append("  enemy_pick_value: %.4f" % top_ban.enemy_pick_value)
	_diagnostic_lines.append("  own_pick_value: %.4f" % top_ban.own_pick_value)
	_diagnostic_lines.append("  denial_value: %.4f" % top_ban.denial_value)
	_diagnostic_lines.append("  enemy_synergy: %.4f" % top_ban.enemy_synergy)
	_diagnostic_lines.append("  counters_my_team: %.4f" % top_ban.counters_my_team)
	_diagnostic_lines.append("  acting_side: %s" % str(top_ban.acting_side))
	_diagnostic_lines.append("  enemy_gets_first_pick_after_ban_phase: %s" % str(top_ban.enemy_gets_first_pick_after_ban_phase))
	_diagnostic_lines.append("  early_ban_fallback_component: %.4f" % top_ban.early_ban_fallback_component)
	
	# Find rank of banned champion in our pick list
	var our_pick_rank = -1
	if typeof(our_pick_recs_var) == TYPE_ARRAY:
		var our_pick_recs: Array = our_pick_recs_var
		for i in range(our_pick_recs.size()):
			if str(our_pick_recs[i].candidate) == str(top_ban.candidate):
				our_pick_rank = i
				break
	
	_diagnostic_lines.append("  Rank as our pick: %d" % our_pick_rank)
	
	# Find rank of banned champion in enemy pick list
	var enemy_pick_rank = -1
	if typeof(enemy_pick_recs_var) == TYPE_ARRAY:
		var enemy_pick_recs: Array = enemy_pick_recs_var
		for i in range(enemy_pick_recs.size()):
			if str(enemy_pick_recs[i].candidate) == str(top_ban.candidate):
				enemy_pick_rank = i
				break
	
	_diagnostic_lines.append("  Rank as enemy pick: %d" % enemy_pick_rank)
	
	# Report top 10 ban candidates with full breakdown
	_diagnostic_lines.append("  Top 10 ban candidates:")
	for i in range(min(10, recommendations.size())):
		var rec = recommendations[i]
		_diagnostic_lines.append("    %d. %s (total: %.4f, enemy_val: %.4f, own_val: %.4f, denial: %.4f, enemy_syn: %.4f, counters: %.4f)" % [i, str(rec.candidate), rec.total_score, rec.enemy_pick_value, rec.own_pick_value, rec.denial_value, rec.enemy_synergy, rec.counters_my_team])
		
		# Find rank as enemy pick
		var cand_enemy_pick_rank = -1
		if typeof(enemy_pick_recs_var) == TYPE_ARRAY:
			var enemy_pick_recs: Array = enemy_pick_recs_var
			for j in range(enemy_pick_recs.size()):
				if str(enemy_pick_recs[j].candidate) == str(rec.candidate):
					cand_enemy_pick_rank = j
					break
		
		# Find rank as our pick
		var cand_our_pick_rank = -1
		if typeof(our_pick_recs_var) == TYPE_ARRAY:
			var our_pick_recs: Array = our_pick_recs_var
			for j in range(our_pick_recs.size()):
				if str(our_pick_recs[j].candidate) == str(rec.candidate):
					cand_our_pick_rank = j
					break
		
		_diagnostic_lines.append("       enemy_pick_rank: %d, our_pick_rank: %d" % [cand_enemy_pick_rank, cand_our_pick_rank])
	
	# Report top 5 our pick candidates
	if typeof(our_pick_recs_var) == TYPE_ARRAY:
		var our_pick_recs: Array = our_pick_recs_var
		_diagnostic_lines.append("  Top 5 our pick candidates:")
		for i in range(min(5, our_pick_recs.size())):
			var rec = our_pick_recs[i]
			_diagnostic_lines.append("    %d. %s (%.4f)" % [i, str(rec.candidate), rec.total_score])
	
	# Report top 5 enemy pick candidates
	if typeof(enemy_pick_recs_var) == TYPE_ARRAY:
		var enemy_pick_recs: Array = enemy_pick_recs_var
		_diagnostic_lines.append("  Top 5 enemy pick candidates:")
		for i in range(min(5, enemy_pick_recs.size())):
			var rec = enemy_pick_recs[i]
			_diagnostic_lines.append("    %d. %s (%.4f)" % [i, str(rec.candidate), rec.total_score])
	
	_diagnostic_lines.append("")


func _write_diagnostic_report() -> void:
	var output_path = "res://full_draft_ban_diagnostic_report.txt"
	var f = FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f:
		f.store_string("\n".join(_diagnostic_lines))
		f.flush()
		f.close()


func seed_rng(seed: int) -> void:
	seed(seed)
