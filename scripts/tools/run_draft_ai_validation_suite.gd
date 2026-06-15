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
	_run_check("Native Draft AI Tag Validation", "validate_native_draft_ai_tags.gd", "native_draft_ai_tag_path_report.md")
	_run_check("Full Draft Validation (native)", "full_draft_validation.gd", "full_draft_validation_report_native.txt")
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
