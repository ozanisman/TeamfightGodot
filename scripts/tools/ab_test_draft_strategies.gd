extends SceneTree

## A/B test runner for draft recommendation strategies.
## Compares strategies by running actual combat simulations.
##
## Usage:
##   godot --headless --script res://scripts/tools/ab_test_draft_strategies.gd \
##     -- --trials=500 --sims-per-trial=50 --depths=1,2,3 \
##     --strategies=hybrid,logit,random --output=res://stats_output_partial/draft_ab_test.csv

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SpawnSpecScript := preload("res://scripts/simulation/spawn_spec.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const DraftStrategyRandomPath := "res://scripts/tools/draft_strategy_random.gd"
const DraftStrategyLogitPath := "res://scripts/tools/draft_strategy_logit.gd"
const DraftStrategyHybridPath := "res://scripts/tools/draft_strategy_hybrid.gd"
const DraftStrategyCertifiedPath := "res://scripts/tools/draft_strategy_certified.gd"

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
	OS.set_environment("TEAMFIGHT_STATS_EXPORT_MINIMAL", "1")
	ChampionCatalogScript.build_catalog()
	ChampionCatalogScript.build_role_configs()
	ChampionCatalogScript.build_passive_registry()
	ChampionCatalogScript.build_minion_catalog()
	ChampionCatalogScript.freeze_built_specs_for_worker_reuse()

	var trials := maxi(1, int(_extract_argument("--trials=", "500")))
	var sims_per_trial := maxi(1, int(_extract_argument("--sims-per-trial=", "50")))
	var depths_str := _extract_argument("--depths=", "1,2,3")
	var strategies_str := _extract_argument("--strategies=", "hybrid,logit,random")
	var stats_dir := _extract_argument("--stats-dir=", "res://stats_output")
	var output_path := _extract_argument("--output=", "res://stats_output_partial/draft_ab_test.csv")
	var base_seed := int(_extract_argument("--base-seed=", "90000"))

	var depths: Array[int] = []
	for s in depths_str.split(","):
		var d := int(s.strip_edges())
		if d >= 1 and d <= TEAM_SIZE - 1:
			depths.append(d)
	if depths.is_empty():
		depths = [1, 2, 3]

	var strategy_names: Array[String] = []
	for s in strategies_str.split(","):
		strategy_names.append(s.strip_edges())

	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("ab_test_draft_strategies: native backend unavailable")
		quit(1)
		return
	if not backend.has_method("run_matches_stats"):
		push_error("ab_test_draft_strategies: backend missing run_matches_stats()")
		quit(1)
		return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("ab_test_draft_strategies: not enough champions")
		quit(1)
		return

	print("ab_test_draft_strategies: trials=%d sims/trial=%d depths=%s strategies=%s" % [
		trials, sims_per_trial, depths, ",".join(strategy_names)
	])

	# Build strategy instances
	var strategies: Dictionary = {}
	for name in strategy_names:
		match name:
			"random":
				strategies[name] = load(DraftStrategyRandomPath).new()
			"logit":
				strategies[name] = load(DraftStrategyLogitPath).new(stats_dir)
			"hybrid":
				strategies[name] = load(DraftStrategyHybridPath).new(stats_dir)
			"certified":
				strategies[name] = load(DraftStrategyCertifiedPath).new(stats_dir)
			_:
				push_error("ab_test_draft_strategies: unknown strategy '%s'" % name)
				quit(1)
				return

	# Pairwise comparisons: all ordered pairs (A, B) where A != B
	var pairs: Array[Array] = []
	for i in range(strategy_names.size()):
		for j in range(strategy_names.size()):
			if i != j:
				pairs.append([strategy_names[i], strategy_names[j]])

	var csv_lines: Array[String] = [
		"trial_id,depth,draft_position,allies,enemies,banned,available_n,strategy_a,pick_a,winrate_a,strategy_b,pick_b,winrate_b,delta"
	]

	var rng := RandomNumberGenerator.new()
	rng.seed = base_seed

	var total_trials := trials * depths.size() * pairs.size()
	var completed := 0

	for depth in depths:
		for trial in range(trials):
			var state := _sample_state(champion_ids, depth, base_seed + trial, rng)
			var allies: Array[StringName] = state["allies"]
			var enemies: Array[StringName] = state["enemies"]
			var available: Array[StringName] = state["available"]
			var banned: Array[StringName] = state["banned"]
			var draft_position: int = state["draft_position"]

			for pair in pairs:
				var name_a: String = pair[0]
				var name_b: String = pair[1]
				var strat_a = strategies[name_a]
				var strat_b = strategies[name_b]

				var pick_a: StringName
				var pick_b: StringName
				var winrate_a := 0.5
				var winrate_b := 0.5

				if strat_a == null:
					push_error("strat_a is null for %s" % name_a)
					continue
				if strat_b == null:
					push_error("strat_b is null for %s" % name_b)
					continue

				pick_a = strat_a.recommend_next_pick(allies, enemies, available)
				pick_b = strat_b.recommend_next_pick(allies, enemies, available)

				if pick_a.is_empty():
					push_error("pick_a is empty for %s" % name_a)
					continue
				if pick_b.is_empty():
					push_error("pick_b is empty for %s" % name_b)
					continue

				winrate_a = _evaluate_pick(
					backend, allies, enemies, available, pick_a, sims_per_trial,
					base_seed + trial * 10000 + 0
				)
				winrate_b = _evaluate_pick(
					backend, allies, enemies, available, pick_b, sims_per_trial,
					base_seed + trial * 10000 + 1
				)

				var delta := winrate_b - winrate_a
				csv_lines.append("%d,%d,%d,%s,%s,%s,%d,%s,%s,%.6f,%s,%s,%.6f,%.6f" % [
					trial, depth, draft_position,
					"|".join(allies), "|".join(enemies), "|".join(banned), available.size(),
					name_a, pick_a, winrate_a,
					name_b, pick_b, winrate_b,
					delta
				])

				completed += 1
				if completed % 10 == 0:
					print("  ... %d/%d trials done (depth=%d trial=%d %s vs %s)" % [completed, total_trials, depth, trial, name_a, name_b])

	# Write CSV
	var f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f == null:
		push_error("ab_test_draft_strategies: could not open %s" % output_path)
		quit(1)
		return
	f.store_string("\n".join(csv_lines) + "\n")
	f.close()

	# Print summary
	_print_summary(csv_lines, depths, strategy_names)

	if backend.has_method("clear"):
		backend.call("clear")

	print("")
	print("================ A/B TEST COMPLETE ================")
	print("CSV: %s" % output_path)
	print("total comparisons: %d" % (csv_lines.size() - 1))
	print("====================================================")
	quit(0)


func _sample_state(champion_ids: Array[StringName], draft_depth: int, seed: int, rng: RandomNumberGenerator) -> Dictionary:
	var pool := champion_ids.duplicate()
	_shuffle(pool, rng)
	var player: Array[StringName] = []
	var enemy: Array[StringName] = []
	var banned: Array[StringName] = []
	
	# Simulate draft sequence up to draft_depth steps
	var steps_to_simulate := mini(draft_depth, SimConstantsScript.DRAFT_SEQUENCE.size())
	for i in range(steps_to_simulate):
		var turn := SimConstantsScript.DRAFT_SEQUENCE[i]
		if pool.is_empty():
			break
		
		if turn.ends_with("_BAN"):
			var ban_champion := pool.pop_back()
			banned.append(ban_champion)
		elif turn.ends_with("_PICK"):
			if turn.begins_with("B_"):
				if player.size() < TEAM_SIZE:
					player.append(pool.pop_back())
			elif turn.begins_with("R_"):
				if enemy.size() < TEAM_SIZE:
					enemy.append(pool.pop_back())
	
	# Calculate global draft position from the next step index (next pick to recommend)
	var next_step_index := steps_to_simulate
	var draft_position := SimConstantsScript.get_global_pick_position(next_step_index)
	
	var picking_player := (seed % 2) == 0
	return {
		"allies": player if picking_player else enemy,
		"enemies": enemy if picking_player else player,
		"available": pool,
		"banned": banned,
		"picking_team": "player" if picking_player else "enemy",
		"draft_position": draft_position,
	}


func _evaluate_pick(
	backend: RefCounted,
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	pick: StringName,
	sims_per_trial: int,
	seed_offset: int
) -> float:
	# Complete the draft: add pick to allies, then snake-fill rest
	var completed_allies := allies.duplicate()
	var completed_enemies := enemies.duplicate()
	completed_allies.append(pick)

	var pool: Array[StringName] = []
	for id in available:
		if id != pick:
			pool.append(id)

	var rng := RandomNumberGenerator.new()
	rng.seed = seed_offset
	_shuffle(pool, rng)
	
	# Simulate snake draft order for remaining picks
	# Full pick sequence: B_PICK, R_PICK, R_PICK, B_PICK, B_PICK, R_PICK, R_PICK, B_PICK, B_PICK, R_PICK
	var pick_sequence := ["B_PICK", "R_PICK", "R_PICK", "B_PICK", "B_PICK", "R_PICK", "R_PICK", "B_PICK", "B_PICK", "R_PICK"]
	
	# Calculate starting position in sequence based on current team sizes
	var total_picks := completed_allies.size() + completed_enemies.size()
	var sequence_index := total_picks
	
	while completed_allies.size() < TEAM_SIZE or completed_enemies.size() < TEAM_SIZE:
		if sequence_index >= pick_sequence.size():
			break
		
		var turn: String = pick_sequence[sequence_index]
		if turn == "B_PICK" and completed_allies.size() < TEAM_SIZE:
			if not pool.is_empty():
				completed_allies.append(pool.pop_back())
		elif turn == "R_PICK" and completed_enemies.size() < TEAM_SIZE:
			if not pool.is_empty():
				completed_enemies.append(pool.pop_back())
		sequence_index += 1

	# Build units and run simulations
	var player_units := _build_units(completed_allies, &"player")
	var enemy_units := _build_units(completed_enemies, &"enemy")

	var inputs: Array = []
	inputs.resize(sims_per_trial)
	for s in range(sims_per_trial):
		inputs[s] = MatchReplayInputScript.new(
			seed_offset + s, SimConstantsScript.DEFAULT_TICK_RATE,
			player_units, enemy_units
		)

	var summaries_var: Variant = backend.run_matches_stats(inputs)
	if typeof(summaries_var) != TYPE_ARRAY:
		push_error("ab_test_draft_strategies: run_matches_stats did not return array")
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


func _print_summary(csv_lines: Array[String], depths: Array[int], strategy_names: Array[String]) -> void:
	# Skip header
	if csv_lines.size() <= 1:
		return

	# Parse results
	var results_by_depth_strategy: Dictionary = {}
	for d in depths:
		for name in strategy_names:
			results_by_depth_strategy["%d_%s" % [d, name]] = []

	for i in range(1, csv_lines.size()):
		var parts := csv_lines[i].split(",")
		if parts.size() < 13:
			continue
		var depth := int(parts[1])
		var name_a := parts[6]
		var winrate_a := float(parts[8])
		var name_b := parts[9]
		var winrate_b := float(parts[11])

		var key_a := "%d_%s" % [depth, name_a]
		var key_b := "%d_%s" % [depth, name_b]
		if key_a in results_by_depth_strategy:
			results_by_depth_strategy[key_a].append(winrate_a)
		if key_b in results_by_depth_strategy:
			results_by_depth_strategy[key_b].append(winrate_b)

	print("")
	print("================ SUMMARY ================")
	for depth in depths:
		print("Depth %d:" % depth)
		for name in strategy_names:
			var key := "%d_%s" % [depth, name]
			var vals: Array = results_by_depth_strategy.get(key, [])
			if vals.is_empty():
				print("  %s: no data" % name)
				continue
			var mean := _mean(vals)
			var std := _std(vals, mean)
			print("  %s: mean=%.1f%% std=%.1f%% n=%d" % [name, mean * 100.0, std * 100.0, vals.size()])

		# Pairwise deltas for this depth
		for i in range(strategy_names.size()):
			for j in range(strategy_names.size()):
				if i == j:
					continue
				var deltas: Array[float] = []
				for k in range(1, csv_lines.size()):
					var p := csv_lines[k].split(",")
					if p.size() < 12:
						continue
					if int(p[1]) != depth:
						continue
					if p[5] == strategy_names[i] and p[8] == strategy_names[j]:
						deltas.append(float(p[11]))
				if not deltas.is_empty():
					var d_mean := _mean(deltas)
					var d_std := _std(deltas, d_mean)
					var se := d_std / sqrt(float(deltas.size()))
					print("  %s -> %s: delta=%+.2f pp (n=%d, se=%.2f pp)" % [
						strategy_names[i], strategy_names[j],
						d_mean * 100.0, deltas.size(), se * 100.0
					])
	print("===========================================")


func _mean(values: Array) -> float:
	if values.is_empty():
		return 0.0
	var sum := 0.0
	for v in values:
		sum += float(v)
	return sum / float(values.size())


func _std(values: Array, mean: float) -> float:
	if values.size() <= 1:
		return 0.0
	var sum_sq := 0.0
	for v in values:
		var diff := float(v) - mean
		sum_sq += diff * diff
	return sqrt(sum_sq / float(values.size()))


func _shuffle(values: Array, rng: RandomNumberGenerator) -> void:
	for i in range(values.size() - 1, 0, -1):
		var j := rng.randi_range(0, i)
		var tmp = values[i]
		values[i] = values[j]
		values[j] = tmp
