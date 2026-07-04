extends SceneTree

## Structural gate for native_draft_self_play_stats.gd output snapshots.
##
## Usage:
##   godot --headless --path . --script res://scripts/tools/native_draft_self_play_stats_gate.gd \
##     -- --output-dir=res://model_stats/stats_selfplay_smoke --min-matches=50

const DraftStatsSnapshotChecksScript := preload("res://scripts/tools/draft_stats_snapshot_checks.gd")

var _output_dir: String = ""
var _output_path: String = "res://logs/native_draft_self_play_stats_gate_report.md"
var _min_matches: int = DraftStatsSnapshotChecksScript.DEFAULT_MIN_MATCHES


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	await process_frame

	_output_dir = _extract_argument("--output-dir=", "")
	_output_path = _extract_argument("--output=", "res://logs/native_draft_self_play_stats_gate_report.md")
	_min_matches = maxi(1, int(_extract_argument("--min-matches=", str(DraftStatsSnapshotChecksScript.DEFAULT_MIN_MATCHES))))

	if _output_dir.is_empty():
		push_error("native_draft_self_play_stats_gate: --output-dir required")
		_write_report(false, ["--output-dir required"], [])
		quit(1)
		return

	var checks: Array[Dictionary] = DraftStatsSnapshotChecksScript.run_checks(_output_dir, _min_matches)
	var overall_pass: bool = DraftStatsSnapshotChecksScript.overall_pass(checks)

	_write_report(overall_pass, [], checks)
	print("native_draft_self_play_stats_gate: %s" % ("PASS" if overall_pass else "FAIL"))
	quit(0 if overall_pass else 1)


func _write_report(overall_pass: bool, errors: Array[String], checks: Array[Dictionary]) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Self-Play Stats Gate Report")
	lines.append("")
	lines.append("Output dir: `%s`" % _output_dir)
	lines.append("Min matches: %d" % _min_matches)
	lines.append("")
	if not errors.is_empty():
		lines.append("## Errors")
		for err in errors:
			lines.append("- %s" % err)
		lines.append("")
	lines.append("## Checks")
	for check in checks:
		var status: String = "PASS" if check["pass"] else "FAIL"
		lines.append("- %s: %s (%s)" % [check["name"], status, check.get("detail", "")])
	lines.append("")
	lines.append("STATUS: %s" % ("PASS" if overall_pass else "FAIL"))

	var f := FileAccess.open(ProjectSettings.globalize_path(_output_path), FileAccess.WRITE)
	if f == null:
		push_error("native_draft_self_play_stats_gate: could not write %s" % _output_path)
		quit(1)
		return
	f.store_string("\n".join(lines))
	f.close()
	print("native_draft_self_play_stats_gate: wrote %s" % _output_path)
