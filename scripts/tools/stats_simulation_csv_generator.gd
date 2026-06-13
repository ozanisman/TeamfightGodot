class_name StatsSimulationCsvGenerator
extends RefCounted

## Runs native batch matches and writes stats CSVs via [StatsCsvAggregator].
## [param max_worker_threads]: 0 = auto (min of matches, CPU count, [constant DEFAULT_EXPORT_MAX_WORKER_THREADS]); else cap parallel workers.

const SimulationBatchWorkerScript := preload("res://scripts/simulation/simulation_batch_worker.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const StatsCsvAggregatorScript := preload("res://scripts/tools/stats_csv_aggregator.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")

## Upper bound on parallel export workers (avoids huge thread counts on high-core CPUs).
const DEFAULT_EXPORT_MAX_WORKER_THREADS: int = 16

## "Completed draft" for prediction-accuracy evaluation means a full 5-champion comp per side —
## the shape draft_testing.gd / predict_draft_winner are actually used with. Other team sizes in
## the batch (1-4) are partial comps that the draft model wasn't built to evaluate, so the
## current implementation only records team size 5 by default. The team-size filter is exposed
## as a parameter (prediction_team_sizes) so this can be expanded later without code changes.
const DEFAULT_PREDICTION_TEAM_SIZE: int = 5

## Confidence buckets for the draft-prediction calibration table, keyed on the model's predicted
## win-probability for whichever side it picked (i.e. how sure it was about its own pick).
const PREDICTION_CONFIDENCE_BUCKETS: Array[Dictionary] = [
	{"label": "50-55%", "min": 0.50, "max": 0.55},
	{"label": "55-60%", "min": 0.55, "max": 0.60},
	{"label": "60-70%", "min": 0.60, "max": 0.70},
	{"label": "70%+", "min": 0.70, "max": 1.001},
]

## predict_draft_winner's tunable scoring-sensitivity surface, mapped to its own defaults.
## prediction_config_overrides may set any subset of these keys (others fall back to the
## defaults below, which match predict_draft_winner's own — so an empty override dict
## reproduces today's exact evaluation behavior). Also doubles as the whitelist for
## prediction_sweep_param validation.
## scoring_mode is an enum ordinal (ScoringMode: CERTIFIED_PAIRWISE_PROBABILITY=0,
## DRAFT_AWARE_PAIRWISE_PROBABILITY=1) passed
## as a float here for uniformity with the other sweepable knobs; _score_prediction_config casts
## it to int before forwarding to predict_draft_winner.
const PREDICTION_OVERRIDE_DEFAULTS: Dictionary = {
	"logistic_k": 10.0,
	"synergy_amplification": 1.2,
	"matchup_amplification": 1.2,
	"scoring_mode": 0.0,  # CERTIFIED_PAIRWISE_PROBABILITY
	"variance_weight": 0.0,
	"cc_weight": 0.0,
	"mobility_weight": 0.0,
	"sustain_weight": 0.0,
	"best_counter_weight": 0.0,
	"worst_counter_weight": 0.0,
	"best_synergy_weight": 0.0,
	"worst_synergy_weight": 0.0,
	"synergy_aggregation": 0.0,
	"counter_aggregation": 0.0,
	"use_decorrelated_scoring": false,
	"draft_position": 0.0,
	"early_pick_base_weight": 0.7,
	"late_pick_counter_weight": 0.4,
}

## Minimum number of evaluated-match appearances before a champion is surfaced in the
## "Misclassification hotspots" section of the report — filters out noisy small-sample miss rates.
const MISCLASSIFICATION_MIN_APPEARANCES: int = 20

## Set by [method _evaluate_draft_predictions] to the full report text it pushed to the
## terminal — lets callers (e.g. the stats dashboard, running this on a background thread)
## surface the same report in their own UI after [method run]/[method run_packed] returns.
var last_prediction_report: String = ""


static func _now_ns() -> int:
	return Time.get_ticks_usec() * 1000


static func _emit_profile_line(payload: Dictionary) -> void:
	printerr(JSON.stringify(payload))


static func _profile_percent(part_ns: int, total_ns: int) -> float:
	if total_ns <= 0:
		return 0.0
	return 100.0 * float(part_ns) / float(total_ns)


static func _rank_ns_fields(source: Dictionary, fields: Array[String]) -> Array:
	var ranked: Array = []
	var total_ns: int = 0
	for field in fields:
		total_ns += int(source.get(field, 0))
	for field in fields:
		var value_ns: int = int(source.get(field, 0))
		if value_ns <= 0:
			continue
		ranked.append({
			"phase": field,
			"ns": value_ns,
			"pct": _profile_percent(value_ns, total_ns),
		})
	ranked.sort_custom(func(left: Dictionary, right: Dictionary) -> bool:
		return int(left.get("ns", 0)) > int(right.get("ns", 0))
	)
	return ranked


static func _team_size_rankings(team_size_ns: Dictionary, total_ns: int) -> Array:
	var ranked: Array = []
	for key in team_size_ns.keys():
		var value_ns: int = int(team_size_ns[key])
		if value_ns <= 0:
			continue
		ranked.append({
			"team_size": int(key),
			"ns": value_ns,
			"pct": _profile_percent(value_ns, total_ns),
		})
	ranked.sort_custom(func(left: Dictionary, right: Dictionary) -> bool:
		return int(left.get("ns", 0)) > int(right.get("ns", 0))
	)
	return ranked


static func _build_profile_summary(profile_state: Dictionary) -> Dictionary:
	var wall_ns: int = int(profile_state.get("wall_ns", 0))
	var chunk_total_ns: int = int(profile_state.get("chunk_total_ns", 0))
	var chunk_count: int = maxi(0, int(profile_state.get("chunk_count", 0)))
	var match_count: int = maxi(0, int(profile_state.get("match_count", 0)))
	var total_match_path_ns: int = int(profile_state.get("assembly_ns", 0)) + int(profile_state.get("native_run_ns", 0)) + int(profile_state.get("matchup_ns", 0)) + int(profile_state.get("clear_ns", 0))
	var native_profile_total_ns: int = (
		int(profile_state.get("native_setup_ns", 0))
		+ int(profile_state.get("native_simulate_ns", 0))
		+ int(profile_state.get("native_stats_aggregation_ns", 0))
		+ int(profile_state.get("native_matchup_aggregation_ns", 0))
		+ int(profile_state.get("native_result_build_ns", 0))
		+ int(profile_state.get("native_reset_ns", 0))
		+ int(profile_state.get("native_progress_ns", 0))
	)
	var setup_overhead_ns: int = int(profile_state.get("probe_ns", 0)) + int(profile_state.get("progress_reset_ns", 0)) + int(profile_state.get("worker_startup_ns", 0))
	var worker_wait_wall_ns: int = int(profile_state.get("worker_join_ns", 0))
	var bookkeeping_total_ns: int = int(profile_state.get("aggregation_ns", 0)) + int(profile_state.get("csv_write_ns", 0)) + int(profile_state.get("matchup_write_ns", 0))
	var main_thread_accounted_wall_ns: int = setup_overhead_ns + worker_wait_wall_ns + bookkeeping_total_ns
	var per_match_ns: float = float(chunk_total_ns) / float(match_count) if match_count > 0 else 0.0
	var per_chunk_ns: float = float(chunk_total_ns) / float(chunk_count) if chunk_count > 0 else 0.0
	var profile_breakdown: Dictionary = {
		"setup_overhead_ns": setup_overhead_ns,
		"worker_wait_wall_ns": worker_wait_wall_ns,
		"main_thread_bookkeeping_ns": bookkeeping_total_ns,
		"worker_chunk_sum_ns": chunk_total_ns,
		"match_path_sum_ns": total_match_path_ns,
		"main_thread_accounted_wall_ns": main_thread_accounted_wall_ns,
	}
	var wall_breakdown: Dictionary = {
		"setup_overhead_ns": setup_overhead_ns,
		"worker_wait_wall_ns": worker_wait_wall_ns,
		"main_thread_bookkeeping_ns": bookkeeping_total_ns,
	}
	var setup_rankings: Array = _rank_ns_fields(profile_state, ["probe_ns", "progress_reset_ns", "worker_startup_ns"])
	var wall_phase_rankings: Array = _rank_ns_fields(wall_breakdown, ["setup_overhead_ns", "worker_wait_wall_ns", "main_thread_bookkeeping_ns"])
	var bookkeeping_rankings: Array = _rank_ns_fields(profile_state, ["aggregation_ns", "csv_write_ns", "matchup_write_ns"])
	var chunk_rankings: Array = _rank_ns_fields(profile_state, ["assembly_ns", "native_run_ns", "matchup_ns", "clear_ns"])
	var native_rankings: Array = _rank_ns_fields(profile_state, ["native_setup_ns", "native_simulate_ns", "native_stats_aggregation_ns", "native_matchup_aggregation_ns", "native_result_build_ns", "native_reset_ns", "native_progress_ns"])
	return {
		"wall_ns": wall_ns,
		"match_count": match_count,
		"chunk_count": chunk_count,
		"avg_ns_per_match": per_match_ns,
		"avg_ns_per_chunk": per_chunk_ns,
		"setup_phase_rankings": setup_rankings,
		"wall_phase_rankings": wall_phase_rankings,
		"bookkeeping_phase_rankings": bookkeeping_rankings,
		"chunk_phase_rankings": chunk_rankings,
		"native_phase_rankings": native_rankings,
		"top_level_rankings": wall_phase_rankings,
		"dominant_wall_phase": wall_phase_rankings[0]["phase"] if not wall_phase_rankings.is_empty() else "",
		"dominant_top_level_phase": wall_phase_rankings[0]["phase"] if not wall_phase_rankings.is_empty() else "",
		"dominant_chunk_phase": chunk_rankings[0]["phase"] if not chunk_rankings.is_empty() else "",
		"team_size_rankings": _team_size_rankings(Dictionary(profile_state.get("team_size_ns", {})), wall_ns),
		"wall_phase_percentages": {
			"setup_overhead_pct_of_wall": _profile_percent(setup_overhead_ns, wall_ns),
			"worker_wait_pct_of_wall": _profile_percent(worker_wait_wall_ns, wall_ns),
			"bookkeeping_pct_of_wall": _profile_percent(bookkeeping_total_ns, wall_ns),
			"chunk_sum_pct_of_wall": _profile_percent(chunk_total_ns, wall_ns),
			"match_path_sum_pct_of_wall": _profile_percent(total_match_path_ns, wall_ns),
		},
		"top_level_percentages": {
			"setup_overhead_pct_of_wall": _profile_percent(setup_overhead_ns, wall_ns),
			"worker_wait_pct_of_wall": _profile_percent(worker_wait_wall_ns, wall_ns),
			"bookkeeping_pct_of_wall": _profile_percent(bookkeeping_total_ns, wall_ns),
			"chunk_sum_pct_of_wall": _profile_percent(chunk_total_ns, wall_ns),
			"match_path_sum_pct_of_wall": _profile_percent(total_match_path_ns, wall_ns),
		},
		"profile_summary_notes": [
			"worker_wait_wall_ns comes from worker_join_ns and is elapsed wait time, not setup work.",
			"worker_chunk_sum_ns and match_path_sum_ns are summed across chunks and can exceed wall_ns when workers run in parallel.",
		],
		"profile_breakdown_ns": profile_breakdown,
		"native_profile_total_ns": native_profile_total_ns,
	}


func run_packed(p: Dictionary) -> int:
	var raw_sizes: Array = Array(p.get("team_sizes", []))
	var arr: Array[int] = []
	for x in raw_sizes:
		arr.append(int(x))
	var role_override: Variant = p.get("role_by_hero_map", {})
	var role_dict: Dictionary = {}
	if role_override is Dictionary:
		role_dict = Dictionary(role_override)
	var raw_prediction_sizes: Array = Array(p.get("prediction_team_sizes", [DEFAULT_PREDICTION_TEAM_SIZE]))
	var prediction_sizes: Array[int] = []
	for x in raw_prediction_sizes:
		prediction_sizes.append(int(x))
	var prediction_config_overrides: Dictionary = {}
	var raw_overrides: Variant = p.get("prediction_config_overrides", {})
	if raw_overrides is Dictionary:
		prediction_config_overrides = Dictionary(raw_overrides).duplicate()
	var prediction_sweep_param: String = str(p.get("prediction_sweep_param", ""))
	var raw_sweep_values: Array = Array(p.get("prediction_sweep_values", []))
	var prediction_sweep_values: Array[float] = []
	for x in raw_sweep_values:
		prediction_sweep_values.append(float(x))
	return run(
		str(p.get("output_dir", "")),
		arr,
		int(p.get("matches_per_size", 0)),
		int(p.get("base_seed", 0)),
		int(p.get("max_worker_threads", 0)),
		bool(p.get("profile_stats", false)),
		bool(p.get("write_match_log", false)),
		bool(p.get("aggregate_stats_in_worker", true)),
		role_dict,
		bool(p.get("use_native_generated_stats", true)),
		bool(p.get("evaluate_draft_predictions", false)),
		str(p.get("prediction_stats_dir", "res://stats_output")),
		prediction_sizes,
		prediction_config_overrides,
		prediction_sweep_param,
		prediction_sweep_values
	)


func run(
	output_dir: String,
	team_sizes: Array[int],
	matches_per_size: int,
	base_seed: int,
	max_worker_threads: int = 0,
	profile_stats: bool = false,
	write_match_log: bool = false,
	aggregate_stats_in_worker: bool = true,
	role_by_hero_map_override: Dictionary = {},
	use_native_generated_stats: bool = true,
	evaluate_draft_predictions: bool = false,
	prediction_stats_dir: String = "res://model_stats/certified_pairwise_testing_250k",
	prediction_team_sizes: Array[int] = [DEFAULT_PREDICTION_TEAM_SIZE],
	prediction_config_overrides: Dictionary = {},
	prediction_sweep_param: String = "",
	prediction_sweep_values: Array[float] = []
) -> Error:
	if output_dir.is_empty():
		return ERR_INVALID_PARAMETER
	if team_sizes.is_empty():
		return ERR_INVALID_PARAMETER
	if matches_per_size < 1:
		return ERR_INVALID_PARAMETER
	if evaluate_draft_predictions and not write_match_log:
		push_warning("StatsSimulationCsvGenerator: evaluate_draft_predictions requires write_match_log (final comps are read from match logs); enabling it.")
		write_match_log = true
	var role_by_hero_map: Dictionary = role_by_hero_map_override if not role_by_hero_map_override.is_empty() else ChampionCatalogScript.build_role_by_hero_map()
	var roles_for_workers: Dictionary = role_by_hero_map.duplicate() if aggregate_stats_in_worker else {}
	var run_start_ns: int = _now_ns()
	var profile_state: Dictionary = {}
	if profile_stats:
		profile_state = {
			"enabled": true,
			"output_dir": output_dir,
			"team_sizes": team_sizes.duplicate(),
			"matches_per_size": matches_per_size,
			"base_seed": base_seed,
			"max_worker_threads": max_worker_threads,
			"write_match_log": write_match_log,
			"aggregate_stats_in_worker": aggregate_stats_in_worker,
			"probe_ns": 0,
			"progress_reset_ns": 0,
			"team_size_ns": {},
			"worker_startup_ns": 0,
			"worker_join_ns": 0,
			"aggregation_ns": 0,
			"csv_write_ns": 0,
			"matchup_write_ns": 0,
			"chunk_count": 0,
			"match_count": 0,
			"chunk_total_ns": 0,
			"assembly_ns": 0,
			"native_run_ns": 0,
			"native_setup_ns": 0,
			"native_simulate_ns": 0,
			"native_stats_aggregation_ns": 0,
			"native_matchup_aggregation_ns": 0,
			"native_result_build_ns": 0,
			"native_reset_ns": 0,
			"native_progress_ns": 0,
			"matchup_ns": 0,
			"clear_ns": 0,
		}
	var probe_start_ns: int = _now_ns()
	var probe := NativeSimulationBackendScript.new()
	if not probe.is_available():
		return ERR_UNAVAILABLE
	if profile_stats:
		profile_state["probe_ns"] = _now_ns() - probe_start_ns
	var total_matches: int = team_sizes.size() * matches_per_size
	var reset_start_ns: int = _now_ns()
	SimulationBatchWorkerScript.reset_benchmark_progress(total_matches)
	if profile_stats:
		profile_state["progress_reset_ns"] = _now_ns() - reset_start_ns
	var aggregator := StatsCsvAggregatorScript.new()
	aggregator.set_write_match_log(write_match_log)
	aggregator.reset()
	aggregator.preload_roles(role_by_hero_map)
	for sz in team_sizes:
		var size_start_ns: int = _now_ns()
		var per_size_seed: int = base_seed + int(sz) * 1_009_033
		var err_sz: Error = _run_matches_for_team_size(
			aggregator,
			int(sz),
			per_size_seed,
			matches_per_size,
			max_worker_threads,
			profile_state if profile_stats else null,
			write_match_log,
			aggregate_stats_in_worker,
			roles_for_workers,
			use_native_generated_stats
		)
		if err_sz != OK:
			return err_sz
		if profile_stats:
			var team_ns: Dictionary = Dictionary(profile_state.get("team_size_ns", {}))
			team_ns[int(sz)] = _now_ns() - size_start_ns
			profile_state["team_size_ns"] = team_ns

	# Write regular CSV files
	var csv_start_ns: int = _now_ns()
	var csv_err: Error = aggregator.write_to_dir(output_dir)
	if csv_err != OK:
		return csv_err
	if profile_stats:
		profile_state["csv_write_ns"] = _now_ns() - csv_start_ns
	
	# Write matchup data file
	var matchup_start_ns: int = _now_ns()
	var matchup_success: bool = aggregator.write_matchup_file(output_dir)
	if not matchup_success:
		push_error("Failed to write matchup data file")
		return ERR_FILE_CANT_WRITE
	if profile_stats:
		profile_state["matchup_write_ns"] = _now_ns() - matchup_start_ns
		profile_state["wall_ns"] = _now_ns() - run_start_ns
		_emit_profile_line({
			"stats_profile": profile_state,
			"stats_profile_summary": _build_profile_summary(profile_state),
		})

	if evaluate_draft_predictions:
		_evaluate_draft_predictions(
			aggregator.get_match_logs(),
			prediction_team_sizes,
			prediction_stats_dir,
			prediction_config_overrides,
			prediction_sweep_param,
			prediction_sweep_values
		)

	return OK


static func _confidence_bucket_index(confidence: float) -> int:
	for i in range(PREDICTION_CONFIDENCE_BUCKETS.size()):
		var bucket: Dictionary = PREDICTION_CONFIDENCE_BUCKETS[i]
		if confidence >= float(bucket["min"]) and confidence < float(bucket["max"]):
			return i
	return PREDICTION_CONFIDENCE_BUCKETS.size() - 1


## Runs predict_draft_winner on the final comp of every "completed draft" match (per
## prediction_team_sizes, default [5] — see DEFAULT_PREDICTION_TEAM_SIZE), scores predictions
## against actual outcomes, and pushes ONE consolidated report. prediction_stats_dir should point
## at a stats snapshot that is NOT the one this run just (re)generated — predicting against stats
## derived from the very matches being predicted would be in-sample/circular and inflate accuracy.
##
## prediction_config_overrides may override any subset of PREDICTION_OVERRIDE_DEFAULTS' keys
## (logistic_k / synergy_amplification / matchup_amplification / scoring_mode) — e.g. to evaluate a candidate tuning instead of predict_draft_winner's
## own defaults. If prediction_sweep_param + prediction_sweep_values are both non-empty, runs the
## evaluation once per swept value (varying only that one key on top of the base overrides) and
## emits a single comparison report instead of a single-config report — letting you A/B candidate
## tunings (e.g. for the "underconfident" calibration issue) without re-running the batch each time.
func _evaluate_draft_predictions(match_logs: Array, eval_team_sizes: Array[int], prediction_stats_dir: String, config_overrides: Dictionary = {}, sweep_param: String = "", sweep_values: Array[float] = []) -> void:
	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("draft_prediction_eval: native simulation backend unavailable; skipping accuracy report")
		return

	var report: String
	if not sweep_param.is_empty() and not sweep_values.is_empty():
		var metrics_list: Array[Dictionary] = []
		for value in sweep_values:
			var overrides: Dictionary = config_overrides.duplicate()
			overrides[sweep_param] = value
			metrics_list.append(_score_prediction_config(backend, match_logs, eval_team_sizes, prediction_stats_dir, overrides))
		report = _build_sweep_report(sweep_param, sweep_values, metrics_list, eval_team_sizes, prediction_stats_dir)
	else:
		var metrics: Dictionary = _score_prediction_config(backend, match_logs, eval_team_sizes, prediction_stats_dir, config_overrides)
		report = _build_single_config_report(metrics, eval_team_sizes, prediction_stats_dir)

	last_prediction_report = report
	push_warning(report)


## Runs predict_draft_winner (with config_overrides layered onto PREDICTION_OVERRIDE_DEFAULTS)
## against every eligible match in match_logs and scores the predictions against actual outcomes.
## Returns a metrics dict consumed by the report builders below; pushes nothing itself, so it can
## be reused for both single-config and per-sweep-value runs.
func _score_prediction_config(backend: RefCounted, match_logs: Array, eval_team_sizes: Array[int], prediction_stats_dir: String, config_overrides: Dictionary) -> Dictionary:
	var ov: Dictionary = PREDICTION_OVERRIDE_DEFAULTS.duplicate()
	for key in config_overrides:
		ov[key] = config_overrides[key]

	var total: int = 0
	var correct: int = 0
	var draws_skipped: int = 0
	var missing_comp_skipped: int = 0
	var brier_sum: float = 0.0
	var log_loss_sum: float = 0.0
	var confidence_correct_sum: float = 0.0
	var confidence_incorrect_sum: float = 0.0
	var bucket_count := PREDICTION_CONFIDENCE_BUCKETS.size()
	var bucket_n: Array[int] = []
	var bucket_correct_n: Array[int] = []
	var bucket_confidence_sum: Array[float] = []
	for _i in range(bucket_count):
		bucket_n.append(0)
		bucket_correct_n.append(0)
		bucket_confidence_sum.append(0.0)
	# Per-champion misclassification tallies (see MISCLASSIFICATION_MIN_APPEARANCES / hotspot report).
	var champ_total: Dictionary = {}
	var champ_miss_count: Dictionary = {}
	var champ_miss_confidence_sum: Dictionary = {}
	# Per-role misclassification tallies (mirrors champion tracking for role-level hotspot report).
	var role_total: Dictionary = {}
	var role_miss_count: Dictionary = {}
	var role_miss_confidence_sum: Dictionary = {}
	var role_by_hero: Dictionary = ChampionCatalogScript.build_role_by_hero_map()

	for log_entry in match_logs:
		var log: Dictionary = Dictionary(log_entry)
		if int(log.get("team_size", 0)) not in eval_team_sizes:
			continue
		var winner: String = String(log.get("winner", ""))
		if winner != "player" and winner != "enemy":
			draws_skipped += 1
			continue
		var player_comp: Array = Array(log.get("player_comp", []))
		var enemy_comp: Array = Array(log.get("enemy_comp", []))
		if player_comp.is_empty() or enemy_comp.is_empty():
			missing_comp_skipped += 1
			continue

		var prediction: Dictionary = backend.predict_draft_winner(
			player_comp, enemy_comp, prediction_stats_dir,
			0.50, 0.25, 0.25, 0.25, 0.0,
			float(ov["logistic_k"]), false,
			float(ov["synergy_amplification"]), float(ov["matchup_amplification"]),
			int(ov["scoring_mode"]), float(ov["variance_weight"]),
			float(ov["cc_weight"]), float(ov["mobility_weight"]), float(ov["sustain_weight"]),
			float(ov["best_counter_weight"]), float(ov["worst_counter_weight"]),
			float(ov["best_synergy_weight"]), float(ov["worst_synergy_weight"]),
			int(ov["synergy_aggregation"]), int(ov["counter_aggregation"]),
			bool(ov["use_decorrelated_scoring"]),
			int(ov["draft_position"]), float(ov["early_pick_base_weight"]), float(ov["late_pick_counter_weight"])
		)
		var team1_prob: float = clampf(float(prediction.get("team1_prob", 0.5)), 0.0, 1.0)
		var actual_player_win: bool = winner == "player"
		var predicted_player_win: bool = team1_prob >= 0.5
		var is_correct: bool = predicted_player_win == actual_player_win
		# "Confidence" = probability the model assigned to whichever side it actually picked.
		var confidence: float = team1_prob if predicted_player_win else (1.0 - team1_prob)
		var p_actual: float = team1_prob if actual_player_win else (1.0 - team1_prob)
		var actual_outcome: float = 1.0 if actual_player_win else 0.0

		total += 1
		if is_correct:
			correct += 1
			confidence_correct_sum += confidence
		else:
			confidence_incorrect_sum += confidence
		# Brier score and log loss are proper scoring rules: they reward both picking the right
		# side AND being well-calibrated about how confident to be (unlike raw accuracy).
		brier_sum += (team1_prob - actual_outcome) * (team1_prob - actual_outcome)
		log_loss_sum += -log(maxf(p_actual, 1e-9))

		var bucket_index: int = _confidence_bucket_index(confidence)
		bucket_n[bucket_index] += 1
		bucket_confidence_sum[bucket_index] += confidence
		if is_correct:
			bucket_correct_n[bucket_index] += 1

		for champ_variant in (player_comp + enemy_comp):
			var champ := String(champ_variant)
			champ_total[champ] = int(champ_total.get(champ, 0)) + 1
			if not is_correct:
				champ_miss_count[champ] = int(champ_miss_count.get(champ, 0)) + 1
				champ_miss_confidence_sum[champ] = float(champ_miss_confidence_sum.get(champ, 0.0)) + confidence
			# Aggregate role data (mirrors champion tracking)
			var role: String = String(role_by_hero.get(champ, "unknown"))
			role_total[role] = int(role_total.get(role, 0)) + 1
			if not is_correct:
				role_miss_count[role] = int(role_miss_count.get(role, 0)) + 1
				role_miss_confidence_sum[role] = float(role_miss_confidence_sum.get(role, 0.0)) + confidence

	return {
		"total": total,
		"correct": correct,
		"draws_skipped": draws_skipped,
		"missing_comp_skipped": missing_comp_skipped,
		"brier_sum": brier_sum,
		"log_loss_sum": log_loss_sum,
		"confidence_correct_sum": confidence_correct_sum,
		"confidence_incorrect_sum": confidence_incorrect_sum,
		"bucket_n": bucket_n,
		"bucket_correct_n": bucket_correct_n,
		"bucket_confidence_sum": bucket_confidence_sum,
		"champ_total": champ_total,
		"champ_miss_count": champ_miss_count,
		"champ_miss_confidence_sum": champ_miss_confidence_sum,
		"role_total": role_total,
		"role_miss_count": role_miss_count,
		"role_miss_confidence_sum": role_miss_confidence_sum,
	}


## Mean of (actual-correct% − predicted-confidence%) across non-empty calibration buckets.
## Positive = model underconfident (actual beats stated confidence, as observed); negative =
## overconfident; closer to 0 = better-calibrated. Used to compare configs at a glance in sweeps.
func _mean_calibration_gap(metrics: Dictionary) -> float:
	var bucket_n: Array = metrics["bucket_n"]
	var bucket_correct_n: Array = metrics["bucket_correct_n"]
	var bucket_confidence_sum: Array = metrics["bucket_confidence_sum"]
	var gap_sum: float = 0.0
	var gap_count: int = 0
	for i in range(PREDICTION_CONFIDENCE_BUCKETS.size()):
		var n: int = int(bucket_n[i])
		if n == 0:
			continue
		var avg_confidence: float = float(bucket_confidence_sum[i]) / float(n)
		var bucket_accuracy: float = float(bucket_correct_n[i]) / float(n)
		gap_sum += (bucket_accuracy - avg_confidence) * 100.0
		gap_count += 1
	return (gap_sum / float(gap_count)) if gap_count > 0 else 0.0


## Builds the "Misclassification hotspots" lines: champions appearing in at least
## MISCLASSIFICATION_MIN_APPEARANCES evaluated matches, sorted by miss-rate descending, top 5.
func _build_misclassification_hotspot_lines(metrics: Dictionary) -> Array[String]:
	var champ_total: Dictionary = metrics["champ_total"]
	var champ_miss_count: Dictionary = metrics["champ_miss_count"]
	var champ_miss_confidence_sum: Dictionary = metrics["champ_miss_confidence_sum"]

	var entries: Array[Dictionary] = []
	for champ in champ_total:
		var n: int = int(champ_total[champ])
		if n < MISCLASSIFICATION_MIN_APPEARANCES:
			continue
		var miss_n: int = int(champ_miss_count.get(champ, 0))
		var mean_conf_when_wrong: float = (float(champ_miss_confidence_sum.get(champ, 0.0)) / float(miss_n)) if miss_n > 0 else 0.0
		entries.append({
			"champion": champ,
			"n": n,
			"miss_n": miss_n,
			"miss_rate": 100.0 * float(miss_n) / float(n),
			"mean_conf_when_wrong": mean_conf_when_wrong,
		})
	entries.sort_custom(func(a, b): return float(a["miss_rate"]) > float(b["miss_rate"]))

	var lines: Array[String] = []
	for i in range(mini(5, entries.size())):
		var e: Dictionary = entries[i]
		lines.append("    %-24s  %.1f%% miss rate (%d/%d matches), mean confidence when wrong: %.1f%%" % [
			String(e["champion"]), float(e["miss_rate"]), int(e["miss_n"]), int(e["n"]), float(e["mean_conf_when_wrong"]) * 100.0
		])
	return lines


## Builds the "Least misclassified champions" lines: champions appearing in at least
## MISCLASSIFICATION_MIN_APPEARANCES evaluated matches, sorted by miss-rate ascending, top 5.
func _build_least_misclassified_lines(metrics: Dictionary) -> Array[String]:
	var champ_total: Dictionary = metrics["champ_total"]
	var champ_miss_count: Dictionary = metrics["champ_miss_count"]
	var champ_miss_confidence_sum: Dictionary = metrics["champ_miss_confidence_sum"]

	var entries: Array[Dictionary] = []
	for champ in champ_total:
		var n: int = int(champ_total[champ])
		if n < MISCLASSIFICATION_MIN_APPEARANCES:
			continue
		var miss_n: int = int(champ_miss_count.get(champ, 0))
		var mean_conf_when_wrong: float = (float(champ_miss_confidence_sum.get(champ, 0.0)) / float(miss_n)) if miss_n > 0 else 0.0
		entries.append({
			"champion": champ,
			"n": n,
			"miss_n": miss_n,
			"miss_rate": 100.0 * float(miss_n) / float(n),
			"mean_conf_when_wrong": mean_conf_when_wrong,
		})
	entries.sort_custom(func(a, b): return float(a["miss_rate"]) < float(b["miss_rate"]))

	var lines: Array[String] = []
	for i in range(mini(5, entries.size())):
		var e: Dictionary = entries[i]
		lines.append("    %-24s  %.1f%% miss rate (%d/%d matches), mean confidence when wrong: %.1f%%" % [
			String(e["champion"]), float(e["miss_rate"]), int(e["miss_n"]), int(e["n"]), float(e["mean_conf_when_wrong"]) * 100.0
		])
	return lines


## Builds the "Misclassification hotspots by role" lines: all 6 roles, sorted by miss-rate descending.
func _build_role_misclassification_hotspot_lines(metrics: Dictionary) -> Array[String]:
	var role_total: Dictionary = metrics["role_total"]
	var role_miss_count: Dictionary = metrics["role_miss_count"]
	var role_miss_confidence_sum: Dictionary = metrics["role_miss_confidence_sum"]

	var entries: Array[Dictionary] = []
	for role in role_total:
		var n: int = int(role_total[role])
		var miss_n: int = int(role_miss_count.get(role, 0))
		var mean_conf_when_wrong: float = (float(role_miss_confidence_sum.get(role, 0.0)) / float(miss_n)) if miss_n > 0 else 0.0
		entries.append({
			"role": role,
			"n": n,
			"miss_n": miss_n,
			"miss_rate": 100.0 * float(miss_n) / float(n),
			"mean_conf_when_wrong": mean_conf_when_wrong,
		})
	entries.sort_custom(func(a, b): return float(a["miss_rate"]) > float(b["miss_rate"]))

	var lines: Array[String] = []
	for e in entries:
		lines.append("    %-16s  %.1f%% miss rate (%d/%d matches), mean confidence when wrong: %.1f%%" % [
			String(e["role"]).capitalize(), float(e["miss_rate"]), int(e["miss_n"]), int(e["n"]), float(e["mean_conf_when_wrong"]) * 100.0
		])
	return lines


func _build_single_config_report(metrics: Dictionary, eval_team_sizes: Array[int], prediction_stats_dir: String) -> String:
	var total: int = int(metrics["total"])
	var draws_skipped: int = int(metrics["draws_skipped"])
	var missing_comp_skipped: int = int(metrics["missing_comp_skipped"])

	var lines: Array[String] = []
	lines.append("=== Draft Prediction Accuracy (predicted winner vs actual match outcome) ===")
	lines.append("  Stats source: %s  (held out from this run's freshly-generated stats; out-of-sample)" % prediction_stats_dir)
	lines.append("  Team sizes evaluated: %s  (\"completed draft\" = full comp; see DEFAULT_PREDICTION_TEAM_SIZE)" % [str(eval_team_sizes)])
	if total == 0:
		lines.append("  No eligible matches found. (draws skipped: %d, missing comp data: %d)" % [draws_skipped, missing_comp_skipped])
		lines.append("=== End Draft Prediction Accuracy ===")
		return "\n" + "\n".join(lines) + "\n"

	var correct: int = int(metrics["correct"])
	var incorrect: int = total - correct
	var accuracy_pct: float = 100.0 * float(correct) / float(total)
	var brier_score: float = float(metrics["brier_sum"]) / float(total)
	var log_loss: float = float(metrics["log_loss_sum"]) / float(total)
	var mean_conf_correct: float = (float(metrics["confidence_correct_sum"]) / float(correct)) if correct > 0 else 0.0
	var mean_conf_incorrect: float = (float(metrics["confidence_incorrect_sum"]) / float(incorrect)) if incorrect > 0 else 0.0
	var coin_flip_brier: float = 0.25
	var coin_flip_log_loss: float = log(2.0)

	lines.append("  Matches evaluated: %d   (draws skipped: %d, missing comp data: %d)" % [total, draws_skipped, missing_comp_skipped])
	lines.append("")
	lines.append("  Accuracy:    %.1f%%  (%d/%d correct)" % [accuracy_pct, correct, total])
	lines.append("  Brier score: %.4f   (lower is better; %.4f = always-predict-50%% baseline)" % [brier_score, coin_flip_brier])
	lines.append("  Log loss:    %.4f   (lower is better; %.4f = always-predict-50%% baseline)" % [log_loss, coin_flip_log_loss])
	lines.append("  Mean confidence on correct picks:   %.1f%%" % (mean_conf_correct * 100.0))
	lines.append("  Mean confidence on incorrect picks: %.1f%%" % (mean_conf_incorrect * 100.0))
	lines.append("")
	lines.append("  Calibration (model's confidence in its pick -> how often that pick was actually right):")
	var bucket_n: Array = metrics["bucket_n"]
	var bucket_correct_n: Array = metrics["bucket_correct_n"]
	var bucket_confidence_sum: Array = metrics["bucket_confidence_sum"]
	for i in range(PREDICTION_CONFIDENCE_BUCKETS.size()):
		var n: int = int(bucket_n[i])
		if n == 0:
			continue
		var bucket: Dictionary = PREDICTION_CONFIDENCE_BUCKETS[i]
		var avg_confidence: float = float(bucket_confidence_sum[i]) / float(n)
		var bucket_accuracy: float = 100.0 * float(bucket_correct_n[i]) / float(n)
		lines.append("    %-7s  predicted ~%.1f%% confident  ->  actually correct %.1f%%  (n=%d)" % [
			String(bucket["label"]), avg_confidence * 100.0, bucket_accuracy, n
		])
	lines.append("  (A well-calibrated model's \"actually correct %\" should track its stated confidence per bucket.)")

	lines.append("")
	lines.append("  Misclassification hotspots (champions appearing in >= %d evaluated matches, sorted by miss rate):" % MISCLASSIFICATION_MIN_APPEARANCES)
	var hotspot_lines: Array[String] = _build_misclassification_hotspot_lines(metrics)
	if hotspot_lines.is_empty():
		lines.append("    (no champion reached the minimum sample threshold)")
	else:
		for hotspot_line in hotspot_lines:
			lines.append(hotspot_line)

	lines.append("")
	lines.append("  Least misclassified champions (champions appearing in >= %d evaluated matches, sorted by miss rate):" % MISCLASSIFICATION_MIN_APPEARANCES)
	var least_lines: Array[String] = _build_least_misclassified_lines(metrics)
	if least_lines.is_empty():
		lines.append("    (no champion reached the minimum sample threshold)")
	else:
		for least_line in least_lines:
			lines.append(least_line)

	lines.append("")
	lines.append("  Misclassification hotspots by role:")
	var role_hotspot_lines: Array[String] = _build_role_misclassification_hotspot_lines(metrics)
	for role_hotspot_line in role_hotspot_lines:
		lines.append(role_hotspot_line)

	lines.append("=== End Draft Prediction Accuracy ===")
	return "\n" + "\n".join(lines) + "\n"


static func _format_sweep_value(value: float) -> String:
	var text := "%.4f" % value
	if text.contains("."):
		text = text.rstrip("0").rstrip(".")
	return text


func _build_sweep_report(sweep_param: String, sweep_values: Array[float], metrics_list: Array[Dictionary], eval_team_sizes: Array[int], prediction_stats_dir: String) -> String:
	var lines: Array[String] = []
	lines.append("=== Draft Prediction Accuracy Sweep (param: %s) ===" % sweep_param)
	lines.append("  Stats source: %s  (held out from this run's freshly-generated stats; out-of-sample)" % prediction_stats_dir)
	lines.append("  Team sizes evaluated: %s  (\"completed draft\" = full comp; see DEFAULT_PREDICTION_TEAM_SIZE)" % [str(eval_team_sizes)])
	lines.append("")
	lines.append("  %-10s %10s %10s %10s %16s %18s %14s" % [
		sweep_param, "Accuracy", "Brier", "LogLoss", "Conf(correct)", "Conf(incorrect)", "Calib gap"
	])
	for i in range(sweep_values.size()):
		var value: float = sweep_values[i]
		var metrics: Dictionary = metrics_list[i]
		var total: int = int(metrics["total"])
		var value_label := _format_sweep_value(value)
		if total == 0:
			lines.append("  %-10s   (no eligible matches — draws skipped: %d, missing comp data: %d)" % [
				value_label, int(metrics["draws_skipped"]), int(metrics["missing_comp_skipped"])
			])
			continue
		var correct: int = int(metrics["correct"])
		var incorrect: int = total - correct
		var accuracy_pct: float = 100.0 * float(correct) / float(total)
		var brier_score: float = float(metrics["brier_sum"]) / float(total)
		var log_loss: float = float(metrics["log_loss_sum"]) / float(total)
		var mean_conf_correct: float = (float(metrics["confidence_correct_sum"]) / float(correct)) if correct > 0 else 0.0
		var mean_conf_incorrect: float = (float(metrics["confidence_incorrect_sum"]) / float(incorrect)) if incorrect > 0 else 0.0
		var calibration_gap: float = _mean_calibration_gap(metrics)
		lines.append("  %-10s %9.1f%% %10.4f %10.4f %15.1f%% %17.1f%% %+13.1f%%" % [
			value_label, accuracy_pct, brier_score, log_loss,
			mean_conf_correct * 100.0, mean_conf_incorrect * 100.0, calibration_gap
		])
	lines.append("")
	lines.append("  Matches evaluated per row: %d   (draws skipped: %d, missing comp data: %d — same eligible set for every value)" % [
		int(metrics_list[0].get("total", 0)) if not metrics_list.is_empty() else 0,
		int(metrics_list[0].get("draws_skipped", 0)) if not metrics_list.is_empty() else 0,
		int(metrics_list[0].get("missing_comp_skipped", 0)) if not metrics_list.is_empty() else 0,
	])
	lines.append("  Calib gap = mean(actual-correct% − predicted-confidence%) across non-empty calibration buckets.")
	lines.append("  Positive = underconfident (actual beats stated confidence); negative = overconfident; closer to 0 = better calibrated.")
	lines.append("=== End Draft Prediction Accuracy Sweep ===")
	return "\n" + "\n".join(lines) + "\n"


func _worker_count_for_export(matches_per_size: int, max_worker_threads: int) -> int:
	if max_worker_threads > 0:
		return maxi(1, mini(matches_per_size, max_worker_threads))
	var cpu: int = maxi(1, OS.get_processor_count())
	return maxi(1, mini(mini(matches_per_size, cpu), DEFAULT_EXPORT_MAX_WORKER_THREADS))


func _run_matches_for_team_size(
	aggregator: RefCounted,
	team_size: int,
	per_size_seed: int,
	matches_per_size: int,
	max_worker_threads: int,
	profile_state: Variant = null,
	write_match_log: bool = false,
	aggregate_stats_in_worker: bool = true,
	role_by_hero_map: Dictionary = {},
	use_native_generated_stats: bool = true
) -> Error:
	var worker_count: int = _worker_count_for_export(matches_per_size, max_worker_threads)
	var slice: int = (matches_per_size + worker_count - 1) / worker_count
	var do_profile: bool = profile_state is Dictionary
	var startup_start_ns: int = _now_ns()
	const bench_skip_summaries_chunk: bool = false

	# Prepare context for WorkerThreadPool tasks
	var task_context := {
		"team_size": team_size,
		"base_seed": per_size_seed,
		"bench_skip_summaries": bench_skip_summaries_chunk,
		"allow_native_batch": true,
		"profile_stats": do_profile,
		"write_match_log": write_match_log,
		"aggregate_stats_in_worker": aggregate_stats_in_worker,
		"use_native_generated_stats": use_native_generated_stats,
		"role_by_hero_map": role_by_hero_map,
		"skip_catalog_thread_clear": aggregate_stats_in_worker and not bench_skip_summaries_chunk,
		"matches_per_size": matches_per_size,
		"slice": slice,
	}

	# Thread-safe result storage
	var results_mutex := Mutex.new()
	var batch_results: Array = []
	batch_results.resize(worker_count)

	# Task function for WorkerThreadPool
	var task_func = func(worker_index: int) -> void:
		var start_index: int = worker_index * slice
		var end_index: int = mini(matches_per_size, start_index + slice)
		if start_index >= end_index:
			results_mutex.lock()
			batch_results[worker_index] = []
			results_mutex.unlock()
			return

		var thread_data := {
			"start_index": start_index,
			"end_index": end_index,
			"team_size": team_size,
			"base_seed": per_size_seed,
			"bench_skip_summaries": bench_skip_summaries_chunk,
			"allow_native_batch": true,
			"profile_stats": do_profile,
			"write_match_log": write_match_log,
			"aggregate_stats_in_worker": aggregate_stats_in_worker,
			"use_native_generated_stats": use_native_generated_stats,
			"role_by_hero_map": role_by_hero_map,
			"skip_catalog_thread_clear": aggregate_stats_in_worker and not bench_skip_summaries_chunk,
		}

		var runner := SimulationBatchWorkerScript.new()
		var results: Variant = runner.run_chunk(thread_data)

		results_mutex.lock()
		batch_results[worker_index] = results if results is Array else []
		results_mutex.unlock()

	if do_profile:
		var ps: Dictionary = profile_state
		ps["worker_startup_ns"] = int(ps.get("worker_startup_ns", 0)) + (_now_ns() - startup_start_ns)
		ps["worker_count"] = worker_count

	# Use WorkerThreadPool instead of manual Thread creation
	var join_start_ns: int = _now_ns()
	var task_id: int = WorkerThreadPool.add_group_task(task_func, worker_count)
	WorkerThreadPool.wait_for_group_task_completion(task_id)

	if do_profile:
		var ps_join: Dictionary = profile_state
		ps_join["worker_join_ns"] = int(ps_join.get("worker_join_ns", 0)) + (_now_ns() - join_start_ns)

	# Process results
	for batch in batch_results:
		for entry in (batch as Array):
			if do_profile and entry is Dictionary:
				var chunk_dict: Dictionary = Dictionary(entry)
				if chunk_dict.has("profile_stats"):
					var chunk_profile: Dictionary = Dictionary(chunk_dict.get("profile_stats", {}))
					if not chunk_profile.is_empty():
						_emit_profile_line({"stats_profile_chunk": chunk_profile})
						var ps_chunk: Dictionary = profile_state
						ps_chunk["chunk_count"] = int(ps_chunk.get("chunk_count", 0)) + 1
						ps_chunk["chunk_total_ns"] = int(ps_chunk.get("chunk_total_ns", 0)) + int(chunk_profile.get("wall_ns", 0))
						ps_chunk["assembly_ns"] = int(ps_chunk.get("assembly_ns", 0)) + int(chunk_profile.get("assembly_ns", 0))
						ps_chunk["native_run_ns"] = int(ps_chunk.get("native_run_ns", 0)) + int(chunk_profile.get("native_run_ns", 0))
						ps_chunk["native_setup_ns"] = int(ps_chunk.get("native_setup_ns", 0)) + int(chunk_profile.get("native_setup_ns", 0))
						ps_chunk["native_simulate_ns"] = int(ps_chunk.get("native_simulate_ns", 0)) + int(chunk_profile.get("native_simulate_ns", 0))
						ps_chunk["native_stats_aggregation_ns"] = int(ps_chunk.get("native_stats_aggregation_ns", 0)) + int(chunk_profile.get("native_stats_aggregation_ns", 0))
						ps_chunk["native_matchup_aggregation_ns"] = int(ps_chunk.get("native_matchup_aggregation_ns", 0)) + int(chunk_profile.get("native_matchup_aggregation_ns", 0))
						ps_chunk["native_result_build_ns"] = int(ps_chunk.get("native_result_build_ns", 0)) + int(chunk_profile.get("native_result_build_ns", 0))
						ps_chunk["native_reset_ns"] = int(ps_chunk.get("native_reset_ns", 0)) + int(chunk_profile.get("native_reset_ns", 0))
						ps_chunk["native_progress_ns"] = int(ps_chunk.get("native_progress_ns", 0)) + int(chunk_profile.get("native_progress_ns", 0))
						ps_chunk["summary_to_dict_ns"] = int(ps_chunk.get("summary_to_dict_ns", 0)) + int(chunk_profile.get("summary_to_dict_ns", 0))
						ps_chunk["matchup_ns"] = int(ps_chunk.get("matchup_ns", 0)) + int(chunk_profile.get("matchup_ns", 0))
						ps_chunk["clear_ns"] = int(ps_chunk.get("clear_ns", 0)) + int(chunk_profile.get("clear_ns", 0))
						ps_chunk["match_count"] = int(ps_chunk.get("match_count", 0)) + int(chunk_profile.get("match_count", 0))
			var agg_start_ns: int = _now_ns()
			aggregator.consume_summary(team_size, entry)
			if do_profile:
				var ps_agg: Dictionary = profile_state
				ps_agg["aggregation_ns"] = int(ps_agg.get("aggregation_ns", 0)) + (_now_ns() - agg_start_ns)
	return OK
