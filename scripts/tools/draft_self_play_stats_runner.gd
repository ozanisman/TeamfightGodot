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


static func run(params: Dictionary) -> Dictionary:
	var stats_dir: String = String(params.get("stats_dir", "res://model_stats/stats_output_100k"))
	var output_dir: String = String(params.get("output_dir", ""))
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
				var pairing_seed: int = draft_seed + pairing_index * 1000
				seed(pairing_seed)
				pairing_index += 1

				var draft_result: Dictionary = DraftHarnessCoreScript.run_full_draft(
					pool.duplicate(), blue_strat, red_strat,
					backend, stats_dir, sims_per_draft, pairing_seed, false
				)
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
