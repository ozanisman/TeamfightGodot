extends SceneTree

## Standalone certification gate for stats snapshot promotion.
##
## Usage:
##   godot --headless --path . --script res://scripts/tools/native_draft_stats_certification_gate.gd \
##     -- --candidate-dir=res://model_stats/stats_selfplay_candidate \
##        --baseline-stats-dir=res://model_stats/stats_output_100k

const DraftStatsCertificationGateCoreScript := preload("res://scripts/tools/draft_stats_certification_gate_core.gd")
const DraftAiConfigScript := preload("res://scripts/tools/draft_ai_config.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

var _candidate_dir: String = ""
var _baseline_dir: String = ""
var _output_path: String = "res://logs/native_draft_stats_certification_gate_report.md"
var _prior_gates_pass: bool = true
var _run_pick_smoke: bool = true


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


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	await process_frame

	_candidate_dir = _extract_argument("--candidate-dir=", "")
	_baseline_dir = _extract_argument("--baseline-stats-dir=", "res://model_stats/stats_output_100k")
	_output_path = _extract_argument("--output=", "res://logs/native_draft_stats_certification_gate_report.md")
	_prior_gates_pass = not _flag_enabled("--prior-gates-failed")
	_run_pick_smoke = not _flag_enabled("--skip-pick-smoke")

	if _candidate_dir.is_empty():
		push_error("native_draft_stats_certification_gate: --candidate-dir required")
		await _finish(false, ["--candidate-dir required"], {})
		return

	var result: Dictionary = DraftStatsCertificationGateCoreScript.evaluate({
		"candidate_dir": _candidate_dir,
		"baseline_dir": _baseline_dir,
		"prior_gates_pass": _prior_gates_pass,
		"run_pick_smoke": _run_pick_smoke,
		"config_path": _extract_argument("--config-path=", DraftAiConfigScript.DEFAULT_CONFIG_PATH),
		"pick_smoke_states": maxi(1, int(_extract_argument("--pick-smoke-states=", "10"))),
		"pick_regret_ceiling": float(_extract_argument("--pick-regret-ceiling=", str(DraftStatsCertificationGateCoreScript.DEFAULT_PICK_REGRET_CEILING))),
	})

	await _finish(bool(result.get("pass", false)), [], result)


func _finish(overall_pass: bool, errors: Array[String], result: Dictionary) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Stats Certification Gate Report")
	lines.append("")
	lines.append("Generated: " + Time.get_datetime_string_from_system())
	lines.append("")
	lines.append("Candidate: `%s`" % _candidate_dir)
	lines.append("Baseline: `%s`" % _baseline_dir)
	lines.append("Prior gates pass: %s" % _prior_gates_pass)
	lines.append("")
	if not errors.is_empty():
		lines.append("## Errors")
		for err in errors:
			lines.append("- " + err)
		lines.append("")
	lines.append("## Checks")
	for check in result.get("checks", []):
		var status: String = "PASS" if check.get("pass", false) else "FAIL"
		lines.append("- %s: %s (%s)" % [check.get("name", "?"), status, check.get("detail", "")])
	lines.append("")
	lines.append("## Overall")
	lines.append("STATUS: %s" % ("PASS" if overall_pass else "FAIL"))

	var f := FileAccess.open(ProjectSettings.globalize_path(_output_path), FileAccess.WRITE)
	if f != null:
		f.store_string("\n".join(lines) + "\n")
		f.close()
		print("native_draft_stats_certification_gate: wrote %s" % _output_path)

	print("native_draft_stats_certification_gate: %s" % ("PASS" if overall_pass else "FAIL"))
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0 if overall_pass else 1)
