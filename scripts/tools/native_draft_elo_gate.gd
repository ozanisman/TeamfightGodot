extends SceneTree

## Checks relative Elo ordering constraints from native_draft_elo_ladder.gd output.
##
## Usage:
##   godot --headless --path . --script res://scripts/tools/native_draft_elo_gate.gd \
##     -- --input=res://model_stats/native_draft_elo_ladder.csv
##
## Optional:
##     --draft-summary=res://model_stats/native_draft_validation_drafts.csv  (fail if ladder older)
##     --ordering=native_full:random,native_softmax:random
##     --min-gap=10.0

const DEFAULT_MIN_GAP: float = 10.0

const DEFAULT_ORDERING: Array[Dictionary] = [
	{"higher": "native_full", "lower": "random"},
	{"higher": "native_softmax", "lower": "random"},
]

const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

var _input_path: String = ""
var _draft_summary_path: String = ""
var _output_path: String = "res://logs/native_draft_elo_gate_report.md"
var _min_gap: float = DEFAULT_MIN_GAP
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
	_min_gap = _parse_float_arg("--min-gap=", str(DEFAULT_MIN_GAP))
	_ordering = _parse_ordering(_extract_argument("--ordering=", ""))

	if _input_path.is_empty():
		push_error("native_draft_elo_gate: --input required")
		await _finish(false, ["--input required"], [])
		return

	var freshness_errors: Array[String] = _check_input_freshness()
	if not freshness_errors.is_empty():
		push_error("native_draft_elo_gate: stale or missing ladder input")
		await _finish(false, freshness_errors, [])
		return

	var ratings: Dictionary = _load_ratings(_input_path)
	if ratings.is_empty():
		push_error("native_draft_elo_gate: ladder CSV empty or missing")
		await _finish(false, ["ladder CSV empty or missing"], [])
		return

	var checks: Array[Dictionary] = _run_checks(ratings)
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
		return DEFAULT_ORDERING.duplicate(true)
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
		return DEFAULT_ORDERING.duplicate(true)
	return out


func _check_input_freshness() -> Array[String]:
	var errors: Array[String] = []
	var ladder_global: String = ProjectSettings.globalize_path(_input_path)
	if not FileAccess.file_exists(ladder_global):
		errors.append("ladder CSV not found: %s" % _input_path)
		return errors

	if _draft_summary_path.is_empty():
		return errors

	var summary_global: String = ProjectSettings.globalize_path(_draft_summary_path)
	if not FileAccess.file_exists(summary_global):
		errors.append("draft summary not found: %s" % _draft_summary_path)
		return errors

	var ladder_mtime: int = FileAccess.get_modified_time(ladder_global)
	var summary_mtime: int = FileAccess.get_modified_time(summary_global)
	if ladder_mtime < summary_mtime:
		errors.append(
			"ladder CSV is older than draft summary (%s < %s); re-run native_draft_elo_ladder.gd" % [
				_input_path, _draft_summary_path
			]
		)
	return errors


func _load_ratings(path: String) -> Dictionary:
	var ratings: Dictionary = {}
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		return ratings
	var header: Array = _split_csv_line(f.get_line())
	var idx: Dictionary = {}
	for i in range(header.size()):
		idx[header[i].strip_edges()] = i
	if not idx.has("strategy") or not idx.has("elo"):
		push_error("native_draft_elo_gate: ladder CSV missing strategy or elo column")
		f.close()
		return ratings
	while not f.eof_reached():
		var line: String = f.get_line()
		if line.strip_edges().is_empty():
			continue
		var fields: Array = _split_csv_line(line)
		if idx["strategy"] >= fields.size() or idx["elo"] >= fields.size():
			continue
		var strategy: String = fields[idx["strategy"]].strip_edges()
		var elo_str: String = fields[idx["elo"]].strip_edges()
		if not elo_str.is_valid_float():
			continue
		ratings[strategy] = float(elo_str)
	f.close()
	return ratings


func _split_csv_line(line: String) -> Array:
	var out: Array = []
	var current: String = ""
	var in_quotes: bool = false
	var i: int = 0
	while i < line.length():
		var c: String = line[i]
		if c == "\"":
			if in_quotes and i + 1 < line.length() and line[i + 1] == "\"":
				current += "\""
				i += 1
			else:
				in_quotes = not in_quotes
		elif c == "," and not in_quotes:
			out.append(current.strip_edges())
			current = ""
		else:
			current += c
		i += 1
	out.append(current.strip_edges())
	return out


func _run_checks(ratings: Dictionary) -> Array[Dictionary]:
	var checks: Array[Dictionary] = []
	for constraint in _ordering:
		var higher: String = constraint["higher"]
		var lower: String = constraint["lower"]
		var higher_present: bool = ratings.has(higher)
		var lower_present: bool = ratings.has(lower)
		var gap: float = 0.0
		var pass_: bool = false
		var reason: String = "ok"
		if not higher_present or not lower_present:
			reason = "missing strategy in ladder"
		else:
			gap = ratings[higher] - ratings[lower]
			pass_ = gap >= _min_gap
			if not pass_:
				reason = "gap below min"
		checks.append({
			"higher": higher,
			"lower": lower,
			"higher_elo": ratings.get(higher, 0.0),
			"lower_elo": ratings.get(lower, 0.0),
			"gap": gap,
			"min_gap": _min_gap,
			"pass": higher_present and lower_present and pass_,
			"reason": reason,
		})
	return checks


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
