extends SceneTree

## Full-chain stats regeneration + certification pipeline (Workstream D.2).
##
## Usage (smoke defaults):
##   godot --headless --path . --script res://scripts/tools/native_draft_stats_certification.gd \
##     -- --drafts=50 --sims-per-draft=1 \
##        --blue-strategies=native_softmax --red-strategies=native_softmax \
##        --baseline-stats-dir=res://model_stats/stats_output_100k \
##        --output-dir=res://model_stats/stats_selfplay_candidate \
##        --trials=2 --harness-sims-per-draft=25 --min-matches=50
##
## Optional promotion (explicit only):
##     --promote
##
## Calibration:
##     --smoke       force relaxed thresholds (also default when --trials <= 2)
##     --production  force production thresholds even with low --trials

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const DraftSelfPlayStatsRunnerScript := preload("res://scripts/tools/draft_self_play_stats_runner.gd")
const DraftStatsSnapshotChecksScript := preload("res://scripts/tools/draft_stats_snapshot_checks.gd")
const DraftValidationRunnerScript := preload("res://scripts/tools/draft_validation_runner.gd")
const DraftValidationAnalyzerCoreScript := preload("res://scripts/tools/draft_validation_analyzer_core.gd")
const DraftQuantitativeGateCoreScript := preload("res://scripts/tools/draft_quantitative_gate_core.gd")
const DraftEloGateCoreScript := preload("res://scripts/tools/draft_elo_gate_core.gd")
const DraftStatsCertificationGateCoreScript := preload("res://scripts/tools/draft_stats_certification_gate_core.gd")
const DraftStatsPromotionScript := preload("res://scripts/tools/draft_stats_promotion.gd")
const DraftAiConfigScript := preload("res://scripts/tools/draft_ai_config.gd")
const StatsManifestScript := preload("res://scripts/tools/stats_manifest.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

const DEFAULT_BASELINE_STATS_DIR: String = "res://model_stats/stats_output_100k"
const DEFAULT_OUTPUT_DIR: String = "res://model_stats/stats_selfplay_candidate"
const DEFAULT_REPORT_PATH: String = "res://logs/native_draft_stats_certification_report.md"

const DEFAULT_HARNESS_STRATEGIES: Array[String] = [
	"native_full", "native_softmax", "random", "base_power_only"
]

const SMOKE_TRIALS_THRESHOLD: int = 2
const SMOKE_ELO_MIN_GAP: float = 5.0

var _report_lines: Array[String] = []
var _phase_results: Array[Dictionary] = []
var _exit_code: int = 0


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

	var baseline_stats_dir: String = _extract_argument("--baseline-stats-dir=", DEFAULT_BASELINE_STATS_DIR)
	var output_dir: String = _extract_argument("--output-dir=", DEFAULT_OUTPUT_DIR)
	var report_path: String = _extract_argument("--report=", DEFAULT_REPORT_PATH)
	var config_path: String = _extract_argument("--config-path=", DraftAiConfigScript.DEFAULT_CONFIG_PATH)
	var promote: bool = _flag_enabled("--promote")
	var production_mode: bool = _flag_enabled("--production")

	var drafts: int = maxi(1, int(_extract_argument("--drafts=", "50")))
	var sims_per_draft: int = maxi(1, int(_extract_argument("--sims-per-draft=", "1")))
	var base_seed: int = int(_extract_argument("--base-seed=", "500000"))
	var min_matches: int = maxi(1, int(_extract_argument("--min-matches=", "50")))
	var trials: int = maxi(1, int(_extract_argument("--trials=", "2")))
	var harness_sims_per_draft: int = maxi(1, int(_extract_argument("--harness-sims-per-draft=", "25")))
	var harness_base_seed: int = int(_extract_argument("--harness-base-seed=", "100000"))

	var gen_blue: Array[String] = _parse_strategy_names(_extract_argument("--blue-strategies=", "native_softmax"))
	var gen_red: Array[String] = _parse_strategy_names(_extract_argument("--red-strategies=", "native_softmax"))
	var harness_strategies: Array[String] = _parse_strategy_names(
		_extract_argument("--harness-strategies=", ",".join(DEFAULT_HARNESS_STRATEGIES))
	)

	var smoke_mode: bool = _flag_enabled("--smoke") or (trials <= SMOKE_TRIALS_THRESHOLD and not production_mode)
	var calibration_mode: String = "smoke (relaxed thresholds)" if smoke_mode else "production"
	var elo_min_gap: float = SMOKE_ELO_MIN_GAP if smoke_mode else DraftEloGateCoreScript.DEFAULT_MIN_GAP

	_report_lines.append("# Native Draft Stats Certification Report")
	_report_lines.append("")
	_report_lines.append("Generated: " + Time.get_datetime_string_from_system())
	_report_lines.append("")
	_report_lines.append("Baseline stats dir: `%s`" % baseline_stats_dir)
	_report_lines.append("Candidate output dir: `%s`" % output_dir)
	_report_lines.append("Promote: %s" % ("yes" if promote else "no (default)"))
	_report_lines.append("Calibration mode: %s" % calibration_mode)
	_report_lines.append("")

	print("native_draft_stats_certification: baseline=%s candidate=%s drafts=%d trials=%d mode=%s" % [
		baseline_stats_dir, output_dir, drafts, trials, calibration_mode
	])

	# Phase 1: generate
	var gen_result: Dictionary = DraftSelfPlayStatsRunnerScript.run({
		"stats_dir": baseline_stats_dir,
		"output_dir": output_dir,
		"drafts": drafts,
		"sims_per_draft": sims_per_draft,
		"base_seed": base_seed,
		"blue_strategies": gen_blue,
		"red_strategies": gen_red,
		"generator_args": OS.get_cmdline_user_args(),
		"log_prefix": "native_draft_stats_certification/generate",
	})
	if not _record_phase("generate", gen_result.get("ok", false), gen_result):
		await _finish(report_path)
		return
	var candidate_snapshot_id: String = String(gen_result.get("snapshot_id", ""))

	# Phase 2: structural gate
	var struct_checks: Array[Dictionary] = DraftStatsSnapshotChecksScript.run_checks(output_dir, min_matches)
	var struct_pass: bool = DraftStatsSnapshotChecksScript.overall_pass(struct_checks)
	if not _record_phase("structural_gate", struct_pass, {"checks": struct_checks, "snapshot_id": candidate_snapshot_id}):
		await _finish(report_path)
		return

	# Namespaced validation artifact paths
	var artifact_prefix: String = "res://model_stats/cert_%s" % candidate_snapshot_id
	var harness_steps_path: String = "%s_validation.csv" % artifact_prefix
	var harness_drafts_path: String = "%s_validation_drafts.csv" % artifact_prefix
	var summary_path: String = "%s_validation_summary.csv" % artifact_prefix
	var ab_report_path: String = "%s_validation_ab_report.csv" % artifact_prefix
	var ladder_csv_path: String = "%s_elo_ladder.csv" % artifact_prefix

	# Phase 3: harness on candidate stats
	var harness_result: Dictionary = DraftValidationRunnerScript.run({
		"stats_dir": output_dir,
		"output_path": harness_steps_path,
		"draft_summary_output_path": harness_drafts_path,
		"trials": trials,
		"sims_per_draft": harness_sims_per_draft,
		"base_seed": harness_base_seed,
		"blue_strategies": harness_strategies,
		"red_strategies": harness_strategies,
		"log_prefix": "native_draft_stats_certification/harness",
	})
	if not _record_phase("draft_validation_harness", harness_result.get("ok", false), harness_result):
		await _finish(report_path)
		return

	# Phase 4: analyzer
	var analyzer_result: Dictionary = DraftValidationAnalyzerCoreScript.run({
		"draft_summary_path": harness_drafts_path,
		"output_path": summary_path,
		"ab_output_path": ab_report_path,
		"native_strategy_names": ["native_full", "native_softmax"],
	})
	if not _record_phase("analyzer", analyzer_result.get("ok", false), analyzer_result):
		await _finish(report_path)
		return

	# Phase 5: quantitative gate
	var quant_thresholds: Dictionary = {}
	if smoke_mode:
		quant_thresholds = DraftQuantitativeGateCoreScript.smoke_thresholds()
	var quant_result: Dictionary = DraftQuantitativeGateCoreScript.evaluate_from_paths(
		summary_path, ab_report_path, quant_thresholds
	)
	quant_result["calibration_mode"] = calibration_mode
	if not _record_phase("quantitative_gate", quant_result.get("pass", false), quant_result):
		await _finish(report_path)
		return

	# Phase 6: elo ladder + gate
	var elo_result: Dictionary = DraftEloGateCoreScript.run_ladder_and_gate({
		"draft_summary_path": harness_drafts_path,
		"ladder_csv_path": ladder_csv_path,
		"strategies": harness_strategies,
		"require_fresh_ladder": true,
		"min_gap": elo_min_gap,
	})
	elo_result["elo_min_gap"] = elo_min_gap
	if not _record_phase("elo_gate", elo_result.get("pass", false), elo_result):
		await _finish(report_path)
		return

	# Phase 7: certification gate
	var prior_pass: bool = true
	for phase in _phase_results:
		if phase["name"] != "certification_gate" and not phase["pass"]:
			prior_pass = false
			break
	var cert_result: Dictionary = DraftStatsCertificationGateCoreScript.evaluate({
		"candidate_dir": output_dir,
		"baseline_dir": baseline_stats_dir,
		"prior_gates_pass": prior_pass,
		"config_path": config_path,
	})
	if not _record_phase("certification_gate", cert_result.get("pass", false), cert_result):
		await _finish(report_path)
		return

	# Phase 8: optional promote
	if promote:
		var promote_result: Dictionary = DraftStatsPromotionScript.promote(output_dir, baseline_stats_dir, config_path)
		_record_phase("promote", promote_result.get("ok", false), promote_result)
	else:
		_phase_results.append({
			"name": "promote",
			"pass": true,
			"detail": "skipped (no --promote flag)",
		})

	await _finish(report_path)


func _record_phase(phase_name: String, passed: bool, detail: Dictionary) -> bool:
	_phase_results.append({
		"name": phase_name,
		"pass": passed,
		"detail": detail,
	})
	_report_lines.append("## Phase: %s" % phase_name)
	_report_lines.append("Result: %s" % ("PASS" if passed else "FAIL"))
	if detail.has("snapshot_id") and not String(detail["snapshot_id"]).is_empty():
		_report_lines.append("Snapshot ID: %s" % detail["snapshot_id"])
	if detail.has("error") and not String(detail["error"]).is_empty():
		_report_lines.append("Error: %s" % detail["error"])
	if detail.has("calibration_mode"):
		_report_lines.append("Calibration mode: %s" % detail["calibration_mode"])
	if detail.has("elo_min_gap"):
		_report_lines.append("Elo min gap: %.1f" % float(detail["elo_min_gap"]))
	if detail.has("checks"):
		for check in detail["checks"]:
			var status: String = "PASS" if check.get("pass", false) else "FAIL"
			var check_detail: String = str(check.get("detail", check.get("reason", "")))
			var check_name: String = _format_check_name(check)
			_report_lines.append("- %s: %s (%s)" % [check_name, status, check_detail])
	_report_lines.append("")

	if not passed:
		_exit_code = 1
		push_error("native_draft_stats_certification: phase %s FAILED" % phase_name)
		return false

	print("native_draft_stats_certification: phase %s PASS" % phase_name)
	return true


func _finish(report_path: String) -> void:
	var overall_pass: bool = _exit_code == 0
	_report_lines.append("## Overall")
	_report_lines.append("Candidate snapshot: %s" % _phase_snapshot_id())
	_report_lines.append("STATUS: %s" % ("PASS" if overall_pass else "FAIL"))

	var f := FileAccess.open(ProjectSettings.globalize_path(report_path), FileAccess.WRITE)
	if f == null:
		push_error("native_draft_stats_certification: could not write %s" % report_path)
	else:
		f.store_string("\n".join(_report_lines) + "\n")
		f.close()
		print("native_draft_stats_certification: wrote %s" % report_path)

	print("native_draft_stats_certification: %s" % ("PASS" if overall_pass else "FAIL"))
	await HeadlessShutdownScript.teardown_extension_then_quit(self, _exit_code)


func _phase_snapshot_id() -> String:
	for phase in _phase_results:
		var detail: Dictionary = phase.get("detail", {})
		if detail.has("snapshot_id") and not String(detail["snapshot_id"]).is_empty():
			return String(detail["snapshot_id"])
		if detail.has("candidate_snapshot_id") and not String(detail["candidate_snapshot_id"]).is_empty():
			return String(detail["candidate_snapshot_id"])
	var manifest: Dictionary = StatsManifestScript.read_manifest(
		_extract_argument("--output-dir=", DEFAULT_OUTPUT_DIR)
	)
	return String(manifest.get("snapshot_id", ""))


func _format_check_name(check: Dictionary) -> String:
	if check.has("name") and not String(check["name"]).is_empty():
		return String(check["name"])
	if check.has("higher") and check.has("lower"):
		return "%s > %s" % [check["higher"], check["lower"]]
	return "?"
