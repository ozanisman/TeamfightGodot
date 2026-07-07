extends SceneTree

## Policy-driven full drafts + match sims aggregated into production stats CSVs.
##
## Usage:
##   godot --headless --path . --script res://scripts/tools/native_draft_self_play_stats.gd \
##     -- --drafts=1000 --sims-per-draft=1 \
##        --blue-strategies=native_softmax --red-strategies=native_softmax \
##        --stats-dir=res://model_stats/stats_output_100k \
##        --output-dir=res://model_stats/stats_selfplay_native_softmax \
##        --decision-output=res://model_stats/draft_state_training_rows.csv

const DraftHarnessCoreScript := preload("res://scripts/tools/draft_harness_core.gd")
const DraftSelfPlayStatsRunnerScript := preload("res://scripts/tools/draft_self_play_stats_runner.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")


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

	var stats_dir: String = _extract_argument("--stats-dir=", "res://model_stats/stats_output_100k")
	var output_dir: String = _extract_argument("--output-dir=", _default_output_dir())
	var decision_output_path: String = _extract_argument("--decision-output=", "")
	var drafts: int = maxi(1, int(_extract_argument("--drafts=", "1000")))
	var sims_per_draft: int = maxi(1, int(_extract_argument("--sims-per-draft=", "1")))
	var base_seed: int = int(_extract_argument("--base-seed=", "500000"))
	var write_match_log: bool = _flag_enabled("--write-match-log")
	var blue_strategy_names: Array[String] = DraftHarnessCoreScript.dedupe_strategy_names(
		_parse_strategy_names(_extract_argument("--blue-strategies=", "native_softmax"))
	)
	var red_strategy_names: Array[String] = DraftHarnessCoreScript.dedupe_strategy_names(
		_parse_strategy_names(_extract_argument("--red-strategies=", "native_softmax"))
	)

	print("native_draft_self_play_stats: drafts=%d sims/draft=%d stats_dir=%s output_dir=%s" % [
		drafts, sims_per_draft, stats_dir, output_dir
	])

	var result: Dictionary = DraftSelfPlayStatsRunnerScript.run({
		"stats_dir": stats_dir,
		"output_dir": output_dir,
		"decision_output_path": decision_output_path,
		"drafts": drafts,
		"sims_per_draft": sims_per_draft,
		"base_seed": base_seed,
		"write_match_log": write_match_log,
		"blue_strategies": blue_strategy_names,
		"red_strategies": red_strategy_names,
		"generator_args": OS.get_cmdline_user_args(),
		"log_prefix": "native_draft_self_play_stats",
	})

	if not result.get("ok", false):
		push_error("native_draft_self_play_stats: %s" % result.get("error", "failed"))
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	print("native_draft_self_play_stats: wrote %s (%d drafts, %d matches)" % [
		result["output_dir"], result["completed_drafts"], result["consumed_matches"]
	])
	if not String(result.get("decision_output_path", "")).is_empty():
		print("native_draft_self_play_stats: wrote decision rows %s (%d rows)" % [
			result["decision_output_path"], result["decision_row_count"]
		])
	print("STATUS: OK")
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)
