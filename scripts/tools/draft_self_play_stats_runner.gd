class_name DraftSelfPlayStatsRunner
extends RefCounted

## In-process policy-driven stats generation (native_draft_self_play_stats.gd core).

const TEAM_SIZE: int = 5
const PROGRESS_INTERVAL: int = 100

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftHarnessCoreScript := preload("res://scripts/tools/draft_harness_core.gd")
const StatsCsvAggregatorScript := preload("res://scripts/tools/stats_csv_aggregator.gd")
const StatsManifestScript := preload("res://scripts/tools/stats_manifest.gd")
const MatchupTrackerScript := preload("res://scripts/simulation/matchup_tracker.gd")

const DECISION_ROW_COLUMNS: Array[String] = [
	"draft_seed",
	"pairing_index",
	"pairing_seed",
	"blue_strategy",
	"red_strategy",
	"step_index",
	"step_side",
	"step_action",
	"acting_side",
	"blue_picks_before",
	"red_picks_before",
	"blue_bans_before",
	"red_bans_before",
	"legal_pool",
	"selected",
	"blue_picks_final",
	"red_picks_final",
	"blue_bans_final",
	"red_bans_final",
	"blue_wins",
	"red_wins",
	"draws",
	"blue_winrate",
	"red_winrate",
	"drawrate",
	"total_matches",
	"base_power",
	"ally_synergy",
	"enemy_counter_value",
	"counter_risk",
	"role_fit",
	"comp_fit",
	"total_score",
	"phase_label",
	"candidate_role",
	"comp_fingerprint",
]


static func run(params: Dictionary) -> Dictionary:
	var stats_dir: String = String(params.get("stats_dir", "res://model_stats/stats_output_100k"))
	var output_dir: String = String(params.get("output_dir", ""))
	var decision_output_path: String = String(params.get("decision_output_path", ""))
	var drafts: int = maxi(1, int(params.get("drafts", 1000)))
	var sims_per_draft: int = maxi(1, int(params.get("sims_per_draft", 1)))
	var base_seed: int = int(params.get("base_seed", 500000))
	var write_match_log: bool = bool(params.get("write_match_log", false))
	var generator_args: Array = params.get("generator_args", [])
	var blue_strategy_names: Array[String] = _coerce_strategy_names(params.get("blue_strategies", ["native_softmax"]))
	var red_strategy_names: Array[String] = _coerce_strategy_names(params.get("red_strategies", ["native_softmax"]))

	if output_dir.is_empty():
		return {"ok": false, "error": "output_dir required"}

	if DraftHarnessCoreScript.normalize_dir_path(output_dir) == DraftHarnessCoreScript.normalize_dir_path(stats_dir):
		return {"ok": false, "error": "output_dir must differ from stats_dir"}

	if not DraftHarnessCoreScript.validate_strategy_names_nonempty(blue_strategy_names, "blue", "DraftSelfPlayStatsRunner"):
		return {"ok": false, "error": "invalid blue strategies"}
	if not DraftHarnessCoreScript.validate_strategy_names_nonempty(red_strategy_names, "red", "DraftSelfPlayStatsRunner"):
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
	if not DraftHarnessCoreScript.validate_strategy_map(blue_strategies, "blue", "DraftSelfPlayStatsRunner"):
		return {"ok": false, "error": "invalid blue strategy map"}
	if not DraftHarnessCoreScript.validate_strategy_map(red_strategies, "red", "DraftSelfPlayStatsRunner"):
		return {"ok": false, "error": "invalid red strategy map"}

	var aggregator := StatsCsvAggregatorScript.new()
	aggregator.set_write_match_log(write_match_log)
	aggregator.preload_roles(ChampionCatalogScript.build_role_by_hero_map())
	var matchup_tracker := MatchupTrackerScript.new()

	var pool_rng := RandomNumberGenerator.new()
	var pairing_index: int = 0
	var completed_drafts: int = 0
	var consumed_matches: int = 0
	var decision_rows: Array[String] = []
	var decision_output_enabled: bool = not decision_output_path.is_empty()
	if decision_output_enabled:
		decision_rows.append(",".join(DECISION_ROW_COLUMNS))
	var start_msec: int = Time.get_ticks_msec()
	var pairings_per_trial: int = blue_strategy_names.size() * red_strategy_names.size()
	var total_drafts: int = drafts * pairings_per_trial
	var log_prefix: String = String(params.get("log_prefix", "draft_self_play_stats_runner"))

	for trial in range(drafts):
		var draft_seed: int = base_seed + trial
		pool_rng.seed = draft_seed
		var pool: Array[StringName] = champion_ids.duplicate()
		DraftHarnessCoreScript.shuffle_pool(pool_rng, pool)

		for blue_name in blue_strategy_names:
			var blue_strat = blue_strategies[blue_name]
			for red_name in red_strategy_names:
				var red_strat = red_strategies[red_name]
				var current_pairing_index: int = pairing_index
				var pairing_seed: int = draft_seed + pairing_index * 1000
				seed(pairing_seed)
				pairing_index += 1

				var draft_result: Dictionary = DraftHarnessCoreScript.run_full_draft(
					pool.duplicate(), blue_strat, red_strat,
					backend, stats_dir, sims_per_draft, pairing_seed,
					decision_output_enabled, decision_output_enabled
				)
				if decision_output_enabled:
					decision_rows.append_array(_format_decision_rows(
						draft_seed, current_pairing_index, pairing_seed, blue_name, red_name, draft_result
					))
				for summary in draft_result["summaries"]:
					var enriched: Variant = DraftHarnessCoreScript.enrich_summary_teams(
						summary, draft_result["blue_picks"], draft_result["red_picks"]
					)
					aggregator.consume_summary(TEAM_SIZE, enriched)
					DraftHarnessCoreScript.record_matchup_from_summary(
						matchup_tracker, enriched,
						draft_result["blue_picks"], draft_result["red_picks"]
					)
					consumed_matches += 1

				completed_drafts += 1
				if completed_drafts % PROGRESS_INTERVAL == 0 or completed_drafts == total_drafts:
					var elapsed_s: float = float(Time.get_ticks_msec() - start_msec) / 1000.0
					var rate: float = float(completed_drafts) / maxf(elapsed_s, 0.001)
					print("%s: %d/%d drafts (%d matches, %.1f drafts/s)" % [
						log_prefix, completed_drafts, total_drafts, consumed_matches, rate
					])

	var csv_err: Error = aggregator.write_to_dir(output_dir)
	if csv_err != OK:
		if backend.has_method("clear"):
			backend.call("clear")
		return {"ok": false, "error": "write_to_dir failed: %s" % error_string(csv_err)}

	aggregator.ingest_matchup_tracker(matchup_tracker)
	if not aggregator.write_matchup_file(output_dir):
		if backend.has_method("clear"):
			backend.call("clear")
		return {"ok": false, "error": "write_matchup_file failed"}

	if decision_output_enabled and not _write_lines(decision_output_path, decision_rows):
		if backend.has_method("clear"):
			backend.call("clear")
		return {"ok": false, "error": "failed to write decision rows"}

	var manifest: Dictionary = StatsManifestScript.build_manifest(
		output_dir,
		"native_draft_self_play_stats",
		generator_args
	)
	if not StatsManifestScript.write_manifest(output_dir, manifest):
		if backend.has_method("clear"):
			backend.call("clear")
		return {"ok": false, "error": "failed to write manifest"}

	if backend.has_method("clear"):
		backend.call("clear")

	return {
		"ok": true,
		"output_dir": output_dir,
		"snapshot_id": String(manifest.get("snapshot_id", "")),
		"completed_drafts": completed_drafts,
		"consumed_matches": consumed_matches,
		"decision_output_path": decision_output_path,
		"decision_row_count": decision_rows.size() - 1 if decision_output_enabled else 0,
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


static func _format_decision_rows(
	draft_seed: int,
	pairing_index: int,
	pairing_seed: int,
	blue_name: String,
	red_name: String,
	draft_result: Dictionary
) -> Array[String]:
	var rows: Array[String] = []
	var sim: Dictionary = draft_result["sim"]
	var total_matches: int = int(sim["blue_wins"]) + int(sim["red_wins"]) + int(sim["draws"])
	for rec_var in Array(draft_result["step_records"]):
		var rec: Dictionary = rec_var
		var diag: Dictionary = rec.get("diagnostic", {})
		var fields: Array[String] = [
			str(draft_seed),
			str(pairing_index),
			str(pairing_seed),
			blue_name,
			red_name,
			str(int(rec.get("step_index", -1))),
			String(rec.get("side", "")),
			String(rec.get("action", "")),
			String(rec.get("acting_side", "")),
			_join_values(rec.get("blue_picks_before", [])),
			_join_values(rec.get("red_picks_before", [])),
			_join_values(rec.get("blue_bans_before", [])),
			_join_values(rec.get("red_bans_before", [])),
			_join_values(rec.get("legal_pool", [])),
			String(rec.get("chosen", "")),
			_join_values(draft_result.get("blue_picks", [])),
			_join_values(draft_result.get("red_picks", [])),
			_join_values(draft_result.get("blue_bans", [])),
			_join_values(draft_result.get("red_bans", [])),
			str(int(sim["blue_wins"])),
			str(int(sim["red_wins"])),
			str(int(sim["draws"])),
			"%.6f" % float(sim["blue_winrate"]),
			"%.6f" % float(sim["red_winrate"]),
			"%.6f" % float(sim["drawrate"]),
			str(total_matches),
			"%.6f" % float(diag.get("base_power", 0.0)),
			"%.6f" % float(diag.get("ally_synergy", 0.0)),
			"%.6f" % float(diag.get("enemy_counter_value", 0.0)),
			"%.6f" % float(diag.get("counter_risk", 0.0)),
			"%.6f" % float(diag.get("role_fit", 0.0)),
			"%.6f" % float(diag.get("comp_fit", 0.0)),
			"%.6f" % float(diag.get("total_score", 0.0)),
			String(diag.get("phase_label", "")),
			String(diag.get("candidate_role", "")),
			String(diag.get("comp_fingerprint", "")),
		]
		var escaped: Array[String] = []
		for field in fields:
			escaped.append(_csv_field(field))
		rows.append(",".join(escaped))
	return rows


static func _join_values(values: Variant) -> String:
	var parts: Array[String] = []
	if values is Array:
		for value in values:
			parts.append(String(value))
	return "|".join(parts)


static func _csv_field(value: String) -> String:
	if value.contains("\"") or value.contains(",") or value.contains("\n") or value.contains("\r"):
		return "\"" + value.replace("\"", "\"\"") + "\""
	return value


static func _write_lines(path: String, lines: Array[String]) -> bool:
	var global_path: String = ProjectSettings.globalize_path(path)
	var dir_path: String = global_path.get_base_dir()
	if not DirAccess.dir_exists_absolute(dir_path):
		var err: Error = DirAccess.make_dir_recursive_absolute(dir_path)
		if err != OK:
			return false
	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f == null:
		return false
	f.store_string("\n".join(lines) + "\n")
	f.close()
	return true
