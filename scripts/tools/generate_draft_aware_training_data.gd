extends SceneTree

## Training data generator for draft-aware prediction model.
##
## Samples draft states at each depth, completes them with rollouts using snake draft order,
## and labels them with expected win probability from the certified model.

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

const TEAM_SIZE: int = 5
const DEFAULT_OUTPUT := "res://model_stats/draft_aware_training_250k/draft_aware_training.csv"
const ROLES: Array[String] = ["tank", "fighter", "mage", "assassin", "marksman", "support"]


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	print("INIT: generate_draft_aware_training_data")
	call_deferred("_run")


func _run() -> void:
	print("generate_draft_aware_training_data: starting")
	ChampionCatalogScript.build_catalog()
	ChampionCatalogScript.build_role_configs()
	ChampionCatalogScript.build_passive_registry()
	ChampionCatalogScript.build_minion_catalog()
	ChampionCatalogScript.freeze_built_specs_for_worker_reuse()

	var states_per_depth := maxi(1, int(_extract_argument("--states-per-depth=", "25")))
	var rollouts_per_state := maxi(1, int(_extract_argument("--rollouts-per-state=", "20")))
	var depths_str := _extract_argument("--depths=", "1,2,3,4,5")
	var base_seed := int(_extract_argument("--base-seed=", "80000"))
	var stats_dir := _extract_argument("--stats-dir=", "res://model_stats/draft_aware_testing_250k")
	var output_path := _extract_argument("--output=", DEFAULT_OUTPUT)
	
	print("Parameters: states_per_depth=%d rollouts_per_state=%d depths=%s" % [states_per_depth, rollouts_per_state, depths_str])

	var depths: Array[int] = []
	for s in depths_str.split(","):
		var d := int(s.strip_edges())
		if d >= 1 and d <= TEAM_SIZE:
			depths.append(d)
	if depths.is_empty():
		depths = [1, 2, 3, 4, 5]

	print("Depths to process: %s" % str(depths))

	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("generate_draft_aware_training_data: native simulation backend unavailable")
		quit(1)
		return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	print("Champion count: %d" % champion_ids.size())
	
	var role_by_hero: Dictionary = ChampionCatalogScript.build_role_by_hero_map()
	print("Role-by-hero map loaded: %d champions" % role_by_hero.size())
	
	# Load synergy and counter stats
	var synergy_stats_path := ProjectSettings.globalize_path(stats_dir + "/synergy_stats.csv")
	var counter_stats_path := ProjectSettings.globalize_path(stats_dir + "/counter_stats.csv")
	var synergy_stats := _load_stats(synergy_stats_path)
	var counter_stats := _load_stats(counter_stats_path)
	print("Loaded %d synergy stats, %d counter stats" % [synergy_stats.size(), counter_stats.size()])
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("generate_draft_aware_training_data: not enough champions for distinct 5v5 drafts")
		quit(1)
		return

	print("generate_draft_aware_training_data: states/depth=%d rollouts/state=%d depths=%s stats_dir=%s" % [
		states_per_depth, rollouts_per_state, ",".join(depths), stats_dir
	])

	var abs_path := ProjectSettings.globalize_path(output_path)
	print("Output path: %s" % abs_path)
	
	var mk := _ensure_parent_dir(abs_path)
	if mk != OK:
		push_error("generate_draft_aware_training_data: could not create output directory for %s: %s" % [abs_path, error_string(mk)])
		quit(1)
		return
	
	# Open file for writing immediately and write incrementally
	print("Opening file for incremental writing...")
	var f := FileAccess.open(abs_path, FileAccess.WRITE)
	if f == null:
		push_error("generate_draft_aware_training_data: could not open %s" % abs_path)
		print("FileAccess error code: %d" % FileAccess.get_open_error())
		quit(1)
		return
	
	# Write header with pick-focused features (no ban features)
	var header_parts := ["depth", "draft_position", "allies", "enemies", "banned_allies", "banned_enemies", "available_n"]
	# Add ally role counts
	for role in ROLES:
		header_parts.append("ally_%s" % role)
	# Add enemy role counts
	for role in ROLES:
		header_parts.append("enemy_%s" % role)
	# Add synergy/counter features
	header_parts.append_array(["avg_synergy", "avg_counter", "synergy_variance", "counter_variance", "net_matchup"])
	# Add composition balance features
	header_parts.append_array(["ally_role_diversity", "enemy_role_diversity", "ally_tank_ratio", "enemy_tank_ratio", "ally_support_ratio", "enemy_support_ratio", "ally_damage_ratio", "enemy_damage_ratio"])
	header_parts.append_array(["expected_win", "rollout_count"])
	f.store_string(",".join(header_parts) + "\n")
	f.flush()
	
	var total_states := 0

	for depth in depths:
		print("Processing ban phase index %d..." % depth)
		for state_index in range(states_per_depth):
			var state := _sample_state(champion_ids, depth, base_seed + total_states)
			var allies: Array[StringName] = state["allies"]
			var enemies: Array[StringName] = state["enemies"]
			var banned_allies: Array[StringName] = state["banned_allies"]
			var banned_enemies: Array[StringName] = state["banned_enemies"]
			var available: Array[StringName] = state["available"]
			var draft_position: int = state["draft_position"]

			var expected_win := _rollout_label_state(
				backend, allies, enemies, available, rollouts_per_state, base_seed, total_states, stats_dir
			)

			# Extract role counts
			var ally_roles := _extract_role_counts(allies, role_by_hero)
			var enemy_roles := _extract_role_counts(enemies, role_by_hero)
			
			# Compute synergy/counter strength
			var syn_cnt := _compute_synergy_counter_strength(allies, enemies, synergy_stats, counter_stats)
			
			# Compute composition balance
			var comp_balance := _compute_composition_balance(ally_roles, enemy_roles)

			# Build row with all features
			var row_parts := [
				str(depth),
				str(draft_position),
				"|".join(allies),
				"|".join(enemies),
				"|".join(banned_allies),
				"|".join(banned_enemies),
				str(available.size())
			]
			# Add ally role counts
			for role in ROLES:
				row_parts.append(str(ally_roles.get(role, 0)))
			# Add enemy role counts
			for role in ROLES:
				row_parts.append(str(enemy_roles.get(role, 0)))
			# Add synergy/counter features
			row_parts.append_array([str(syn_cnt["avg_synergy"]), str(syn_cnt["avg_counter"]), str(syn_cnt["synergy_variance"]), str(syn_cnt["counter_variance"]), str(syn_cnt["net_matchup"])])
			# Add composition balance features
			row_parts.append_array([str(comp_balance["ally_role_diversity"]), str(comp_balance["enemy_role_diversity"]), str(comp_balance["ally_tank_ratio"]), str(comp_balance["enemy_tank_ratio"]), str(comp_balance["ally_support_ratio"]), str(comp_balance["enemy_support_ratio"]), str(comp_balance["ally_damage_ratio"]), str(comp_balance["enemy_damage_ratio"])])
			row_parts.append_array([str(expected_win), str(rollouts_per_state)])

			# Write row immediately
			f.store_string(",".join(row_parts) + "\n")
			
			# Flush periodically
			if (state_index + 1) % 10 == 0:
				f.flush()
				print("  State %d/%d at depth %d, expected_win=%.4f" % [state_index + 1, states_per_depth, depth, expected_win])

			total_states += 1

	# Final flush and close
	print("Final flush and close...")
	f.flush()
	f.close()
	print("File closed successfully")
	print("File written to: %s" % abs_path)
	print("Total states written: %d" % total_states)
	
	if backend.has_method("clear"):
		backend.call("clear")

	print("")
	print("================ DRAFT-AWARE TRAINING DATA GENERATED ================")
	print("total_states=%d (ban phases only: depths 0,3)" % total_states)
	print("states_per_depth=%d rollouts_per_state=%d" % [states_per_depth, rollouts_per_state])
	print("CSV written to: %s" % abs_path)
	print("CSV lines: %d" % (total_states + 1))
	print("======================================================================")
	quit(0)


func _load_stats(path: String) -> Dictionary:
	var f := FileAccess.open(path, FileAccess.READ)
	if f == null:
		push_warning("generate_draft_aware_training_data: could not open %s, using default values" % path)
		return {}
	var lines := f.get_as_text().split("\n")
	f.close()

	var result := {}
	for i in range(1, lines.size()):
		var line := lines[i].strip_edges()
		if line.is_empty():
			continue
		var parts := line.split(",")
		if parts.size() >= 2:
			result[parts[0]] = float(parts[1])
	return result


func _extract_role_counts(champions: Array[StringName], role_by_hero: Dictionary) -> Dictionary:
	var counts: Dictionary = {}
	for role in ROLES:
		counts[role] = 0
	for champ in champions:
		var role_str: String = role_by_hero.get(String(champ), "")
		if role_str in counts:
			counts[role_str] += 1
	return counts


func _compute_synergy_counter_strength(allies: Array[StringName], enemies: Array[StringName], synergy_stats: Dictionary, counter_stats: Dictionary) -> Dictionary:
	var result := {"avg_synergy": 0.5, "avg_counter": 0.5, "synergy_variance": 0.0, "counter_variance": 0.0, "net_matchup": 0.5}
	
	if allies.is_empty() or enemies.is_empty():
		return result
	
	var synergy_sum := 0.0
	var counter_sum := 0.0
	var synergy_values: Array[float] = []
	var counter_values: Array[float] = []
	var n_pairs := 0
	
	for ally in allies:
		for other_ally in allies:
			if ally != other_ally:
				var key := String(ally) + "|" + String(other_ally)
				var syn: float = synergy_stats.get(key, 0.5)
				synergy_sum += syn
				synergy_values.append(syn)
		for enemy in enemies:
			var key := String(ally) + "|" + String(enemy)
			var cnt: float = counter_stats.get(key, 0.5)
			counter_sum += cnt
			counter_values.append(cnt)
			n_pairs += 1
	
	var n_allies := float(allies.size())
	var n_enemies := float(enemies.size())
	
	if n_allies > 1:
		result["avg_synergy"] = synergy_sum / (n_allies * (n_allies - 1))
	if n_pairs > 0:
		result["avg_counter"] = counter_sum / n_pairs
	
	# Compute variance
	if not synergy_values.is_empty():
		var syn_mean := synergy_sum / float(synergy_values.size())
		var syn_var := 0.0
		for v in synergy_values:
			syn_var += (v - syn_mean) * (v - syn_mean)
		result["synergy_variance"] = syn_var / float(synergy_values.size())
	
	if not counter_values.is_empty():
		var cnt_mean := counter_sum / float(counter_values.size())
		var cnt_var := 0.0
		for v in counter_values:
			cnt_var += (v - cnt_mean) * (v - cnt_mean)
		result["counter_variance"] = cnt_var / float(counter_values.size())
	
	# Compute net_matchup (same as avg_counter for this context)
	if n_pairs > 0:
		result["net_matchup"] = counter_sum / n_pairs
	
	return result


func _compute_composition_balance(ally_roles: Dictionary, enemy_roles: Dictionary) -> Dictionary:
	var result := {
		"ally_role_diversity": 0.0,
		"enemy_role_diversity": 0.0,
		"ally_tank_ratio": 0.0,
		"enemy_tank_ratio": 0.0,
		"ally_support_ratio": 0.0,
		"enemy_support_ratio": 0.0,
		"ally_damage_ratio": 0.0,
		"enemy_damage_ratio": 0.0
	}
	
	# Count unique roles
	var ally_unique := 0
	var enemy_unique := 0
	var ally_total := 0.0
	var enemy_total := 0.0
	
	for role in ROLES:
		var a_count: int = ally_roles.get(role, 0)
		var e_count: int = enemy_roles.get(role, 0)
		if a_count > 0:
			ally_unique += 1
		if e_count > 0:
			enemy_unique += 1
		ally_total += a_count
		enemy_total += e_count
	
	if ally_total > 0:
		result["ally_role_diversity"] = float(ally_unique) / ally_total
		result["ally_tank_ratio"] = ally_roles.get("tank", 0) / ally_total
		result["ally_support_ratio"] = ally_roles.get("support", 0) / ally_total
		# Damage roles: fighter, mage, assassin, marksman
		var ally_damage: int = ally_roles.get("fighter", 0) + ally_roles.get("mage", 0) + ally_roles.get("assassin", 0) + ally_roles.get("marksman", 0)
		result["ally_damage_ratio"] = ally_damage / ally_total
	
	if enemy_total > 0:
		result["enemy_role_diversity"] = float(enemy_unique) / enemy_total
		result["enemy_tank_ratio"] = enemy_roles.get("tank", 0) / enemy_total
		result["enemy_support_ratio"] = enemy_roles.get("support", 0) / enemy_total
		var enemy_damage: int = enemy_roles.get("fighter", 0) + enemy_roles.get("mage", 0) + enemy_roles.get("assassin", 0) + enemy_roles.get("marksman", 0)
		result["enemy_damage_ratio"] = enemy_damage / enemy_total
	
	return result


func _ensure_parent_dir(path: String) -> Error:
	var slash := path.rfind("/")
	var backslash := path.rfind("\\")
	var sep := maxi(slash, backslash)
	if sep < 0:
		return OK
	var dir_path := path.substr(0, sep)
	if dir_path.is_empty() or DirAccess.dir_exists_absolute(dir_path):
		return OK
	return DirAccess.make_dir_recursive_absolute(dir_path)


func _sample_state(champion_ids: Array[StringName], draft_depth: int, seed: int) -> Dictionary:
	var pool := champion_ids.duplicate()
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	_shuffle(pool, rng)
	var player: Array[StringName] = []
	var enemy: Array[StringName] = []
	var banned_allies: Array[StringName] = []
	var banned_enemies: Array[StringName] = []
	
	# Ban phases occur at specific sequence indices:
	# Phase 1: indices 0-5 (6 bans before any picks)
	# Phase 2: indices 12-15 (4 bans after 3 picks each)
	var ban_phase_indices: Array[int] = [0, 1, 2, 3, 4, 5, 12, 13, 14, 15]
	# Use seed to randomly select a ban phase index for variety
	var target_sequence_index: int = ban_phase_indices[seed % ban_phase_indices.size()]
	
	# Simulate draft sequence up to the target ban phase
	for i in range(target_sequence_index):
		if i >= SimConstantsScript.DRAFT_SEQUENCE.size():
			break
		var turn := SimConstantsScript.DRAFT_SEQUENCE[i]
		if pool.is_empty():
			break
		
		if turn.ends_with("_BAN"):
			var ban_champion: StringName = pool.pop_back()
			if turn.begins_with("B_"):
				banned_allies.append(ban_champion)
			elif turn.begins_with("R_"):
				banned_enemies.append(ban_champion)
		elif turn.ends_with("_PICK"):
			if turn.begins_with("B_"):
				if player.size() < TEAM_SIZE:
					player.append(pool.pop_back())
			elif turn.begins_with("R_"):
				if enemy.size() < TEAM_SIZE:
					enemy.append(pool.pop_back())
	
	# Calculate global draft position from the next step index (next pick to recommend)
	var next_step_index := target_sequence_index
	var draft_position := SimConstantsScript.get_global_pick_position(next_step_index)
	
	var picking_player := (seed % 2) == 0
	return {
		"allies": player if picking_player else enemy,
		"enemies": enemy if picking_player else player,
		"available": pool,
		"banned_allies": banned_allies if picking_player else banned_enemies,
		"banned_enemies": banned_enemies if picking_player else banned_allies,
		"picking_team": "player" if picking_player else "enemy",
		"draft_position": draft_position,
	}


func _rollout_label_state(backend: RefCounted, allies: Array[StringName], enemies: Array[StringName], available: Array[StringName], rollouts_per_state: int, base_seed: int, state_index: int, stats_dir: String) -> float:
	var sum: float = 0.0
	var valid_rollouts: int = 0
	var predictions: Array[float] = []
	for rollout_index in range(rollouts_per_state):
		var completion := _complete_snake_rollout(
			allies, enemies, available, base_seed + state_index * 10000 + rollout_index
		)
		if completion["allies"].size() != TEAM_SIZE or completion["enemies"].size() != TEAM_SIZE:
			continue
		var prediction: Dictionary = backend.predict_draft_winner(
			completion["allies"], completion["enemies"], stats_dir
		)
		if prediction.is_empty():
			print("  WARNING: empty prediction for rollout %d" % rollout_index)
			continue
		var prob := float(prediction.get("team1_prob", 0.5))
		predictions.append(prob)
		sum += prob
		valid_rollouts += 1
	
	# Debug: check prediction variance
	if state_index == 0 and valid_rollouts > 0:
		var pred_avg := sum / float(valid_rollouts)
		var pred_min: float = predictions.min()
		var pred_max: float = predictions.max()
		print("  State 0 predictions: avg=%.4f min=%.4f max=%.4f variance=%.4f valid=%d" % [pred_avg, pred_min, pred_max, pred_max - pred_min, valid_rollouts])
	
	if valid_rollouts == 0:
		print("  WARNING: no valid rollouts for state %d" % state_index)
		return 0.5
	return sum / float(valid_rollouts)


func _complete_snake_rollout(allies: Array[StringName], enemies: Array[StringName], available: Array[StringName], seed: int) -> Dictionary:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var completed_allies := allies.duplicate()
	var completed_enemies := enemies.duplicate()
	var pool: Array[StringName] = available.duplicate()
	_shuffle(pool, rng)
	
	# Multi-objective heuristic completion: winrate + synergy + counter-pick
	var combat_stats := _load_combat_stats_simple()
	var synergy_stats := _load_synergy_stats()
	var counter_stats := _load_counter_stats()
	
	while completed_allies.size() < TEAM_SIZE and not pool.is_empty():
		var best_champ := _select_best_champion_multi_objective(pool, completed_allies, completed_enemies, combat_stats, synergy_stats, counter_stats, rng)
		if best_champ:
			completed_allies.append(best_champ)
		else:
			completed_allies.append(pool.pop_back())
	
	while completed_enemies.size() < TEAM_SIZE and not pool.is_empty():
		var best_champ := _select_best_champion_multi_objective(pool, completed_enemies, completed_allies, combat_stats, synergy_stats, counter_stats, rng)
		if best_champ:
			completed_enemies.append(best_champ)
		else:
			completed_enemies.append(pool.pop_back())
	
	return {"allies": completed_allies, "enemies": completed_enemies}


func _load_combat_stats_simple() -> Dictionary:
	var result := {}
	var f := FileAccess.open(ProjectSettings.globalize_path("res://model_stats/draft_aware_testing_250k/combat_stats.csv"), FileAccess.READ)
	if f == null:
		return result
	var lines := f.get_as_text().split("\n")
	f.close()
	for i in range(1, lines.size()):
		var line := lines[i].strip_edges()
		if line.is_empty():
			continue
		var parts := line.split(",")
		if parts.size() >= 2:
			result[parts[0]] = float(parts[1])
	return result


func _load_synergy_stats() -> Dictionary:
	var result := {}
	var f := FileAccess.open(ProjectSettings.globalize_path("res://model_stats/draft_aware_testing_250k/matchup_with.csv"), FileAccess.READ)
	if f == null:
		return result
	var lines := f.get_as_text().split("\n")
	f.close()
	for i in range(1, lines.size()):
		var line := lines[i].strip_edges()
		if line.is_empty():
			continue
		var parts := line.split(",")
		if parts.size() >= 5:
			var champion := parts[0]
			var ally := parts[1]
			var winrate = float(parts[4])
			if not result.has(champion):
				result[champion] = {}
			result[champion][ally] = winrate
	return result


func _load_counter_stats() -> Dictionary:
	var result := {}
	var f := FileAccess.open(ProjectSettings.globalize_path("res://model_stats/draft_aware_testing_250k/matchup_vs.csv"), FileAccess.READ)
	if f == null:
		return result
	var lines := f.get_as_text().split("\n")
	f.close()
	for i in range(1, lines.size()):
		var line := lines[i].strip_edges()
		if line.is_empty():
			continue
		var parts := line.split(",")
		if parts.size() >= 5:
			var champion := parts[0]
			var opponent := parts[1]
			var winrate = float(parts[4])
			if not result.has(champion):
				result[champion] = {}
			result[champion][opponent] = winrate
	return result


func _select_best_champion_multi_objective(pool: Array[StringName], my_team: Array[StringName], enemy_team: Array[StringName], combat_stats: Dictionary, synergy_stats: Dictionary, counter_stats: Dictionary, rng: RandomNumberGenerator) -> StringName:
	if pool.is_empty():
		return ""
	
	var best_champ: StringName = ""
	var best_score: float = -1.0
	
	# Sample top 5 to add some randomness
	var sample_size: int = mini(5, pool.size())
	for i in range(sample_size):
		var idx: int = rng.randi_range(0, pool.size() - 1)
		var champ: StringName = pool[idx]
		
		# Winrate score (40%)
		var winrate = combat_stats.get(String(champ), 0.5)
		
		# Synergy score (30%) - average winrate with current team
		var synergy_score = 0.5
		if my_team.size() > 0 and synergy_stats.has(String(champ)):
			var total_synergy = 0.0
			var count = 0
			for ally in my_team:
				if synergy_stats[String(champ)].has(String(ally)):
					total_synergy += synergy_stats[String(champ)][String(ally)]
					count += 1
			if count > 0:
				synergy_score = total_synergy / float(count)
		
		# Counter score (30%) - average winrate vs enemy team
		var counter_score = 0.5
		if enemy_team.size() > 0 and counter_stats.has(String(champ)):
			var total_counter = 0.0
			var count = 0
			for opponent in enemy_team:
				if counter_stats[String(champ)].has(String(opponent)):
					total_counter += counter_stats[String(champ)][String(opponent)]
					count += 1
			if count > 0:
				counter_score = total_counter / float(count)
		
		# Combined score: 0.4 * winrate + 0.3 * synergy + 0.3 * counter
		var combined_score = 0.4 * winrate + 0.3 * synergy_score + 0.3 * counter_score
		
		if combined_score > best_score:
			best_score = combined_score
			best_champ = champ
	
	if best_champ != "":
		pool.erase(best_champ)
	return best_champ


func _shuffle(values: Array, rng: RandomNumberGenerator) -> void:
	for i in range(values.size() - 1, 0, -1):
		var j := rng.randi_range(0, i)
		var tmp = values[i]
		values[i] = values[j]
		values[j] = tmp
