class_name DraftValidationRunner
extends RefCounted

## In-process full-draft validation harness (native_draft_validation_harness.gd core).

const TEAM_SIZE: int = 5

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftHarnessCoreScript := preload("res://scripts/tools/draft_harness_core.gd")


static func run(params: Dictionary) -> Dictionary:
	var stats_dir: String = String(params.get("stats_dir", "res://model_stats/stats_output_100k"))
	var output_path: String = String(params.get("output_path", "res://model_stats/native_draft_validation.csv"))
	var draft_summary_output_path: String = String(params.get("draft_summary_output_path", ""))
	var trials: int = maxi(1, int(params.get("trials", 50)))
	var sims_per_draft: int = maxi(1, int(params.get("sims_per_draft", 25)))
	var base_seed: int = int(params.get("base_seed", 100000))
	var blue_strategy_names: Array[String] = _coerce_strategy_names(params.get("blue_strategies", ["native_full"]))
	var red_strategy_names: Array[String] = _coerce_strategy_names(params.get("red_strategies", ["random", "base_power_only"]))
	var log_prefix: String = String(params.get("log_prefix", "draft_validation_runner"))

	if not DraftHarnessCoreScript.validate_strategy_names_nonempty(blue_strategy_names, "blue", log_prefix):
		return {"ok": false, "error": "invalid blue strategies"}
	if not DraftHarnessCoreScript.validate_strategy_names_nonempty(red_strategy_names, "red", log_prefix):
		return {"ok": false, "error": "invalid red strategies"}

	var backend: RefCounted = NativeSimulationBackendScript.new()
	if not backend.is_available():
		return {"ok": false, "error": "native backend unavailable"}
	if not backend.has_method("run_matches_stats"):
		return {"ok": false, "error": "backend missing run_matches_stats()"}

	var required_files: Array[String] = ["combat_stats.csv", "matchup_with.csv", "matchup_vs.csv"]
	for file_name in required_files:
		var path: String = stats_dir.path_join(file_name)
		if not FileAccess.file_exists(path):
			return {"ok": false, "error": "missing required stats file %s" % path}

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		return {"ok": false, "error": "not enough champions"}

	var blue_strategies: Dictionary = DraftHarnessCoreScript.build_strategy_map(blue_strategy_names, stats_dir)
	var red_strategies: Dictionary = DraftHarnessCoreScript.build_strategy_map(red_strategy_names, stats_dir)
	if not DraftHarnessCoreScript.validate_strategy_map(blue_strategies, "blue", log_prefix):
		return {"ok": false, "error": "invalid blue strategy map"}
	if not DraftHarnessCoreScript.validate_strategy_map(red_strategies, "red", log_prefix):
		return {"ok": false, "error": "invalid red strategy map"}

	var csv_rows: Array[String] = []
	csv_rows.append(
		"draft_seed,blue_strategy,red_strategy,step_index,step_side,step_action," +
		"chosen_candidate,blue_picks,red_picks,blue_bans,red_bans," +
		"blue_wins,red_wins,draws,blue_winrate,red_winrate,drawrate," +
		"base_power,ally_synergy,enemy_counter_value,counter_risk,role_fit,comp_fit," +
		"total_score,phase_label,candidate_role,comp_fingerprint"
	)

	var draft_summary_rows: Array[String] = []
	var emit_draft_summary: bool = not draft_summary_output_path.is_empty()
	if emit_draft_summary:
		draft_summary_rows.append(
			"draft_seed,blue_strategy,red_strategy,blue_picks,red_picks," +
			"blue_bans,red_bans,blue_wins,red_wins,draws,blue_winrate,red_winrate,drawrate,total_matches"
		)

	var pool_rng := RandomNumberGenerator.new()
	var pairing_index: int = 0

	for trial in range(trials):
		var draft_seed: int = base_seed + trial
		pool_rng.seed = draft_seed
		var pool: Array[StringName] = champion_ids.duplicate()
		DraftHarnessCoreScript.shuffle_pool(pool_rng, pool)

		for blue_name in blue_strategy_names:
			var blue_strat = blue_strategies[blue_name]
			for red_name in red_strategy_names:
				var red_strat = red_strategies[red_name]
				seed(draft_seed + pairing_index * 1000)
				pairing_index += 1

				var draft_result: Dictionary = DraftHarnessCoreScript.run_full_draft(
					pool.duplicate(), blue_strat, red_strat,
					backend, stats_dir, sims_per_draft, draft_seed, true
				)
				csv_rows.append_array(_format_draft_rows(draft_seed, blue_name, red_name, draft_result))
				if emit_draft_summary:
					draft_summary_rows.append(_format_draft_summary_row(draft_seed, blue_name, red_name, draft_result))

	var f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f == null:
		if backend.has_method("clear"):
			backend.call("clear")
		return {"ok": false, "error": "could not open output %s" % output_path}
	f.store_string("\n".join(csv_rows) + "\n")
	f.close()

	if emit_draft_summary:
		var df := FileAccess.open(ProjectSettings.globalize_path(draft_summary_output_path), FileAccess.WRITE)
		if df == null:
			if backend.has_method("clear"):
				backend.call("clear")
			return {"ok": false, "error": "could not open draft summary %s" % draft_summary_output_path}
		df.store_string("\n".join(draft_summary_rows) + "\n")
		df.close()

	if backend.has_method("clear"):
		backend.call("clear")

	return {
		"ok": true,
		"output_path": output_path,
		"draft_summary_output_path": draft_summary_output_path,
		"step_row_count": csv_rows.size() - 1,
		"draft_row_count": draft_summary_rows.size() - 1 if emit_draft_summary else 0,
	}


static func _coerce_strategy_names(raw: Variant) -> Array[String]:
	var out: Array[String] = []
	if raw is Array:
		for item in raw:
			var trimmed: String = String(item).strip_edges()
			if not trimmed.is_empty():
				out.append(trimmed)
	elif raw is String:
		for part in String(raw).split(","):
			var trimmed: String = part.strip_edges()
			if not trimmed.is_empty():
				out.append(trimmed)
	return DraftHarnessCoreScript.dedupe_strategy_names(out)


static func _format_draft_rows(
	draft_seed: int,
	blue_name: String,
	red_name: String,
	draft_result: Dictionary
) -> Array[String]:
	var rows: Array[String] = []
	var sim: Dictionary = draft_result["sim"]
	for rec in draft_result["step_records"]:
		rows.append(_format_row(
			draft_seed, blue_name, red_name,
			rec["step_index"], rec["side"], rec["action"], rec["chosen"],
			draft_result["blue_picks"], draft_result["red_picks"],
			draft_result["blue_bans"], draft_result["red_bans"],
			sim["blue_wins"], sim["red_wins"], sim["draws"],
			sim["blue_winrate"], sim["red_winrate"], sim["drawrate"],
			rec["diagnostic"]
		))
	return rows


static func _format_row(
	draft_seed: int,
	blue_name: String,
	red_name: String,
	step_index: int,
	step_side: String,
	step_action: String,
	chosen: StringName,
	blue_picks: Array,
	red_picks: Array,
	blue_bans: Array,
	red_bans: Array,
	blue_wins: int,
	red_wins: int,
	draws: int,
	blue_winrate: float,
	red_winrate: float,
	drawrate: float,
	diag: Dictionary
) -> String:
	var parts: Array[String] = [
		str(draft_seed),
		blue_name,
		red_name,
		str(step_index),
		step_side,
		step_action,
		String(chosen),
		"|".join(blue_picks),
		"|".join(red_picks),
		"|".join(blue_bans),
		"|".join(red_bans),
		str(blue_wins),
		str(red_wins),
		str(draws),
		"%.6f" % blue_winrate,
		"%.6f" % red_winrate,
		"%.6f" % drawrate,
		"%.6f" % float(diag.get("base_power", 0.0)),
		"%.6f" % float(diag.get("ally_synergy", 0.0)),
		"%.6f" % float(diag.get("enemy_counter_value", 0.0)),
		"%.6f" % float(diag.get("counter_risk", 0.0)),
		"%.6f" % float(diag.get("role_fit", 0.0)),
		"%.6f" % float(diag.get("comp_fit", 0.0)),
		"%.6f" % float(diag.get("total_score", 0.0)),
		String(diag.get("phase_label", "")),
		String(diag.get("candidate_role", "")),
		String(diag.get("comp_fingerprint", ""))
	]
	return ",".join(parts)


static func _format_draft_summary_row(
	draft_seed: int,
	blue_name: String,
	red_name: String,
	result: Dictionary
) -> String:
	var sim: Dictionary = result["sim"]
	var total_matches: int = int(sim["blue_wins"]) + int(sim["red_wins"]) + int(sim["draws"])
	var parts: Array[String] = [
		str(draft_seed),
		blue_name,
		red_name,
		"|".join(result["blue_picks"]),
		"|".join(result["red_picks"]),
		"|".join(result["blue_bans"]),
		"|".join(result["red_bans"]),
		str(sim["blue_wins"]),
		str(sim["red_wins"]),
		str(sim["draws"]),
		"%.6f" % float(sim["blue_winrate"]),
		"%.6f" % float(sim["red_winrate"]),
		"%.6f" % float(sim["drawrate"]),
		str(total_matches)
	]
	return ",".join(parts)
