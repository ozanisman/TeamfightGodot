extends SceneTree

## Deterministic full-draft validation harness.
## Runs full 5v5 snake drafts using NATIVE_FULL and baseline strategies,
## simulates the completed teams, and emits per-step CSV diagnostics.
##
## Usage:
##   godot --headless --script res://scripts/tools/native_draft_validation_harness.gd \
##     -- --trials=25 --sims-per-draft=25 \
##     --blue-strategies=native_full \
##     --red-strategies=random,base_power_only \
##     --output=res://model_stats/native_draft_validation.csv \
##     --draft-summary-output=res://model_stats/native_draft_validation_drafts.csv

const TEAM_SIZE: int = 5

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftHarnessCoreScript := preload("res://scripts/tools/draft_harness_core.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

var _backend: RefCounted = null
var _stats_dir: String = "res://model_stats/stats_output_100k"
var _output_path: String = "res://model_stats/native_draft_validation.csv"
var _draft_summary_output_path: String = ""
var _trials: int = 50
var _sims_per_draft: int = 25
var _base_seed: int = 100000
var _blue_strategy_names: Array[String] = ["native_full"]
var _red_strategy_names: Array[String] = ["random", "base_power_only"]


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

	_trials = maxi(1, int(_extract_argument("--trials=", "50")))
	_sims_per_draft = maxi(1, int(_extract_argument("--sims-per-draft=", "25")))
	_base_seed = int(_extract_argument("--base-seed=", "100000"))
	_stats_dir = _extract_argument("--stats-dir=", "res://model_stats/stats_output_100k")
	_output_path = _extract_argument("--output=", "res://model_stats/native_draft_validation.csv")
	_draft_summary_output_path = _extract_argument("--draft-summary-output=", "")
	_blue_strategy_names = DraftHarnessCoreScript.dedupe_strategy_names(
		_parse_strategy_names(_extract_argument("--blue-strategies=", "native_full"))
	)
	_red_strategy_names = DraftHarnessCoreScript.dedupe_strategy_names(
		_parse_strategy_names(_extract_argument("--red-strategies=", "random,base_power_only"))
	)

	if not DraftHarnessCoreScript.validate_strategy_names_nonempty(
		_blue_strategy_names, "blue", "native_draft_validation_harness"
	):
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not DraftHarnessCoreScript.validate_strategy_names_nonempty(
		_red_strategy_names, "red", "native_draft_validation_harness"
	):
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	print("native_draft_validation_harness: trials=%d sims/draft=%d stats_dir=%s" % [
		_trials, _sims_per_draft, _stats_dir
	])

	_backend = NativeSimulationBackendScript.new()
	if not _backend.is_available():
		push_error("native_draft_validation_harness: native backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not _backend.has_method("run_matches_stats"):
		push_error("native_draft_validation_harness: backend missing run_matches_stats()")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var required_files: Array[String] = ["combat_stats.csv", "matchup_with.csv", "matchup_vs.csv"]
	for f in required_files:
		var path: String = _stats_dir.path_join(f)
		if not FileAccess.file_exists(path):
			push_error("native_draft_validation_harness: missing required stats file %s" % path)
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("native_draft_validation_harness: not enough champions")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var blue_strategies: Dictionary = DraftHarnessCoreScript.build_strategy_map(_blue_strategy_names, _stats_dir)
	var red_strategies: Dictionary = DraftHarnessCoreScript.build_strategy_map(_red_strategy_names, _stats_dir)
	if not DraftHarnessCoreScript.validate_strategy_map(blue_strategies, "blue", "native_draft_validation_harness"):
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not DraftHarnessCoreScript.validate_strategy_map(red_strategies, "red", "native_draft_validation_harness"):
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var csv_rows: Array[String] = []
	csv_rows.append(
		"draft_seed,blue_strategy,red_strategy,step_index,step_side,step_action," +
		"chosen_candidate,blue_picks,red_picks,blue_bans,red_bans," +
		"blue_wins,red_wins,draws,blue_winrate,red_winrate,drawrate," +
		"base_power,ally_synergy,enemy_counter_value,counter_risk,role_fit,comp_fit," +
		"total_score,phase_label,candidate_role,comp_fingerprint"
	)

	var draft_summary_rows: Array[String] = []
	var emit_draft_summary: bool = not _draft_summary_output_path.is_empty()
	if emit_draft_summary:
		draft_summary_rows.append(
			"draft_seed,blue_strategy,red_strategy,blue_picks,red_picks," +
			"blue_bans,red_bans,blue_wins,red_wins,draws,blue_winrate,red_winrate,drawrate,total_matches"
		)

	var pool_rng := RandomNumberGenerator.new()
	var pairing_index: int = 0

	for trial in range(_trials):
		var draft_seed: int = _base_seed + trial
		pool_rng.seed = draft_seed
		var pool: Array[StringName] = champion_ids.duplicate()
		DraftHarnessCoreScript.shuffle_pool(pool_rng, pool)

		for blue_name in _blue_strategy_names:
			var blue_strat = blue_strategies[blue_name]
			for red_name in _red_strategy_names:
				var red_strat = red_strategies[red_name]

				seed(draft_seed + pairing_index * 1000)
				pairing_index += 1

				var draft_result: Dictionary = DraftHarnessCoreScript.run_full_draft(
					pool.duplicate(), blue_strat, red_strat,
					_backend, _stats_dir, _sims_per_draft, draft_seed, true
				)
				csv_rows.append_array(_format_draft_rows(draft_seed, blue_name, red_name, draft_result))
				if emit_draft_summary:
					draft_summary_rows.append(_format_draft_summary_row(draft_seed, blue_name, red_name, draft_result))

				if trial == 0 and blue_name == _blue_strategy_names[0] and red_name == _red_strategy_names[0]:
					print("sample draft: blue=%s red=%s" % [draft_result.blue_picks, draft_result.red_picks])

	var f := FileAccess.open(ProjectSettings.globalize_path(_output_path), FileAccess.WRITE)
	if f == null:
		push_error("native_draft_validation_harness: could not open output %s" % _output_path)
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	f.store_string("\n".join(csv_rows) + "\n")
	f.close()

	print("native_draft_validation_harness: wrote %s (%d rows)" % [_output_path, csv_rows.size() - 1])

	if emit_draft_summary:
		var df := FileAccess.open(ProjectSettings.globalize_path(_draft_summary_output_path), FileAccess.WRITE)
		if df == null:
			push_error("native_draft_validation_harness: could not open draft summary %s" % _draft_summary_output_path)
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return
		df.store_string("\n".join(draft_summary_rows) + "\n")
		df.close()
		print("native_draft_validation_harness: wrote %s (%d rows)" % [_draft_summary_output_path, draft_summary_rows.size() - 1])

	if _backend.has_method("clear"):
		_backend.call("clear")

	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)


func _format_draft_rows(
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


func _format_row(
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


func _format_draft_summary_row(
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
