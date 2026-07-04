extends SceneTree

## Policy-driven full drafts + match sims aggregated into production stats CSVs.
##
## Usage:
##   godot --headless --path . --script res://scripts/tools/native_draft_self_play_stats.gd \
##     -- --drafts=1000 --sims-per-draft=1 \
##        --blue-strategies=native_softmax --red-strategies=native_softmax \
##        --stats-dir=res://model_stats/stats_output_100k \
##        --output-dir=res://model_stats/stats_selfplay_native_softmax

const TEAM_SIZE: int = 5
const PROGRESS_INTERVAL: int = 100

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftHarnessCoreScript := preload("res://scripts/tools/draft_harness_core.gd")
const StatsCsvAggregatorScript := preload("res://scripts/tools/stats_csv_aggregator.gd")
const StatsManifestScript := preload("res://scripts/tools/stats_manifest.gd")
const MatchupTrackerScript := preload("res://scripts/simulation/matchup_tracker.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

var _backend: RefCounted = null
var _stats_dir: String = "res://model_stats/stats_output_100k"
var _output_dir: String = ""
var _drafts: int = 1000
var _sims_per_draft: int = 1
var _base_seed: int = 500000
var _write_match_log: bool = false
var _blue_strategy_names: Array[String] = ["native_softmax"]
var _red_strategy_names: Array[String] = ["native_softmax"]


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _flag_enabled(flag: String) -> bool:
	for a in OS.get_cmdline_user_args():
		if str(a) == flag:
			return true
	return false


func _parse_strategy_names(s: String) -> Array[String]:
	var out: Array[String] = []
	for part in s.split(","):
		var trimmed: String = part.strip_edges()
		if not trimmed.is_empty():
			out.append(trimmed)
	return out


func _default_output_dir() -> String:
	var stamp: String = Time.get_datetime_string_from_system().replace(":", "").replace("-", "").replace("T", "_")
	return "res://model_stats/stats_selfplay_%s" % stamp


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	await process_frame

	OS.set_environment("TEAMFIGHT_STATS_EXPORT_MINIMAL", "1")
	ChampionCatalogScript.build_catalog()
	ChampionCatalogScript.build_role_configs()
	ChampionCatalogScript.build_passive_registry()
	ChampionCatalogScript.build_minion_catalog()
	ChampionCatalogScript.freeze_built_specs_for_worker_reuse()

	_drafts = maxi(1, int(_extract_argument("--drafts=", "1000")))
	_sims_per_draft = maxi(1, int(_extract_argument("--sims-per-draft=", "1")))
	_base_seed = int(_extract_argument("--base-seed=", "500000"))
	_stats_dir = _extract_argument("--stats-dir=", "res://model_stats/stats_output_100k")
	_output_dir = _extract_argument("--output-dir=", _default_output_dir())
	_write_match_log = _flag_enabled("--write-match-log")
	_blue_strategy_names = DraftHarnessCoreScript.dedupe_strategy_names(
		_parse_strategy_names(_extract_argument("--blue-strategies=", "native_softmax"))
	)
	_red_strategy_names = DraftHarnessCoreScript.dedupe_strategy_names(
		_parse_strategy_names(_extract_argument("--red-strategies=", "native_softmax"))
	)

	if DraftHarnessCoreScript.normalize_dir_path(_output_dir) == DraftHarnessCoreScript.normalize_dir_path(_stats_dir):
		push_error("native_draft_self_play_stats: --output-dir must differ from --stats-dir")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	if not DraftHarnessCoreScript.validate_strategy_names_nonempty(
		_blue_strategy_names, "blue", "native_draft_self_play_stats"
	):
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not DraftHarnessCoreScript.validate_strategy_names_nonempty(
		_red_strategy_names, "red", "native_draft_self_play_stats"
	):
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	print("native_draft_self_play_stats: drafts=%d sims/draft=%d stats_dir=%s output_dir=%s" % [
		_drafts, _sims_per_draft, _stats_dir, _output_dir
	])

	_backend = NativeSimulationBackendScript.new()
	if not _backend.is_available():
		push_error("native_draft_self_play_stats: native backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not _backend.has_method("run_matches_stats"):
		push_error("native_draft_self_play_stats: backend missing run_matches_stats()")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var required_files: Array[String] = ["combat_stats.csv", "matchup_with.csv", "matchup_vs.csv"]
	for f in required_files:
		var path: String = _stats_dir.path_join(f)
		if not FileAccess.file_exists(path):
			push_error("native_draft_self_play_stats: missing required stats file %s" % path)
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("native_draft_self_play_stats: not enough champions")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var blue_strategies: Dictionary = DraftHarnessCoreScript.build_strategy_map(_blue_strategy_names, _stats_dir)
	var red_strategies: Dictionary = DraftHarnessCoreScript.build_strategy_map(_red_strategy_names, _stats_dir)
	if not DraftHarnessCoreScript.validate_strategy_map(blue_strategies, "blue", "native_draft_self_play_stats"):
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not DraftHarnessCoreScript.validate_strategy_map(red_strategies, "red", "native_draft_self_play_stats"):
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var aggregator := StatsCsvAggregatorScript.new()
	aggregator.set_write_match_log(_write_match_log)
	aggregator.preload_roles(ChampionCatalogScript.build_role_by_hero_map())
	var matchup_tracker := MatchupTrackerScript.new()

	var pool_rng := RandomNumberGenerator.new()
	var pairing_index: int = 0
	var completed_drafts: int = 0
	var consumed_matches: int = 0
	var start_msec: int = Time.get_ticks_msec()
	var pairings_per_trial: int = _blue_strategy_names.size() * _red_strategy_names.size()
	var total_drafts: int = _drafts * pairings_per_trial

	for trial in range(_drafts):
		var draft_seed: int = _base_seed + trial
		pool_rng.seed = draft_seed
		var pool: Array[StringName] = champion_ids.duplicate()
		DraftHarnessCoreScript.shuffle_pool(pool_rng, pool)

		for blue_name in _blue_strategy_names:
			var blue_strat = blue_strategies[blue_name]
			for red_name in _red_strategy_names:
				var red_strat = red_strategies[red_name]

				var pairing_seed: int = draft_seed + pairing_index * 1000
				seed(pairing_seed)
				pairing_index += 1

				var draft_result: Dictionary = DraftHarnessCoreScript.run_full_draft(
					pool.duplicate(), blue_strat, red_strat,
					_backend, _stats_dir, _sims_per_draft, pairing_seed, false
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
					print("native_draft_self_play_stats: %d/%d drafts (%d matches, %.1f drafts/s)" % [
						completed_drafts, total_drafts, consumed_matches, rate
					])

	var csv_err: Error = aggregator.write_to_dir(_output_dir)
	if csv_err != OK:
		push_error("native_draft_self_play_stats: write_to_dir failed: %s" % error_string(csv_err))
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	aggregator.ingest_matchup_tracker(matchup_tracker)

	if not aggregator.write_matchup_file(_output_dir):
		push_error("native_draft_self_play_stats: write_matchup_file failed")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var manifest: Dictionary = StatsManifestScript.build_manifest(
		_output_dir,
		"native_draft_self_play_stats",
		OS.get_cmdline_user_args()
	)
	if not StatsManifestScript.write_manifest(_output_dir, manifest):
		push_error("native_draft_self_play_stats: failed to write manifest")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	if _backend.has_method("clear"):
		_backend.call("clear")

	print("native_draft_self_play_stats: wrote %s (%d drafts, %d matches)" % [
		_output_dir, completed_drafts, consumed_matches
	])
	print("STATUS: OK")
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)
