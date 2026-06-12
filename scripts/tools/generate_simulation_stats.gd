extends SceneTree

## Headless CSV export for stats dashboard. Run via run_godot.ps1 --generate-stats (forwards args after --).
## Throughput: [SimulationBatchWorker] batches each worker chunk via native `run_matches_stats` when available,
## avoiding per-match GDExtension round-trips. Raise `--export-workers=` (or leave 0 for CPU auto-cap) for parallelism across chunks.
##
## --evaluate-draft-predictions: after the batch completes, runs predict_draft_winner on each
## completed-draft match's final comp and prints one accuracy report comparing predicted vs actual
## winner. Requires --prediction-stats-dir= to point at a stats snapshot OTHER than --out-dir
## (predicting against stats derived from the very matches being predicted is circular/in-sample).
## --prediction-team-sizes= filters which matches count as a "completed draft" (default: 5).
##
## Prediction tuning overrides (optional; each defaults to predict_draft_winner's own default —
## i.e. omitting all of them reproduces today's exact behavior). Recognized override keys:
## logistic_k, score_sharpness, interaction_weight, synergy_amplification,
## matchup_amplification, scoring_mode.
##   --prediction-logistic-k=, --prediction-score-sharpness=,
##   --prediction-interaction-weight=, --prediction-synergy-amplification=,
##   --prediction-matchup-amplification=, --prediction-scoring-mode=
## scoring_mode is the ScoringMode enum ordinal: ADDITIVE=0, MULTIPLICATIVE=1,
## CERTIFIED_PAIRWISE_PROBABILITY=3, DRAFT_AWARE_PAIRWISE_PROBABILITY=4 (default).
##
## Sweep mode (A/B compare a single override key across several values in ONE report instead of
## re-running the batch per value): --prediction-sweep-param=<override key>
## --prediction-sweep-values=<comma-separated floats>, e.g.
##   --prediction-sweep-param=logistic_k --prediction-sweep-values=10,12,15,20,25
##
## --analyze-signal-variance: after the batch completes, analyzes signal variance across champions
## from the generated stats and prints a comprehensive report. Requires --signal-variance-output=
## to specify CSV output path (e.g. res://stats_output/signal_variance.csv). Uses the same
## --team-sizes= and --matches-per-size= parameters as the main batch.

const StatsSimulationCsvGeneratorScript := preload("res://scripts/tools/stats_simulation_csv_generator.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")


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


func _run() -> void:
	OS.set_environment("TEAMFIGHT_STATS_EXPORT_MINIMAL", "1")
	# Pre-initialize champion catalog static cache to avoid threading issues
	ChampionCatalogScript.build_catalog()
	ChampionCatalogScript.build_role_configs()
	ChampionCatalogScript.build_passive_registry()
	ChampionCatalogScript.build_minion_catalog()
	ChampionCatalogScript.freeze_built_specs_for_worker_reuse()
	
	var out_dir := _extract_argument("--out-dir=", "res://stats_output")
	var sizes_raw := _extract_argument("--team-sizes=", "1,2,3,4,5")
	var matches := int(_extract_argument("--matches-per-size=", "100"))
	var seed := int(_extract_argument("--base-seed=", "0"))
	var export_workers := int(_extract_argument("--export-workers=", "0"))
	var profile_enabled := _flag_enabled("--profile-stats")
	var write_match_log := _flag_enabled("--write-match-log")
	var aggregate_stats_in_worker := not _flag_enabled("--no-worker-aggregate")
	var use_native_generated_stats := not _flag_enabled("--no-native-generated-stats")
	var evaluate_draft_predictions := _flag_enabled("--evaluate-draft-predictions")
	var prediction_stats_dir := _extract_argument("--prediction-stats-dir=", "res://stats_output")
	var prediction_sizes_raw := _extract_argument("--prediction-team-sizes=", "5")
	var analyze_signal_variance := _flag_enabled("--analyze-signal-variance")
	var signal_variance_output_path := _extract_argument("--signal-variance-output=", "")

	# Recognized prediction tuning override keys -> their CLI flag prefixes.
	var prediction_override_flags: Dictionary = {
		"logistic_k": "--prediction-logistic-k=",
		"score_sharpness": "--prediction-score-sharpness=",
		"interaction_weight": "--prediction-interaction-weight=",
		"synergy_amplification": "--prediction-synergy-amplification=",
		"matchup_amplification": "--prediction-matchup-amplification=",
		"scoring_mode": "--prediction-scoring-mode=",
		"variance_weight": "--prediction-variance-weight=",
		"cc_weight": "--prediction-cc-weight=",
		"mobility_weight": "--prediction-mobility-weight=",
		"sustain_weight": "--prediction-sustain-weight=",
		"best_counter_weight": "--prediction-best-counter-weight=",
		"worst_counter_weight": "--prediction-worst-counter-weight=",
		"best_synergy_weight": "--prediction-best-synergy-weight=",
		"worst_synergy_weight": "--prediction-worst-synergy-weight=",
		"synergy_aggregation": "--prediction-synergy-aggregation=",
		"counter_aggregation": "--prediction-counter-aggregation=",
		"use_decorrelated_scoring": "--prediction-use-decorrelated-scoring",
		"draft_position": "--prediction-draft-position=",
		"early_pick_base_weight": "--prediction-early-pick-base-weight=",
		"late_pick_counter_weight": "--prediction-late-pick-counter-weight=",
	}
	var prediction_config_overrides: Dictionary = {}
	for override_key in prediction_override_flags:
		var flag_prefix: String = prediction_override_flags[override_key]
		var raw_value := _extract_argument(flag_prefix, "")
		if raw_value.is_empty():
			continue
		if not raw_value.is_valid_float():
			push_error("generate_simulation_stats: %s%s is not a valid number" % [flag_prefix, raw_value])
			quit(1)
			return
		prediction_config_overrides[override_key] = raw_value.to_float()

	var prediction_sweep_param := _extract_argument("--prediction-sweep-param=", "")
	var prediction_sweep_values_raw := _extract_argument("--prediction-sweep-values=", "")
	var prediction_sweep_values: Array[float] = []
	if not prediction_sweep_param.is_empty():
		if not prediction_override_flags.has(prediction_sweep_param):
			push_error("generate_simulation_stats: --prediction-sweep-param=%s is not a recognized override key (expected one of: %s)" % [
				prediction_sweep_param, ", ".join(prediction_override_flags.keys())
			])
			quit(1)
			return
		if prediction_sweep_values_raw.is_empty():
			push_error("generate_simulation_stats: --prediction-sweep-param set but --prediction-sweep-values is empty")
			quit(1)
			return
		for part in prediction_sweep_values_raw.split(","):
			var t: String = part.strip_edges()
			if t.is_empty():
				continue
			if not t.is_valid_float():
				push_error("generate_simulation_stats: --prediction-sweep-values contains invalid number '%s'" % t)
				quit(1)
				return
			prediction_sweep_values.append(t.to_float())
		if prediction_sweep_values.is_empty():
			push_error("generate_simulation_stats: --prediction-sweep-param set but --prediction-sweep-values has no valid numbers")
			quit(1)
			return
	elif not prediction_sweep_values_raw.is_empty():
		push_error("generate_simulation_stats: --prediction-sweep-values set but --prediction-sweep-param is empty")
		quit(1)
		return

	var arr: Array[int] = []
	for part in sizes_raw.split(","):
		var t: String = part.strip_edges()
		if t.is_empty():
			continue
		arr.append(int(t))
	if arr.is_empty():
		push_error("generate_simulation_stats: no team sizes")
		quit(1)
		return
	var prediction_sizes: Array[int] = []
	for part in prediction_sizes_raw.split(","):
		var t: String = part.strip_edges()
		if t.is_empty():
			continue
		prediction_sizes.append(int(t))
	if evaluate_draft_predictions and prediction_sizes.is_empty():
		push_error("generate_simulation_stats: --evaluate-draft-predictions set but --prediction-team-sizes is empty")
		quit(1)
		return
	if analyze_signal_variance and signal_variance_output_path.is_empty():
		push_error("generate_simulation_stats: --analyze-signal-variance set but --signal-variance-output is empty")
		quit(1)
		return
	var gen := StatsSimulationCsvGeneratorScript.new()
	var err: Error = gen.run(
		out_dir,
		arr,
		matches,
		seed,
		export_workers,
		profile_enabled,
		write_match_log,
		aggregate_stats_in_worker,
		{},
		use_native_generated_stats,
		evaluate_draft_predictions,
		prediction_stats_dir,
		prediction_sizes,
		prediction_config_overrides,
		prediction_sweep_param,
		prediction_sweep_values
	)
	if err != OK:
		push_error("generate_simulation_stats failed: %s" % error_string(err))
		quit(1)
		return
	print("generate_simulation_stats: wrote CSVs to ", out_dir)

	if analyze_signal_variance:
		print()
		print("Running signal variance analysis...")
		var analyzer_script := load("res://scripts/tools/analyze_signal_variance.gd")
		if analyzer_script == null:
			push_error("Failed to load analyze_signal_variance.gd")
			quit(1)
			return
		var analyzer: RefCounted = analyzer_script.new()
		analyzer.analyze(out_dir, arr, 20, signal_variance_output_path)

	quit(0)
