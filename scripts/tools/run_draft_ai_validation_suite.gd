extends SceneTree

## Draft AI Validation Suite
## Runs all available validation checks for native draft AI
## Skips optional checks if scripts are not present

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

var _report_lines := []
var _overall_pass := true

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	print("=== Draft AI Validation Suite ===")
	_report_lines.append("# Draft AI Validation Suite Report")
	_report_lines.append("")
	_report_lines.append("Generated: " + Time.get_datetime_string_from_system())
	_report_lines.append("")

	# Check backend availability
	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		_report_lines.append("## Backend Status")
		_report_lines.append("Native backend: UNAVAILABLE")
		_report_lines.append("")
		_report_lines.append("OVERALL: FAIL - Native backend unavailable")
		_write_report()
		quit(1)
		return

	_report_lines.append("## Backend Status")
	_report_lines.append("Native backend: AVAILABLE")
	_report_lines.append("")

	# Run required checks (check existing report files)
	_run_check("Full Draft Validation (native)", "full_draft_validation.gd", "full_draft_validation_report_native.txt")
	_run_check("Native Draft Quantitative Gate", "native_draft_quantitative_gate.gd", "native_draft_quantitative_gate_report.md")
	_run_elo_checks()
	_run_tier_checks()
	_run_self_play_stats_checks()
	_run_lookahead_gate_checks()
	_run_stats_certification_checks()
	_run_check("Native Recommendation Explanations Audit", "audit_native_recommendation_explanations.gd", "native_recommendation_explanations_audit_report.md")

	# Run optional checks if available
	_check_optional_report_file("Native Ban Quality Audit", "audit_native_ban_quality.gd", "native_ban_quality_audit_report.md")

	# Overall result
	_report_lines.append("")
	_report_lines.append("## Overall Result")
	if _overall_pass:
		_report_lines.append("OVERALL: PASS")
		print("=== OVERALL: PASS ===")
	else:
		_report_lines.append("OVERALL: FAIL")
		print("=== OVERALL: FAIL ===")

	_write_report()
	quit(0 if _overall_pass else 1)

func _run_check(check_name: String, script_name: String, report_file: String) -> void:
	print("\nChecking: " + check_name)
	_report_lines.append("## " + check_name)
	_report_lines.append("Script: " + script_name)
	_report_lines.append("Report file: " + report_file)

	# Read the report file to get status
	var report_path = "res://logs/" + report_file
	if not FileAccess.file_exists(ProjectSettings.globalize_path(report_path)):
		_report_lines.append("Result: FAIL - Report file not found")
		_report_lines.append("Note: Run the individual validation script to generate this report")
		_report_lines.append("Command: C:\\Godot\\godot.exe --headless --path . --script res://scripts/tools/" + script_name)
		print("  FAIL - Report file not found")
		_overall_pass = false
		_report_lines.append("")
		return

	var f = FileAccess.open(ProjectSettings.globalize_path(report_path), FileAccess.READ)
	if not f:
		_report_lines.append("Result: FAIL - Could not read report file")
		print("  FAIL - Could not read report file")
		_overall_pass = false
		_report_lines.append("")
		return

	var content := f.get_as_text()
	f.close()

	# Parse STATUS line
	var status := "UNKNOWN"
	for line in content.split("\n"):
		if line.begins_with("STATUS:"):
			status = line.substr(7).strip_edges()
			break

	if status == "PASS" and not _contains_terminal_marker(content):
		_report_lines.append("Result: PASS")
		print("  PASS")
	else:
		_report_lines.append("Result: FAIL")
		_report_lines.append("Status from report: " + status)
		print("  FAIL - Status: " + status)
		_overall_pass = false

	_report_lines.append("")


func _run_elo_checks() -> void:
	print("\nChecking: Native Draft Elo Ladder + Gate")
	_report_lines.append("## Native Draft Elo Ladder + Gate")
	_report_lines.append("Ladder CSV: model_stats/native_draft_elo_ladder.csv")
	_report_lines.append("Ladder report: logs/native_draft_elo_ladder_report.md")
	_report_lines.append("Gate report: logs/native_draft_elo_gate_report.md")

	var ladder_csv := "res://model_stats/native_draft_elo_ladder.csv"
	var ladder_report := "res://logs/native_draft_elo_ladder_report.md"
	var drafts_csv := "res://model_stats/native_draft_validation_drafts.csv"
	var gate_report := "res://logs/native_draft_elo_gate_report.md"

	var elo_pass := true

	if not FileAccess.file_exists(ProjectSettings.globalize_path(ladder_csv)):
		_report_lines.append("Result: FAIL - Ladder CSV not found (run native_draft_elo_ladder.gd)")
		print("  FAIL - Ladder CSV not found")
		elo_pass = false
	elif not _check_report_status(ladder_report, "OK"):
		_report_lines.append("Result: FAIL - Ladder report missing or not STATUS: OK")
		print("  FAIL - Ladder report")
		elo_pass = false

	if FileAccess.file_exists(ProjectSettings.globalize_path(drafts_csv)) and FileAccess.file_exists(ProjectSettings.globalize_path(ladder_csv)):
		var ladder_mtime: int = FileAccess.get_modified_time(ProjectSettings.globalize_path(ladder_csv))
		var drafts_mtime: int = FileAccess.get_modified_time(ProjectSettings.globalize_path(drafts_csv))
		if ladder_mtime < drafts_mtime:
			_report_lines.append("Result: FAIL - Ladder CSV older than draft summary (re-run ladder)")
			print("  FAIL - Stale ladder CSV")
			elo_pass = false

	if not _check_report_status(gate_report, "PASS"):
		_report_lines.append("Result: FAIL - Elo gate report missing or not STATUS: PASS")
		print("  FAIL - Elo gate report")
		elo_pass = false

	if elo_pass:
		_report_lines.append("Result: PASS")
		print("  PASS")
	else:
		_overall_pass = false

	_report_lines.append("")


func _run_tier_checks() -> void:
	print("\nChecking: Native Draft Tier Calibration + Gate")
	_report_lines.append("## Native Draft Tier Calibration + Gate")
	_report_lines.append("Tier summary: model_stats/native_draft_tier_calibration_summary.csv")
	_report_lines.append("Gate report: logs/native_draft_tier_gate_report.md")

	var tier_summary := "res://model_stats/native_draft_tier_calibration_summary.csv"
	var tier_drafts := "res://model_stats/native_draft_tier_calibration_drafts.csv"
	var gate_report := "res://logs/native_draft_tier_gate_report.md"

	var tier_pass := true

	if not FileAccess.file_exists(ProjectSettings.globalize_path(tier_summary)):
		_report_lines.append("Result: FAIL - Tier summary CSV not found (run tier calibration harness)")
		print("  FAIL - Tier summary CSV not found")
		tier_pass = false
	elif FileAccess.file_exists(ProjectSettings.globalize_path(tier_drafts)):
		var summary_mtime: int = FileAccess.get_modified_time(ProjectSettings.globalize_path(tier_summary))
		var drafts_mtime: int = FileAccess.get_modified_time(ProjectSettings.globalize_path(tier_drafts))
		if summary_mtime < drafts_mtime:
			_report_lines.append("Result: FAIL - Tier summary older than draft summary (re-run analyzer)")
			print("  FAIL - Stale tier summary CSV")
			tier_pass = false

	if not _check_report_status(gate_report, "PASS"):
		_report_lines.append("Result: FAIL - Tier gate report missing or not STATUS: PASS")
		print("  FAIL - Tier gate report")
		tier_pass = false

	if tier_pass:
		_report_lines.append("Result: PASS")
		print("  PASS")
	else:
		_overall_pass = false

	_report_lines.append("")


func _run_self_play_stats_checks() -> void:
	print("\nChecking: Native Draft Self-Play Stats Gate")
	_report_lines.append("## Native Draft Self-Play Stats Gate")
	_report_lines.append("Smoke output: model_stats/stats_selfplay_smoke")
	_report_lines.append("Gate report: logs/native_draft_self_play_stats_gate_report.md")

	var gate_report := "res://logs/native_draft_self_play_stats_gate_report.md"
	var smoke_dir := "res://model_stats/stats_selfplay_smoke"

	var self_play_pass := true

	if not DirAccess.dir_exists_absolute(ProjectSettings.globalize_path(smoke_dir)):
		_report_lines.append("Result: FAIL - Self-play smoke output not found (run native_draft_self_play_stats.gd)")
		print("  FAIL - Self-play smoke output not found")
		self_play_pass = false
	elif not _check_report_status(gate_report, "PASS"):
		_report_lines.append("Result: FAIL - Self-play stats gate report missing or not STATUS: PASS")
		print("  FAIL - Self-play stats gate report")
		self_play_pass = false

	if self_play_pass:
		_report_lines.append("Result: PASS")
		print("  PASS")
	else:
		_overall_pass = false

	_report_lines.append("")


func _run_lookahead_gate_checks() -> void:
	print("\nChecking (optional): Native Draft Lookahead Gate")
	_report_lines.append("## Native Draft Lookahead Gate")
	_report_lines.append("Script: native_draft_lookahead_gate.gd")
	var gate_report := "res://logs/native_draft_lookahead_gate_report.md"
	_report_lines.append("Report file: native_draft_lookahead_gate_report.md")

	if not FileAccess.file_exists(ProjectSettings.globalize_path(gate_report)):
		_report_lines.append("Result: SKIPPED (report not found; run lookahead calibration + gate)")
		print("  SKIPPED - report not found")
		_report_lines.append("")
		return

	if _check_report_status(gate_report, "PASS"):
		_report_lines.append("Result: PASS")
		print("  PASS")
	else:
		_report_lines.append("Result: FAIL - gate report not STATUS: PASS")
		print("  FAIL - gate report")
		# Optional check: do not fail overall suite
	_report_lines.append("")


func _run_stats_certification_checks() -> void:
	print("\nChecking (optional): Native Draft Stats Certification")
	_report_lines.append("## Native Draft Stats Certification")
	_report_lines.append("Script: native_draft_stats_certification.gd")
	var cert_report := "res://logs/native_draft_stats_certification_report.md"
	_report_lines.append("Report file: native_draft_stats_certification_report.md")

	if not FileAccess.file_exists(ProjectSettings.globalize_path(cert_report)):
		_report_lines.append("Result: SKIPPED (report not found; run stats certification pipeline)")
		print("  SKIPPED - report not found")
		_report_lines.append("")
		return

	if _check_report_status(cert_report, "PASS"):
		_report_lines.append("Result: PASS")
		print("  PASS")
	else:
		_report_lines.append("Result: FAIL - certification report not STATUS: PASS")
		print("  FAIL - certification report")
	_report_lines.append("")


func _check_report_status(report_path: String, expected_status: String) -> bool:
	if not FileAccess.file_exists(ProjectSettings.globalize_path(report_path)):
		return false
	var f := FileAccess.open(ProjectSettings.globalize_path(report_path), FileAccess.READ)
	if f == null:
		return false
	var content := f.get_as_text()
	f.close()
	if _contains_terminal_marker(content):
		return false
	for line in content.split("\n"):
		if line.begins_with("STATUS:"):
			return line.substr(7).strip_edges() == expected_status
	return false


func _check_optional_report_file(check_name: String, script_name: String, report_file: String) -> void:
	var script_path := "res://scripts/tools/" + script_name
	if not FileAccess.file_exists(ProjectSettings.globalize_path(script_path)):
		print("\nSkipping (not found): " + check_name)
		_report_lines.append("## " + check_name)
		_report_lines.append("Script: " + script_name)
		_report_lines.append("Result: SKIPPED (script not found)")
		_report_lines.append("")
		return

	print("\nChecking (optional): " + check_name)
	_report_lines.append("## " + check_name)
	_report_lines.append("Script: " + script_name)
	_report_lines.append("Report file: " + report_file)

	# Read the report file to get status
	var report_path = "res://logs/" + report_file
	if not FileAccess.file_exists(ProjectSettings.globalize_path(report_path)):
		_report_lines.append("Result: FAIL - Report file not found")
		_report_lines.append("Note: Run the individual validation script to generate this report")
		_report_lines.append("Command: C:\\Godot\\godot.exe --headless --path . --script res://scripts/tools/" + script_name)
		print("  FAIL - Report file not found")
		_overall_pass = false
		_report_lines.append("")
		return

	var f = FileAccess.open(ProjectSettings.globalize_path(report_path), FileAccess.READ)
	if not f:
		_report_lines.append("Result: FAIL - Could not read report file")
		print("  FAIL - Could not read report file")
		_overall_pass = false
		_report_lines.append("")
		return

	var content := f.get_as_text()
	f.close()

	# Parse STATUS line
	var status := "UNKNOWN"
	for line in content.split("\n"):
		if line.begins_with("STATUS:"):
			status = line.substr(7).strip_edges()
			break

	if status == "PASS" and not _contains_terminal_marker(content):
		_report_lines.append("Result: PASS")
		print("  PASS")
	else:
		_report_lines.append("Result: FAIL")
		_report_lines.append("Status from report: " + status)
		print("  FAIL - Status: " + status)
		_overall_pass = false

	_report_lines.append("")


func _contains_terminal_marker(content: String) -> bool:
	for line in content.split("\n"):
		var normalized_line := String(line).strip_edges().to_upper()
		if normalized_line.find("FAIL") >= 0 or normalized_line.begins_with("ERROR") or normalized_line.begins_with("EXCEPTION") or normalized_line.begins_with("WARNING") or normalized_line.begins_with("WARN"):
			return true
	return false


func _write_report() -> void:
	var output_path = "res://logs/draft_ai_validation_suite_report.md"
	var f = FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f:
		f.store_string("\n".join(_report_lines))
		f.close()
		print("\nReport written to: " + output_path)
	else:
		push_error("Failed to write report to: " + output_path)
