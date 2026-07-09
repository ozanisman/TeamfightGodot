extends SceneTree

## Validation-only multi-seed policy candidate sweep.

const DraftValidationRunnerScript := preload("res://scripts/tools/draft_validation_runner.gd")
const DraftValidationAnalyzerCoreScript := preload("res://scripts/tools/draft_validation_analyzer_core.gd")
const DraftValidationCsvScript := preload("res://scripts/tools/draft_validation_csv.gd")
const DraftEloGateCoreScript := preload("res://scripts/tools/draft_elo_gate_core.gd")
const DraftPersonaRealismCoreScript := preload("res://scripts/tools/draft_persona_realism_core.gd")
const DraftPersonaGateCoreScript := preload("res://scripts/tools/draft_persona_gate_core.gd")

const DEFAULT_OUTPUT_DIR: String = "res://model_stats/draft_policy_candidate_sweep"
const DEFAULT_CONTROL: String = "native_softmax"
const DEFAULT_CANDIDATES: Array[String] = [
	"native_lookahead_softmax",
	"native_softmax_safe",
	"native_softmax_ceiling",
	"native_softmax_counter_heavy",
]
const DEFAULT_SEEDS: Array[int] = [1337, 9257, 17231]
const DEFAULT_TRIALS_PER_SEED: int = 20
const DEFAULT_SIMS_PER_DRAFT: int = 10
const SIDE_BIAS_MARGIN_PP: float = DraftPersonaGateCoreScript.DEFAULT_SIDE_BIAS_MARGIN_PP
const REALISM_MARGIN: float = DraftPersonaGateCoreScript.DEFAULT_REALISM_MARGIN
const P_VALUE_THRESHOLD: float = DraftPersonaGateCoreScript.DEFAULT_P_VALUE_THRESHOLD


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	await process_frame
	if _has_flag("--self-test"):
		var self_test: Dictionary = _run_self_test()
		var ok: bool = bool(self_test.get("pass", false))
		print("native_draft_policy_candidate_sweep self-test: %s" % ("PASS" if ok else "FAIL"))
		quit(0 if ok else 1)
		return

	var output_dir: String = _extract_argument("--output-dir=", DEFAULT_OUTPUT_DIR)
	var control: String = _extract_argument("--control=", DEFAULT_CONTROL)
	var candidates: Array[String] = _parse_string_list(_extract_csv_argument("--candidates=", ",".join(DEFAULT_CANDIDATES)))
	var seeds: Array[int] = _parse_seed_list(_extract_csv_argument("--seeds=", _join_ints(DEFAULT_SEEDS)))
	var trials_per_seed: int = maxi(1, int(_extract_argument("--trials-per-seed=", str(DEFAULT_TRIALS_PER_SEED))))
	var sims_per_draft: int = maxi(1, int(_extract_argument("--sims-per-draft=", str(DEFAULT_SIMS_PER_DRAFT))))

	if candidates.is_empty():
		push_error("native_draft_policy_candidate_sweep: --candidates cannot be empty")
		quit(1)
		return
	if seeds.is_empty():
		push_error("native_draft_policy_candidate_sweep: --seeds cannot be empty")
		quit(1)
		return
	if not _ensure_dir(output_dir):
		push_error("native_draft_policy_candidate_sweep: could not create %s" % output_dir)
		quit(1)
		return

	var result: Dictionary = _run_sweep(output_dir, control, candidates, seeds, trials_per_seed, sims_per_draft)
	if not bool(result.get("ok", false)):
		push_error("native_draft_policy_candidate_sweep: %s" % String(result.get("error", "failed")))
		quit(1)
		return
	print("native_draft_policy_candidate_sweep: wrote %s" % output_dir)
	print("native_draft_policy_candidate_sweep: %s" % String(result.get("status", "UNKNOWN")))
	quit(0)


func _run_sweep(
	output_dir: String,
	control: String,
	candidates: Array[String],
	seeds: Array[int],
	trials_per_seed: int,
	sims_per_draft: int
) -> Dictionary:
	var raw_strategies: Array[String] = [control]
	raw_strategies.append_array(candidates)
	var strategies: Array[String] = _dedupe(raw_strategies)
	var step_paths: Array[String] = []
	var draft_paths: Array[String] = []
	for seed_value in seeds:
		var seed_prefix: String = output_dir.path_join("seed_%d" % seed_value)
		var step_path: String = "%s_validation.csv" % seed_prefix
		var draft_path: String = "%s_drafts.csv" % seed_prefix
		var run_result: Dictionary = DraftValidationRunnerScript.run({
			"output_path": step_path,
			"draft_summary_output_path": draft_path,
			"trials": trials_per_seed,
			"sims_per_draft": sims_per_draft,
			"base_seed": seed_value,
			"blue_strategies": strategies,
			"red_strategies": strategies,
			"log_prefix": "native_draft_policy_candidate_sweep",
		})
		if not bool(run_result.get("ok", false)):
			return {"ok": false, "error": "seed %d failed: %s" % [seed_value, String(run_result.get("error", ""))]}
		step_paths.append(step_path)
		draft_paths.append(draft_path)

	var merged_steps_path: String = output_dir.path_join("policy_candidate_sweep_validation.csv")
	var merged_drafts_path: String = output_dir.path_join("policy_candidate_sweep_drafts.csv")
	if not _merge_csv_files(step_paths, merged_steps_path):
		return {"ok": false, "error": "failed to merge step CSVs"}
	if not _merge_csv_files(draft_paths, merged_drafts_path):
		return {"ok": false, "error": "failed to merge draft summary CSVs"}

	var summary_path: String = output_dir.path_join("policy_candidate_sweep_summary.csv")
	var ab_path: String = output_dir.path_join("policy_candidate_sweep_ab_report.csv")
	var analyzer_result: Dictionary = DraftValidationAnalyzerCoreScript.run({
		"draft_summary_path": merged_drafts_path,
		"output_path": summary_path,
		"ab_output_path": ab_path,
		"native_strategy_names": strategies,
	})
	if not bool(analyzer_result.get("ok", false)):
		return {"ok": false, "error": "analyzer failed: %s" % String(analyzer_result.get("error", ""))}

	var ladder_path: String = output_dir.path_join("policy_candidate_sweep_elo_ladder.csv")
	var empty_ordering: Array[Dictionary] = []
	var elo_result: Dictionary = DraftEloGateCoreScript.run_ladder_and_gate({
		"draft_summary_path": merged_drafts_path,
		"ladder_csv_path": ladder_path,
		"strategies": strategies,
		"ordering": empty_ordering,
		"min_gap": 0.0,
	})
	if not bool(elo_result.get("pass", false)):
		return {"ok": false, "error": "Elo ladder failed: %s" % "; ".join(Array(elo_result.get("errors", [])))}

	var realism_path: String = output_dir.path_join("policy_candidate_sweep_realism_metrics.csv")
	var realism_report_path: String = output_dir.path_join("policy_candidate_sweep_realism_report.md")
	var realism_result: Dictionary = DraftPersonaRealismCoreScript.run({
		"draft_summary_path": merged_drafts_path,
		"output_csv_path": realism_path,
		"output_report_path": realism_report_path,
	})
	if not bool(realism_result.get("ok", false)):
		return {"ok": false, "error": "realism failed: %s" % String(realism_result.get("error", ""))}

	var summary_rows: Array[Dictionary] = DraftValidationCsvScript.load_metric_rows(summary_path)
	var ab_rows: Array[Dictionary] = DraftValidationCsvScript.load_metric_rows(ab_path)
	var drafts: Array[Dictionary] = DraftValidationCsvScript.load_draft_summaries(merged_drafts_path)
	var ratings: Dictionary = elo_result.get("ratings", {})
	var realism_metrics: Dictionary = DraftPersonaRealismCoreScript.load_metrics_csv(realism_path)
	var gate_result: Dictionary = DraftPersonaGateCoreScript.evaluate_rows(summary_rows, ab_rows, ratings, [], {
		"control": control,
		"personas": candidates,
		"realism_metrics": realism_metrics,
		"elo_min_gap": 0.0,
		"side_bias_margin_pp": SIDE_BIAS_MARGIN_PP,
		"p_value_threshold": P_VALUE_THRESHOLD,
		"realism_margin": REALISM_MARGIN,
	})
	if not Array(gate_result.get("errors", [])).is_empty():
		return {"ok": false, "error": "candidate evaluation failed: %s" % "; ".join(Array(gate_result.get("errors", [])))}

	var candidate_rows: Array[Dictionary] = _evaluate_candidates(
		control, candidates, drafts, summary_rows, ratings, realism_metrics, gate_result
	)
	var overall_status: String = _overall_status(candidate_rows)
	var metrics: Dictionary = {
		"status": overall_status,
		"validation_only": true,
		"control": control,
		"candidates": candidates,
		"seeds": seeds,
		"trials_per_seed": trials_per_seed,
		"sims_per_draft": sims_per_draft,
		"paths": {
			"merged_steps": merged_steps_path,
			"merged_drafts": merged_drafts_path,
			"summary": summary_path,
			"ab_report": ab_path,
			"elo_ladder": ladder_path,
			"realism_metrics": realism_path,
			"realism_report": realism_report_path,
		},
		"candidate_results": candidate_rows,
	}
	var metrics_path: String = output_dir.path_join("policy_candidate_sweep_metrics.json")
	var report_path: String = output_dir.path_join("policy_candidate_sweep_report.md")
	if not _write_text(metrics_path, JSON.stringify(metrics, "\t") + "\n"):
		return {"ok": false, "error": "failed to write metrics JSON"}
	if not _write_text(report_path, _report_markdown(metrics)):
		return {"ok": false, "error": "failed to write report"}
	return {"ok": true, "status": overall_status, "metrics_path": metrics_path, "report_path": report_path}


func _evaluate_candidates(
	control: String,
	candidates: Array[String],
	drafts: Array[Dictionary],
	summary_rows: Array[Dictionary],
	ratings: Dictionary,
	realism_metrics: Dictionary,
	gate_result: Dictionary
) -> Array[Dictionary]:
	var decisions_by_candidate: Dictionary = {}
	for raw_decision in Array(gate_result.get("decisions", [])):
		var decision: Dictionary = raw_decision
		decisions_by_candidate[String(decision.get("persona", ""))] = decision

	var out: Array[Dictionary] = []
	for candidate in candidates:
		var decision: Dictionary = decisions_by_candidate.get(candidate, {})
		var vs_control: Dictionary = _side_balanced_vs_control(drafts, control, candidate)
		var realism_delta: Dictionary = _realism_delta(realism_metrics, control, candidate)
		var realism_status: String = String(decision.get("realism_status", "NOT_EVALUATED"))
		var strength_improved: bool = (
			float(decision.get("elo_gap", NAN)) > 0.0
			or float(vs_control.get("score_rate", 0.0)) > 0.5
			or float(decision.get("ab_delta", NAN)) > 0.0
		)
		var realism_improved: bool = realism_status == "PASS" and bool(realism_delta.get("improved", false))
		var promotion_ready: bool = _promotion_ready(decision)
		var status: String = "PROMOTION_CANDIDATE"
		if not promotion_ready:
			status = "FOLLOW_UP_ONLY" if strength_improved or realism_improved else "REJECTED"
		out.append({
			"candidate": candidate,
			"status": status,
			"reasons": _candidate_notes(decision, strength_improved, realism_improved),
			"elo": float(decision.get("elo", NAN)),
			"control_elo": float(decision.get("control_elo", NAN)),
			"elo_gap": float(decision.get("elo_gap", NAN)),
			"score_rate_vs_control": float(vs_control.get("score_rate", 0.0)),
			"decisive_winrate_vs_control": float(vs_control.get("decisive_winrate", 0.0)),
			"drafts_vs_control": int(vs_control.get("drafts", 0)),
			"matches_vs_control": int(vs_control.get("matches", 0)),
			"ab_delta": float(decision.get("ab_delta", NAN)),
			"ab_delta_ci_lower": float(decision.get("ab_delta_ci_lower", NAN)),
			"ab_delta_ci_upper": float(decision.get("ab_delta_ci_upper", NAN)),
			"ab_p_value": float(decision.get("ab_p_value", NAN)),
			"ab_status": String(decision.get("ab_status", "NOT_EVALUATED")),
			"self_play_bias_pp": float(decision.get("self_play_bias_pp", NAN)),
			"control_self_play_bias_pp": float(decision.get("control_self_play_bias_pp", NAN)),
			"side_bias_limit_pp": float(decision.get("side_bias_limit_pp", NAN)),
			"realism_status": realism_status,
			"realism_improved": realism_improved,
			"realism_delta": realism_delta,
			"latency_ms": "NOT_APPLICABLE",
			"latency_status": "NOT_APPLICABLE",
		})
	return out


func _promotion_ready(decision: Dictionary) -> bool:
	return (
		float(decision.get("elo_gap", NAN)) >= 0.0
		and float(decision.get("self_play_bias_pp", INF)) <= float(decision.get("side_bias_limit_pp", -INF)) + DraftPersonaGateCoreScript.COMPARISON_EPSILON
		and String(decision.get("ab_status", "NOT_EVALUATED")) != "REGRESSION"
		and String(decision.get("ab_status", "NOT_EVALUATED")) != "NOT_EVALUATED"
		and String(decision.get("realism_status", "NOT_EVALUATED")) == "PASS"
	)


func _candidate_notes(decision: Dictionary, strength_improved: bool, realism_improved: bool) -> Array[String]:
	var notes: Array[String] = []
	for reason in Array(decision.get("reasons", [])):
		notes.append(String(reason))
	for blocker in Array(decision.get("promotion_blockers", [])):
		notes.append(String(blocker))
	if strength_improved:
		notes.append("at least one strength metric improved")
	if realism_improved:
		notes.append("at least one realism metric improved")
	if notes.is_empty():
		notes.append("meets promotion rules")
	return notes


func _side_balanced_vs_control(drafts: Array[Dictionary], control: String, candidate: String) -> Dictionary:
	var candidate_wins: int = 0
	var control_wins: int = 0
	var draws: int = 0
	var draft_count: int = 0
	for draft in drafts:
		var blue: String = String(draft.get("blue_strategy", ""))
		var red: String = String(draft.get("red_strategy", ""))
		if blue == candidate and red == control:
			candidate_wins += int(draft.get("blue_wins", 0))
			control_wins += int(draft.get("red_wins", 0))
			draws += int(draft.get("draws", 0))
			draft_count += 1
		elif blue == control and red == candidate:
			candidate_wins += int(draft.get("red_wins", 0))
			control_wins += int(draft.get("blue_wins", 0))
			draws += int(draft.get("draws", 0))
			draft_count += 1
	var decisive: int = candidate_wins + control_wins
	var total: int = decisive + draws
	return {
		"drafts": draft_count,
		"matches": total,
		"candidate_wins": candidate_wins,
		"control_wins": control_wins,
		"draws": draws,
		"decisive_winrate": float(candidate_wins) / float(maxi(1, decisive)),
		"score_rate": (float(candidate_wins) + 0.5 * float(draws)) / float(maxi(1, total)),
	}


func _realism_delta(metrics: Dictionary, control: String, candidate: String) -> Dictionary:
	if not metrics.has(control) or not metrics.has(candidate):
		return {"improved": false}
	var control_row: Dictionary = metrics[control]
	var candidate_row: Dictionary = metrics[candidate]
	var deltas: Dictionary = {}
	var improved: bool = false
	for key in ["pick_entropy_norm", "ban_entropy_norm", "unique_pick_rate", "unique_ban_rate"]:
		var delta: float = _metric_value(candidate_row, key) - _metric_value(control_row, key)
		deltas[key] = delta
		if delta > 0.000001:
			improved = true
	for key in ["top_pick_concentration", "top_ban_concentration", "repeated_opener_rate"]:
		var delta: float = _metric_value(candidate_row, key) - _metric_value(control_row, key)
		deltas[key] = delta
		if delta < -0.000001:
			improved = true
	deltas["improved"] = improved
	return deltas


func _metric_value(row: Dictionary, key: String) -> float:
	var raw: Variant = row.get(key, 0.0)
	if raw is float or raw is int:
		return float(raw)
	var text: String = String(raw)
	if text.is_valid_float():
		return float(text)
	return 0.0


func _overall_status(candidate_rows: Array[Dictionary]) -> String:
	var has_promotion: bool = false
	var has_follow_up: bool = false
	for row in candidate_rows:
		var status: String = String(row.get("status", ""))
		has_promotion = has_promotion or status == "PROMOTION_CANDIDATE"
		has_follow_up = has_follow_up or status == "FOLLOW_UP_ONLY"
	if has_promotion:
		return "PROMOTION_CANDIDATE"
	if has_follow_up:
		return "FOLLOW_UP_ONLY"
	return "REJECTED"


func _report_markdown(metrics: Dictionary) -> String:
	var lines: Array[String] = []
	lines.append("# Native Draft Policy Candidate Sweep")
	lines.append("")
	lines.append("STATUS: %s" % String(metrics.get("status", "")))
	lines.append("Validation-only: no gameplay wiring, native policy change, stats promotion, or manifest update.")
	lines.append("")
	lines.append("Control: `%s`" % String(metrics.get("control", "")))
	lines.append("Seeds: `%s`" % ",".join(_variant_array_to_strings(metrics.get("seeds", []))))
	lines.append("Trials per seed: %d" % int(metrics.get("trials_per_seed", 0)))
	lines.append("Sims per draft: %d" % int(metrics.get("sims_per_draft", 0)))
	lines.append("")
	lines.append("## Candidate Decisions")
	lines.append("")
	lines.append("| Candidate | Status | Elo gap | Score vs control | Decisive winrate | A/B delta | A/B 95% CI | A/B p | Side bias | Realism | Latency | Notes |")
	lines.append("| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- | --- |")
	for row_var in Array(metrics.get("candidate_results", [])):
		var row: Dictionary = row_var
		lines.append("| `%s` | %s | %s | %.4f | %.4f | %s | %s | %s | %spp | %s | %s | %s |" % [
			String(row.get("candidate", "")),
			String(row.get("status", "")),
			_format_float(float(row.get("elo_gap", NAN)), 2),
			float(row.get("score_rate_vs_control", 0.0)),
			float(row.get("decisive_winrate_vs_control", 0.0)),
			_format_float(float(row.get("ab_delta", NAN)), 4),
			"[%s, %s]" % [
				_format_float(float(row.get("ab_delta_ci_lower", NAN)), 4),
				_format_float(float(row.get("ab_delta_ci_upper", NAN)), 4),
			],
			_format_float(float(row.get("ab_p_value", NAN)), 4),
			_format_float(float(row.get("self_play_bias_pp", NAN)), 2),
			String(row.get("realism_status", "")),
			String(row.get("latency_status", "")),
			"; ".join(Array(row.get("reasons", [])).map(func(v: Variant) -> String: return String(v))),
		])
	lines.append("")
	lines.append("## Artifacts")
	var paths: Dictionary = metrics.get("paths", {})
	for key in _sorted_keys(paths):
		lines.append("- %s: `%s`" % [key, String(paths[key])])
	lines.append("")
	return "\n".join(lines)


func _run_self_test() -> Dictionary:
	var control: String = DEFAULT_CONTROL
	var candidates: Array[String] = ["candidate_promote", "candidate_follow", "candidate_reject"]
	var ratings: Dictionary = {
		control: 1500.0,
		"candidate_promote": 1510.0,
		"candidate_follow": 1495.0,
		"candidate_reject": 1490.0,
	}
	var drafts: Array[Dictionary] = [
		_draft(control, control, 55, 45, 0),
		_draft("candidate_promote", "candidate_promote", 52, 48, 0),
		_draft("candidate_follow", "candidate_follow", 53, 47, 0),
		_draft("candidate_reject", "candidate_reject", 52, 48, 0),
		_draft("candidate_promote", control, 56, 44, 0),
		_draft(control, "candidate_promote", 45, 55, 0),
		_draft("candidate_follow", control, 54, 46, 0),
		_draft(control, "candidate_follow", 48, 52, 0),
		_draft("candidate_reject", control, 45, 55, 0),
		_draft(control, "candidate_reject", 56, 44, 0),
	]
	var summary_rows: Array[Dictionary] = _synthetic_summary_rows(drafts)
	var ab_rows: Array[Dictionary] = _synthetic_ab_rows(drafts)
	var realism_metrics: Dictionary = {
		control: _realism_row(0.80, 0.80, 0.70, 0.70, 0.18, 0.18, 0.10),
		"candidate_promote": _realism_row(0.81, 0.80, 0.70, 0.70, 0.18, 0.18, 0.10),
		"candidate_follow": _realism_row(0.82, 0.80, 0.70, 0.70, 0.18, 0.18, 0.10),
		"candidate_reject": _realism_row(0.70, 0.70, 0.60, 0.60, 0.30, 0.30, 0.20),
	}
	var gate_result: Dictionary = DraftPersonaGateCoreScript.evaluate_rows(summary_rows, ab_rows, ratings, [], {
		"control": control,
		"personas": candidates,
		"realism_metrics": realism_metrics,
	})
	var rows: Array[Dictionary] = _evaluate_candidates(control, candidates, drafts, summary_rows, ratings, realism_metrics, gate_result)
	var by_candidate: Dictionary = {}
	for row in rows:
		by_candidate[String(row["candidate"])] = row
	var cases: Array[Dictionary] = [
		{"name": "promotion candidate", "pass": String(by_candidate["candidate_promote"]["status"]) == "PROMOTION_CANDIDATE"},
		{"name": "follow-up candidate", "pass": String(by_candidate["candidate_follow"]["status"]) == "FOLLOW_UP_ONLY"},
		{"name": "rejected candidate", "pass": String(by_candidate["candidate_reject"]["status"]) == "REJECTED"},
		{"name": "overall status", "pass": _overall_status(rows) == "PROMOTION_CANDIDATE"},
	]
	var failed: Array[String] = []
	for case in cases:
		if not bool(case["pass"]):
			failed.append(String(case["name"]))
	return {"pass": failed.is_empty(), "failed": failed, "cases": cases}


func _synthetic_summary_rows(drafts: Array[Dictionary]) -> Array[Dictionary]:
	var rows: Array[Dictionary] = []
	for draft in drafts:
		rows.append({
			"metric": "matchup",
			"grouping": "blue=%s red=%s" % [String(draft["blue_strategy"]), String(draft["red_strategy"])],
			"wins": int(draft["blue_wins"]),
			"losses": int(draft["red_wins"]),
			"draws": int(draft["draws"]),
		})
	return rows


func _synthetic_ab_rows(drafts: Array[Dictionary]) -> Array[Dictionary]:
	var rows: Array[Dictionary] = []
	for draft in drafts:
		rows.append({
			"metric": "matchup_winrate_ci",
			"grouping": "blue=%s red=%s" % [String(draft["blue_strategy"]), String(draft["red_strategy"])],
			"wins": int(draft["blue_wins"]),
			"losses": int(draft["red_wins"]),
		})
	return rows


func _draft(blue: String, red: String, blue_wins: int, red_wins: int, draws: int) -> Dictionary:
	var total: int = blue_wins + red_wins + draws
	return {
		"draft_seed": 1,
		"blue_strategy": blue,
		"red_strategy": red,
		"blue_wins": blue_wins,
		"red_wins": red_wins,
		"draws": draws,
		"blue_winrate": float(blue_wins) / float(maxi(1, total)),
		"red_winrate": float(red_wins) / float(maxi(1, total)),
		"drawrate": float(draws) / float(maxi(1, total)),
	}


func _realism_row(pick_entropy: float, ban_entropy: float, unique_pick: float, unique_ban: float, top_pick: float, top_ban: float, repeated: float) -> Dictionary:
	return {
		"pick_entropy_norm": pick_entropy,
		"ban_entropy_norm": ban_entropy,
		"unique_pick_rate": unique_pick,
		"unique_ban_rate": unique_ban,
		"top_pick_concentration": top_pick,
		"top_ban_concentration": top_ban,
		"repeated_opener_rate": repeated,
	}


func _merge_csv_files(paths: Array[String], output_path: String) -> bool:
	var lines: Array[String] = []
	var wrote_header: bool = false
	for path in paths:
		var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
		if f == null:
			return false
		var header: String = f.get_line()
		if not wrote_header:
			lines.append(header)
			wrote_header = true
		while not f.eof_reached():
			var line: String = f.get_line()
			if not line.strip_edges().is_empty():
				lines.append(line)
		f.close()
	return _write_text(output_path, "\n".join(lines) + "\n")


func _ensure_dir(path: String) -> bool:
	var global_path: String = ProjectSettings.globalize_path(path)
	if DirAccess.dir_exists_absolute(global_path):
		return true
	return DirAccess.make_dir_recursive_absolute(global_path) == OK


func _write_text(path: String, text: String) -> bool:
	var global_path: String = ProjectSettings.globalize_path(path)
	var dir_path: String = global_path.get_base_dir()
	if not DirAccess.dir_exists_absolute(dir_path):
		DirAccess.make_dir_recursive_absolute(dir_path)
	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f == null:
		return false
	f.store_string(text)
	f.close()
	return true


func _has_flag(flag: String) -> bool:
	for a in OS.get_cmdline_user_args():
		if str(a) == flag:
			return true
	return false


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _extract_csv_argument(prefix: String, default_value: String) -> String:
	var args: Array = OS.get_cmdline_user_args()
	for i in range(args.size()):
		var arg: String = str(args[i])
		if not arg.begins_with(prefix):
			continue
		var parts: Array[String] = [arg.substr(prefix.length())]
		var j: int = i + 1
		while j < args.size():
			var next_arg: String = str(args[j])
			if next_arg.begins_with("--"):
				break
			parts.append(next_arg)
			j += 1
		return ",".join(parts)
	return default_value


func _parse_string_list(raw: String) -> Array[String]:
	var out: Array[String] = []
	for part in raw.split(","):
		var trimmed: String = part.strip_edges()
		if not trimmed.is_empty():
			out.append(trimmed)
	return _dedupe(out)


func _parse_seed_list(raw: String) -> Array[int]:
	var out: Array[int] = []
	for part in raw.split(","):
		var trimmed: String = part.strip_edges()
		if trimmed.is_valid_int():
			out.append(int(trimmed))
	return out


func _dedupe(values: Array[String]) -> Array[String]:
	var seen: Dictionary = {}
	var out: Array[String] = []
	for value in values:
		if not seen.has(value):
			seen[value] = true
			out.append(value)
	return out


func _join_ints(values: Array[int]) -> String:
	var parts: Array[String] = []
	for value in values:
		parts.append(str(value))
	return ",".join(parts)


func _variant_array_to_strings(values: Variant) -> Array[String]:
	var out: Array[String] = []
	for value in Array(values):
		out.append(str(value))
	return out


func _sorted_keys(dict: Dictionary) -> Array[String]:
	var keys: Array[String] = []
	for key in dict.keys():
		keys.append(String(key))
	keys.sort()
	return keys


func _format_float(value: float, digits: int) -> String:
	if is_nan(value):
		return "n/a"
	return "%.*f" % [digits, value]
