extends SceneTree

const DraftPersonaRealismCoreScript := preload("res://scripts/tools/draft_persona_realism_core.gd")

const SELF_TEST_OUTPUT_PATH: String = "res://logs/native_draft_persona_realism_metrics_self_test_report.md"


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	await process_frame
	if _has_flag("--self-test"):
		var self_test: Dictionary = DraftPersonaRealismCoreScript.self_test()
		var report_ok: bool = _write_self_test_report(self_test)
		var ok: bool = bool(self_test.get("pass", false)) and report_ok
		print("native_draft_persona_realism_metrics self-test: %s" % ("PASS" if ok else "FAIL"))
		quit(0 if ok else 1)
		return

	var result: Dictionary = DraftPersonaRealismCoreScript.run({
		"draft_summary_path": _extract_argument("--draft-summary=", ""),
		"output_csv_path": _extract_argument("--output=", DraftPersonaRealismCoreScript.DEFAULT_OUTPUT_CSV),
		"output_report_path": _extract_argument("--report=", DraftPersonaRealismCoreScript.DEFAULT_OUTPUT_REPORT),
	})
	if not bool(result.get("ok", false)):
		push_error("native_draft_persona_realism_metrics: %s" % String(result.get("error", "failed")))
		quit(1)
		return

	print("native_draft_persona_realism_metrics: wrote %s and %s" % [
		String(result.get("output_csv_path", "")),
		String(result.get("output_report_path", "")),
	])
	quit(0)


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


func _write_self_test_report(result: Dictionary) -> bool:
	var lines: Array[String] = []
	lines.append("# Native Draft Persona Realism Metrics Self-Test")
	lines.append("")
	lines.append("STATUS: %s" % ("PASS" if bool(result.get("pass", false)) else "FAIL"))
	for case in Array(result.get("cases", [])):
		var case_dict: Dictionary = case
		lines.append("- %s: %s" % [
			String(case_dict.get("name", "")),
			"PASS" if bool(case_dict.get("pass", false)) else "FAIL",
		])
	var global_path: String = ProjectSettings.globalize_path(SELF_TEST_OUTPUT_PATH)
	var dir_path: String = global_path.get_base_dir()
	if not DirAccess.dir_exists_absolute(dir_path):
		DirAccess.make_dir_recursive_absolute(dir_path)
	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f == null:
		push_error("native_draft_persona_realism_metrics: could not open self-test report %s" % SELF_TEST_OUTPUT_PATH)
		return false
	f.store_string("\n".join(lines) + "\n")
	f.close()
	return true
