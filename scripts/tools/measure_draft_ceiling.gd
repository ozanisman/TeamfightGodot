extends SceneTree

## Theoretical-ceiling measurement for the single-match draft-prediction metric.
##
## The accuracy metric in stats_simulation_csv_generator.gd::_score_prediction_config predicts the
## winner of ONE match (one comp + one seed) — a single Bernoulli draw of that comp's true
## win-probability p over seeds. The best possible predictor knows p exactly and always picks the
## favored side, but the realized outcome is still random. So the Bayes ceiling for this metric is
## E[max(p, 1-p)] over the comp distribution. This tool estimates that ceiling so we can decide
## whether the ~63.5% accuracy wall is a fundamental limit or leftover signal.
##
## CRITICAL: simulation_batch_worker.gd::_build_batch_input_for_seed couples the comp AND the sim
## RNG to one seed. This tool DECOUPLES them: it samples a comp using the SAME uniform-random
## distinct-5v5 shuffle distribution, then holds that comp fixed while varying only the sim seed.
##
## Run via run_godot.ps1 --measure-draft-ceiling (forwards args after --).
##   --num-comps=500 --seeds-per-comp=500 --comp-base-seed=1 --sim-base-seed=1000000
##   --team-size=5 --ceiling-output=res://stats_output/draft_ceiling.csv

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SpawnSpecScript := preload("res://scripts/simulation/spawn_spec.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

## 63.5% baseline accuracy the recommender plateaued at (see wiki/notes/draft_prediction_context.md).
const ACCURACY_BASELINE: float = 0.635


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _flag_enabled(prefix: String) -> bool:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg == prefix:
			return true
		if arg.begins_with(prefix + "="):
			var tail: String = arg.substr(prefix.length() + 1)
			return tail != "0" and tail.to_lower() != "false"
	return false


func _init() -> void:
	call_deferred("_run")


## Builds a SpawnSpec unit array for champion ids placed at the given team's formation positions.
func _build_units(ids: Array[StringName], team: StringName) -> Array:
	var units: Array = []
	for index in range(ids.size()):
		var pos := SimConstantsScript.spawn_position(index, team)
		units.append(SpawnSpecScript.new(ids[index], team, pos.x, pos.y))
	return units


## Runs seeds_per_comp matches for a fixed orientation (player_units vs enemy_units), reusing the
## unit arrays across seeds. Returns the summaries Array, or null on error.
func _run_seed_batch(
	backend: RefCounted, player_units: Array, enemy_units: Array,
	comp_index: int, seeds_per_comp: int, sim_base_seed: int
) -> Variant:
	var inputs: Array = []
	inputs.resize(seeds_per_comp)
	for s in range(seeds_per_comp):
		var sim_seed: int = sim_base_seed + comp_index * seeds_per_comp + s
		inputs[s] = MatchReplayInputScript.new(
			sim_seed, SimConstantsScript.DEFAULT_TICK_RATE, player_units, enemy_units
		)
	var summaries_var: Variant = backend.run_matches_stats(inputs)
	if backend.has_method("clear"):
		backend.call("clear")
	if typeof(summaries_var) != TYPE_ARRAY:
		push_error("measure_draft_ceiling: run_matches_stats did not return an array (comp %d)" % comp_index)
		return null
	var summaries: Array = summaries_var
	if summaries.size() != seeds_per_comp:
		push_error("measure_draft_ceiling: expected %d summaries, got %d (comp %d)" % [seeds_per_comp, summaries.size(), comp_index])
		return null
	return summaries


## Replicates the comp distribution of simulation_batch_worker.gd::_build_batch_input_for_seed for a
## single comp seed, returning [players, enemies] as Array[StringName] of length team_size each.
func _sample_comp(comp_seed: int, team_size: int, archetypes: Array[StringName]) -> Array:
	var rng := RandomNumberGenerator.new()
	rng.seed = comp_seed
	var players: Array[StringName] = []
	var enemies: Array[StringName] = []
	if archetypes.size() < team_size * 2:
		for _i in range(team_size):
			players.append(archetypes[rng.randi_range(0, archetypes.size() - 1)])
		for _i in range(team_size):
			enemies.append(archetypes[rng.randi_range(0, archetypes.size() - 1)])
	else:
		var indices: Array[int] = []
		for i in range(archetypes.size()):
			indices.append(i)
		for i in range(indices.size() - 1, 0, -1):
			var j := rng.randi_range(0, i)
			var tmp := indices[i]
			indices[i] = indices[j]
			indices[j] = tmp
		for i in range(team_size):
			players.append(archetypes[indices[i]])
		for i in range(team_size, team_size * 2):
			enemies.append(archetypes[indices[i]])
	return [players, enemies]


func _run() -> void:
	OS.set_environment("TEAMFIGHT_STATS_EXPORT_MINIMAL", "1")
	ChampionCatalogScript.build_catalog()
	ChampionCatalogScript.build_role_configs()
	ChampionCatalogScript.build_passive_registry()
	ChampionCatalogScript.build_minion_catalog()
	ChampionCatalogScript.freeze_built_specs_for_worker_reuse()

	var num_comps := maxi(1, int(_extract_argument("--num-comps=", "500")))
	var seeds_per_comp := maxi(2, int(_extract_argument("--seeds-per-comp=", "500")))
	var comp_base_seed := int(_extract_argument("--comp-base-seed=", "1"))
	var sim_base_seed := int(_extract_argument("--sim-base-seed=", "1000000"))
	var team_size := maxi(1, int(_extract_argument("--team-size=", "5")))
	var ceiling_output := _extract_argument("--ceiling-output=", "res://model_stats/certified_pairwise_training_250k/draft_ceiling.csv")
	# Mirror mode: run each comp+seed twice with sides swapped. Averaging a comp's winrate across both
	# orientations cancels the engine's player-side bias, so the ceiling reflects comp-signal ONLY.
	var mirror := _flag_enabled("--mirror")

	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("measure_draft_ceiling: native simulation backend unavailable")
		quit(1)
		return
	if not backend.has_method("run_matches_stats"):
		push_error("measure_draft_ceiling: backend missing run_matches_stats()")
		quit(1)
		return

	var archetypes: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	var half: int = seeds_per_comp / 2

	# Aggregates.
	var raw_max_sum: float = 0.0          # sum of max(p, 1-p) over comps with decisive games
	var raw_max_comps: int = 0
	var sh_correct: int = 0               # split-half: correct predictions on the held-out half
	var sh_total: int = 0
	var total_player_wins: int = 0        # side-bias check
	var total_decisive: int = 0
	# Imbalance histogram on |p - 0.5|.
	var imbalance_edges: Array[float] = [0.02, 0.05, 0.10, 0.20, 0.51]
	var imbalance_bins: Array[int] = [0, 0, 0, 0, 0]
	var near_half_comps: int = 0          # |p - 0.5| <= 0.05

	var csv_lines: Array[String] = ["comp_index,team_a,team_b,decisive_n,p_team_a_win,abs_imbalance"]

	print("measure_draft_ceiling: %d comps x %d seeds (team_size=%d, mirror=%s) ..." % [
		num_comps, seeds_per_comp, team_size, str(mirror)
	])

	for comp_index in range(num_comps):
		var comp: Array = _sample_comp(comp_base_seed + comp_index, team_size, archetypes)
		var team_a: Array[StringName] = comp[0]   # tracked team (the "players" comp)
		var team_b: Array[StringName] = comp[1]

		# Orientation A (normal): team_a on player slot, team_b on enemy slot.
		# Unit arrays built once per orientation and reused across seeds.
		var sums_a: Variant = _run_seed_batch(
			backend, _build_units(team_a, &"player"), _build_units(team_b, &"enemy"),
			comp_index, seeds_per_comp, sim_base_seed
		)
		if sums_a == null:
			quit(1)
			return
		# Orientation B (mirror): swap sides — team_b on player slot, team_a on enemy slot, SAME seeds.
		var sums_b: Variant = null
		if mirror:
			sums_b = _run_seed_batch(
				backend, _build_units(team_b, &"player"), _build_units(team_a, &"enemy"),
				comp_index, seeds_per_comp, sim_base_seed
			)
			if sums_b == null:
				quit(1)
				return

		# Tally team_a wins across orientation(s). In orientation A, team_a wins when winner=="player";
		# in orientation B (mirror), team_a wins when winner=="enemy". player_slot_wins tracks the raw
		# engine side-bias (always winner=="player", regardless of which comp is there).
		var ta_h1: int = 0
		var dec_h1: int = 0
		var ta_h2: int = 0
		var dec_h2: int = 0
		var ta_all: int = 0
		var dec_all: int = 0
		var player_slot_wins: int = 0
		var orientations: Array = [[sums_a, true]]
		if mirror:
			orientations.append([sums_b, false])
		for ori in orientations:
			var sums: Array = ori[0]
			var ta_is_player: bool = ori[1]
			for i in range(seeds_per_comp):
				var winner: String = String(Dictionary(sums[i]).get("winner_team", ""))
				var is_player: bool = winner == "player"
				var is_enemy: bool = winner == "enemy"
				if not (is_player or is_enemy):
					continue
				if is_player:
					player_slot_wins += 1
				var ta_won: bool = is_player if ta_is_player else is_enemy
				dec_all += 1
				ta_all += 1 if ta_won else 0
				if i < half:
					dec_h1 += 1
					ta_h1 += 1 if ta_won else 0
				else:
					dec_h2 += 1
					ta_h2 += 1 if ta_won else 0

		total_player_wins += player_slot_wins
		total_decisive += dec_all

		# Raw-max ceiling contribution (uses all decisive seeds). With mirror on, p_team_a is averaged
		# across both orientations, so the engine side-bias cancels and this is comp-signal only.
		var p_team_a: float = 0.5
		if dec_all > 0:
			p_team_a = float(ta_all) / float(dec_all)
			raw_max_sum += maxf(p_team_a, 1.0 - p_team_a)
			raw_max_comps += 1
		var abs_imbalance: float = absf(p_team_a - 0.5)

		# Imbalance histogram + near-half count.
		if dec_all > 0:
			for b in range(imbalance_edges.size()):
				if abs_imbalance < imbalance_edges[b]:
					imbalance_bins[b] += 1
					break
			if abs_imbalance <= 0.05:
				near_half_comps += 1

		# Split-half: choose favored team from first half, score on second half.
		if dec_h1 > 0 and dec_h2 > 0:
			var predict_team_a: bool = float(ta_h1) >= float(dec_h1) * 0.5
			sh_correct += ta_h2 if predict_team_a else (dec_h2 - ta_h2)
			sh_total += dec_h2

		csv_lines.append("%d,%s,%s,%d,%.4f,%.4f" % [
			comp_index,
			"|".join(team_a),
			"|".join(team_b),
			dec_all,
			p_team_a,
			abs_imbalance,
		])

		if (comp_index + 1) % 50 == 0:
			print("  ... %d/%d comps done" % [comp_index + 1, num_comps])

	# Write CSV.
	var f := FileAccess.open(ceiling_output, FileAccess.WRITE)
	if f == null:
		push_error("measure_draft_ceiling: could not open %s for writing" % ceiling_output)
	else:
		f.store_string("\n".join(csv_lines) + "\n")
		f.close()

	# Compute headline numbers.
	var raw_max_ceiling: float = (raw_max_sum / float(raw_max_comps)) if raw_max_comps > 0 else 0.0
	var split_half_ceiling: float = (float(sh_correct) / float(sh_total)) if sh_total > 0 else 0.0
	var sh_se: float = 0.0
	if sh_total > 0:
		sh_se = sqrt(split_half_ceiling * (1.0 - split_half_ceiling) / float(sh_total))
	var sh_ci: float = 1.96 * sh_se
	var side_bias: float = (float(total_player_wins) / float(total_decisive)) if total_decisive > 0 else 0.0
	var near_half_frac: float = (float(near_half_comps) / float(raw_max_comps)) if raw_max_comps > 0 else 0.0

	# Report.
	var lines: Array[String] = []
	lines.append("")
	lines.append("================ DRAFT PREDICTION CEILING ================")
	lines.append("comps=%d  seeds/comp=%d  team_size=%d  decisive_matches=%d  mirror=%s" % [
		num_comps, seeds_per_comp, team_size, total_decisive, str(mirror)
	])
	if mirror:
		lines.append("MIRROR MODE: each comp run on both sides; ceiling is SIDE-DEBIASED (comp-signal only).")
	lines.append("")
	lines.append("Split-half ceiling (HEADLINE, unbiased): %.1f%%  (95%% CI +/- %.1f%%, n=%d)" % [
		split_half_ceiling * 100.0, sh_ci * 100.0, sh_total
	])
	lines.append("Raw-max ceiling   (cross-check, optimistic): %.1f%%  (n=%d comps)" % [
		raw_max_ceiling * 100.0, raw_max_comps
	])
	lines.append("Noise-bias gap (raw-max minus split-half): %.1f pp" % [
		(raw_max_ceiling - split_half_ceiling) * 100.0
	])
	lines.append("")
	lines.append("vs 63.5% baseline:")
	lines.append("  split-half ceiling - baseline = %+.1f pp" % [(split_half_ceiling - ACCURACY_BASELINE) * 100.0])
	lines.append("")
	lines.append("Comp imbalance distribution (|p - 0.5|):")
	var labels: Array[String] = ["[0.00,0.02)", "[0.02,0.05)", "[0.05,0.10)", "[0.10,0.20)", "[0.20,0.50]"]
	for b in range(labels.size()):
		var frac: float = (float(imbalance_bins[b]) / float(raw_max_comps)) if raw_max_comps > 0 else 0.0
		lines.append("  %-12s %6d comps (%.1f%%)" % [labels[b], imbalance_bins[b], frac * 100.0])
	lines.append("  comps within +/-0.05 of 50/50: %.1f%%" % [near_half_frac * 100.0])
	lines.append("")
	lines.append("Side bias (player-side win rate across all matches): %.1f%%" % [side_bias * 100.0])
	lines.append("  (far from 50% => first-side/positioning advantage confounds predictability)")
	lines.append("")
	lines.append("Per-comp CSV written to: %s" % ceiling_output)
	lines.append("")
	lines.append("Interpretation:")
	lines.append("  split-half ~ 63-65% => 63.5% is at/near the ceiling; accept it, reframe metric (regret/top-3/calibration).")
	lines.append("  split-half >> 65%   => real signal left on the table; pursue counter-design then strategic-draft training.")
	lines.append("=========================================================")
	var report: String = "\n".join(lines)
	print(report)

	quit(0)
