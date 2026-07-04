extends SceneTree

## Deterministic full-draft validation harness.
##
## Usage:
##   godot --headless --script res://scripts/tools/native_draft_validation_harness.gd \
##     -- --trials=25 --sims-per-draft=25 \
##     --blue-strategies=native_full \
##     --red-strategies=random,base_power_only \
##     --output=res://model_stats/native_draft_validation.csv \
##     --draft-summary-output=res://model_stats/native_draft_validation_drafts.csv

const DraftHarnessCoreScript := preload("res://scripts/tools/draft_harness_core.gd")
const DraftValidationRunnerScript := preload("res://scripts/tools/draft_validation_runner.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _parse_strategy_names(s: String) -> Array[String]:
	var out: Array[String] = []
	for part in s.split(","):
		var trimmed: String = part.strip_edges()
		if not trimmed.is_empty():
			out.append(trimmed)
	return out


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
	var output_path: String = _extract_argument("--output=", "res://model_stats/native_draft_validation.csv")
	var draft_summary_output_path: String = _extract_argument("--draft-summary-output=", "")
	var trials: int = maxi(1, int(_extract_argument("--trials=", "50")))
	var sims_per_draft: int = maxi(1, int(_extract_argument("--sims-per-draft=", "25")))
	var base_seed: int = int(_extract_argument("--base-seed=", "100000"))
	var blue_strategy_names: Array[String] = DraftHarnessCoreScript.dedupe_strategy_names(
		_parse_strategy_names(_extract_argument("--blue-strategies=", "native_full"))
	)
	var red_strategy_names: Array[String] = DraftHarnessCoreScript.dedupe_strategy_names(
		_parse_strategy_names(_extract_argument("--red-strategies=", "random,base_power_only"))
	)

	print("native_draft_validation_harness: trials=%d sims/draft=%d stats_dir=%s" % [
		trials, sims_per_draft, stats_dir
	])

	var result: Dictionary = DraftValidationRunnerScript.run({
		"stats_dir": stats_dir,
		"output_path": output_path,
		"draft_summary_output_path": draft_summary_output_path,
		"trials": trials,
		"sims_per_draft": sims_per_draft,
		"base_seed": base_seed,
		"blue_strategies": blue_strategy_names,
		"red_strategies": red_strategy_names,
		"log_prefix": "native_draft_validation_harness",
	})

	if not result.get("ok", false):
		push_error("native_draft_validation_harness: %s" % result.get("error", "failed"))
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	print("native_draft_validation_harness: wrote %s (%d rows)" % [output_path, result["step_row_count"]])
	if not draft_summary_output_path.is_empty():
		print("native_draft_validation_harness: wrote %s (%d rows)" % [
			draft_summary_output_path, result["draft_row_count"]
		])

	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)
