extends SceneTree

## Checks relative Elo ordering constraints from native_draft_elo_ladder.gd output.
##
## Usage:
##   godot --headless --path . --script res://scripts/tools/native_draft_elo_gate.gd \
##     -- --input=res://model_stats/native_draft_elo_ladder.csv

const DraftEloGateCoreScript := preload("res://scripts/tools/draft_elo_gate_core.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

var _input_path: String = ""
var _draft_summary_path: String = ""
var _output_path: String = "res://logs/native_draft_elo_gate_report.md"
var _min_gap: float = DraftEloGateCoreScript.DEFAULT_MIN_GAP
var _ordering: Array[Dictionary] = []


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	_input_path = _extract_argument("--input=", "")
	_draft_summary_path = _extract_argument("--draft-summary=", "")
	_output_path = _extract_argument("--output=", "res://logs/native_draft_elo_gate_report.md")
	_min_gap = _parse_float_arg("--min-gap=", str(DraftEloGateCoreScript.DEFAULT_MIN_GAP))
	_ordering = _parse_ordering(_extract_argument("--ordering=", ""))

	if _input_path.is_empty():
		push_error("native_draft_elo_gate: --input required")
		await _finish(false, ["--input required"], [])
		return

	var freshness_errors: Array[String] = []
	if not _draft_summary_path.is_empty():
		freshness_errors = DraftEloGateCoreScript.check_input_freshness(_input_path, _draft_summary_path)
	elif not FileAccess.file_exists(ProjectSettings.globalize_path(_input_path)):
		freshness_errors = ["ladder CSV not found: %s" % _input_path]

	if not freshness_errors.is_empty():
		push_error("native_draft_elo_gate: stale or missing ladder input")
		await _finish(false, freshness_errors, [])
		return

	var ratings: Dictionary = DraftEloGateCoreScript.load_ratings_csv(_input_path)
	if ratings.is_empty():
		push_error("native_draft_elo_gate: ladder CSV empty or missing")
		await _finish(false, ["ladder CSV empty or missing"], [])
		return

	var checks: Array[Dictionary] = DraftEloGateCoreScript.run_ordering_checks(ratings, _ordering, _min_gap)
	var overall_pass: bool = true
	for check in checks:
		if not check["pass"]:
			overall_pass = false
			break

	await _finish(overall_pass, [], checks)


func _parse_float_arg(prefix: String, default_value: String) -> float:
	var raw: String = _extract_argument(prefix, default_value)
	if not raw.is_valid_float():
		push_warning("native_draft_elo_gate: invalid float for %s (%s), using default %s" % [prefix, raw, default_value])
		raw = default_value
	return float(raw)


func _parse_ordering(raw: String) -> Array[Dictionary]:
	if raw.is_empty():
		return DraftEloGateCoreScript.DEFAULT_ORDERING.duplicate(true)
	var out: Array[Dictionary] = []
	for part in raw.split(","):
		var trimmed: String = part.strip_edges()
		if trimmed.is_empty():
			continue
		var pieces: PackedStringArray = trimmed.split(":")
		if pieces.size() != 2:
			push_warning("native_draft_elo_gate: invalid ordering entry '%s', expected higher:lower" % trimmed)
			continue
		var higher: String = pieces[0].strip_edges()
		var lower: String = pieces[1].strip_edges()
		if higher.is_empty() or lower.is_empty():
			push_warning("native_draft_elo_gate: invalid ordering entry '%s', expected higher:lower" % trimmed)
			continue
		out.append({"higher": higher, "lower": lower})
	if out.is_empty():
		push_warning("native_draft_elo_gate: no valid --ordering entries, using defaults")
		return DraftEloGateCoreScript.DEFAULT_ORDERING.duplicate(true)
	return out


func _write_report(overall_pass: bool, errors: Array[String], checks: Array[Dictionary]) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Elo Gate Report")
	lines.append("")
	lines.append("Generated: " + Time.get_datetime_string_from_system())
	lines.append("")
	lines.append("Input: %s" % _input_path)
	if not _draft_summary_path.is_empty():
		lines.append("Draft summary: %s" % _draft_summary_path)
	lines.append("Min gap: %.1f Elo points" % _min_gap)
	lines.append("")
	if not errors.is_empty():
		lines.append("## Errors")
		for err in errors:
			lines.append("- " + err)
		lines.append("")
	lines.append("## Ordering Checks")
	for check in checks:
		var status: String = "PASS" if check["pass"] else "FAIL"
		lines.append("- %s > %s: %s (%.1f vs %.1f, gap=%.1f, min=%.1f) [%s]" % [
			check["higher"], check["lower"], status,
			check["higher_elo"], check["lower_elo"], check["gap"], check["min_gap"], check["reason"]
		])
	if checks.is_empty():
		lines.append("- (no checks)")
	lines.append("")
	lines.append("## Overall")
	lines.append("STATUS: %s" % ("PASS" if overall_pass else "FAIL"))

	var f := FileAccess.open(ProjectSettings.globalize_path(_output_path), FileAccess.WRITE)
	if f == null:
		push_error("native_draft_elo_gate: could not write report %s" % _output_path)
		return
	f.store_string("\n".join(lines) + "\n")
	f.close()
	print("native_draft_elo_gate: wrote %s" % _output_path)


func _finish(overall_pass: bool, errors: Array[String], checks: Array[Dictionary]) -> void:
	_write_report(overall_pass, errors, checks)
	print("native_draft_elo_gate: %s" % ("PASS" if overall_pass else "FAIL"))
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0 if overall_pass else 1)
