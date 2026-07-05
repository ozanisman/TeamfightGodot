extends SceneTree

const DraftPersonaGateCoreScript := preload("res://scripts/tools/draft_persona_gate_core.gd")

const DEFAULT_OUTPUT_PATH: String = "res://logs/native_draft_persona_gate_report.md"
const SELF_TEST_OUTPUT_PATH: String = "res://logs/native_draft_persona_gate_self_test_report.md"


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	await process_frame
	if _has_flag("--self-test"):
		var self_test: Dictionary = DraftPersonaGateCoreScript.self_test()
		_write_self_test_report(self_test)
		var self_test_pass: bool = bool(self_test.get("pass", false))
		print("native_draft_persona_gate self-test: %s" % ("PASS" if self_test_pass else "FAIL"))
		quit(0 if self_test_pass else 1)
		return

	var params: Dictionary = {
		"summary_path": _extract_argument("--summary=", ""),
		"ab_report_path": _extract_argument("--ab-report=", ""),
		"elo_ladder_path": _extract_argument("--elo-ladder=", ""),
		"draft_summary_path": _extract_argument("--draft-summary=", ""),
		"realism_metrics_path": _extract_argument("--realism-metrics=", ""),
		"control": _extract_argument("--control=", DraftPersonaGateCoreScript.CONTROL_STRATEGY),
		"personas": _extract_argument("--personas=", ",".join(DraftPersonaGateCoreScript.DEFAULT_PERSONAS)),
		"elo_min_gap": _extract_float("--elo-min-gap=", DraftPersonaGateCoreScript.DEFAULT_ELO_MIN_GAP),
		"side_bias_margin_pp": _extract_float("--side-bias-margin-pp=", DraftPersonaGateCoreScript.DEFAULT_SIDE_BIAS_MARGIN_PP),
		"p_value_threshold": _extract_float("--p-threshold=", DraftPersonaGateCoreScript.DEFAULT_P_VALUE_THRESHOLD),
		"realism_margin": _extract_float("--realism-margin=", DraftPersonaGateCoreScript.DEFAULT_REALISM_MARGIN),
	}
	var output_path: String = _extract_argument("--output=", DEFAULT_OUTPUT_PATH)
	var result: Dictionary = DraftPersonaGateCoreScript.evaluate_from_paths(params)
	_write_report(result, params, output_path)
	print("native_draft_persona_gate: %s" % ("PASS" if bool(result.get("pass", false)) else "FAIL"))
	quit(0 if bool(result.get("pass", false)) else 1)


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


func _extract_float(prefix: String, default_value: float) -> float:
	var raw: String = _extract_argument(prefix, "")
	if raw.is_valid_float():
		return float(raw)
	return default_value


func _write_report(result: Dictionary, params: Dictionary, output_path: String) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Persona Gate Report")
	lines.append("")
	lines.append("STATUS: %s" % ("PASS" if bool(result.get("pass", false)) else "FAIL"))
	lines.append("Generated: %s" % Time.get_datetime_string_from_system())
	lines.append("Summary: `%s`" % String(params.get("summary_path", "")))
	lines.append("A/B report: `%s`" % String(params.get("ab_report_path", "")))
	lines.append("Elo ladder: `%s`" % String(params.get("elo_ladder_path", "")))
	lines.append("Draft summary: `%s`" % String(params.get("draft_summary_path", "")))
	lines.append("Realism metrics: `%s`" % String(params.get("realism_metrics_path", "")))
	lines.append("")

	var errors: Array = result.get("errors", [])
	if not errors.is_empty():
		lines.append("## Errors")
		for err in errors:
			lines.append("- %s" % String(err))
		lines.append("")

	lines.append("## Persona Decisions")
	lines.append("")
	lines.append("| Persona | Decision | Elo Gap | Side Bias | A/B | Realism | Notes |")
	lines.append("|---------|----------|---------|-----------|-----|---------|-------|")
	for decision in Array(result.get("decisions", [])):
		var reasons: Array = decision.get("reasons", [])
		var blockers: Array = decision.get("promotion_blockers", [])
		var notes_parts: Array[String] = []
		for reason in reasons:
			notes_parts.append(String(reason))
		for blocker in blockers:
			notes_parts.append(String(blocker))
		var notes: String = "ok" if notes_parts.is_empty() else "; ".join(notes_parts)
		lines.append("| `%s` | %s | %.2f | %.2fpp (limit %.2fpp) | %s delta=%s p=%s | REALISM: %s | %s |" % [
			String(decision.get("persona", "")),
			String(decision.get("status", "")),
			float(decision.get("elo_gap", NAN)),
			float(decision.get("self_play_bias_pp", NAN)),
			float(decision.get("side_bias_limit_pp", NAN)),
			String(decision.get("ab_status", "")),
			_format_float(float(decision.get("ab_delta", NAN))),
			_format_float(float(decision.get("ab_p_value", NAN))),
			String(decision.get("realism_status", "")),
			notes,
		])
	lines.append("")
	lines.append("Control: `%s`" % String(result.get("control", "")))
	lines.append("Control self-play side-bias: %spp" % _format_float(float(result.get("control_self_play_bias_pp", NAN))))
	lines.append("Promotion rules: Elo gap >= %.2f; side-bias <= control + %.2fpp; no significant side-balanced direct A/B regression at p <= %.3f; realism metrics within %.2fpp of control." % [
		float(params.get("elo_min_gap", DraftPersonaGateCoreScript.DEFAULT_ELO_MIN_GAP)),
		float(params.get("side_bias_margin_pp", DraftPersonaGateCoreScript.DEFAULT_SIDE_BIAS_MARGIN_PP)),
		float(params.get("p_value_threshold", DraftPersonaGateCoreScript.DEFAULT_P_VALUE_THRESHOLD)),
		float(params.get("realism_margin", DraftPersonaGateCoreScript.DEFAULT_REALISM_MARGIN)) * 100.0,
	])

	_write_lines(output_path, lines)


func _write_self_test_report(result: Dictionary) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Persona Gate Self-Test")
	lines.append("")
	lines.append("STATUS: %s" % ("PASS" if bool(result.get("pass", false)) else "FAIL"))
	for case in Array(result.get("cases", [])):
		var case_dict: Dictionary = case
		lines.append("- %s: %s" % [
			String(case_dict.get("name", "")),
			"PASS" if bool(case_dict.get("pass", false)) else "FAIL",
		])
	_write_lines(SELF_TEST_OUTPUT_PATH, lines)


func _write_lines(output_path: String, lines: Array[String]) -> void:
	var global_path: String = ProjectSettings.globalize_path(output_path)
	var dir_path: String = global_path.get_base_dir()
	if not DirAccess.dir_exists_absolute(dir_path):
		DirAccess.make_dir_recursive_absolute(dir_path)
	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f == null:
		push_error("native_draft_persona_gate: could not open report %s" % output_path)
		return
	f.store_string("\n".join(lines) + "\n")
	f.close()


func _format_float(value: float) -> String:
	if is_nan(value):
		return "n/a"
	return "%.4f" % value
