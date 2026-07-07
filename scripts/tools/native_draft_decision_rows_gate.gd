extends SceneTree

## Structural gate for native_draft_self_play_stats.gd decision-row output.
##
## Usage:
##   godot --headless --path . --script res://scripts/tools/native_draft_decision_rows_gate.gd \
##     -- --input=res://model_stats/draft_state_training_rows_smoke.csv \
##        --drafts=2 --blue-strategies=native_softmax --red-strategies=native_softmax

const DraftValidationCsvScript := preload("res://scripts/tools/draft_validation_csv.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")

const REQUIRED_COLUMNS: Array[String] = [
	"draft_seed",
	"pairing_index",
	"pairing_seed",
	"blue_strategy",
	"red_strategy",
	"step_index",
	"step_side",
	"step_action",
	"acting_side",
	"blue_picks_before",
	"red_picks_before",
	"blue_bans_before",
	"red_bans_before",
	"legal_pool",
	"selected",
	"blue_picks_final",
	"red_picks_final",
	"blue_bans_final",
	"red_bans_final",
	"blue_wins",
	"red_wins",
	"draws",
	"blue_winrate",
	"red_winrate",
	"drawrate",
	"total_matches",
]

const MAX_SAMPLE_ERRORS: int = 10

var _input_path: String = ""
var _output_path: String = "res://logs/native_draft_decision_rows_gate_report.md"
var _drafts: int = 1
var _blue_strategies: Array[String] = []
var _red_strategies: Array[String] = []


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

	_input_path = _extract_argument("--input=", "")
	_output_path = _extract_argument("--output=", "res://logs/native_draft_decision_rows_gate_report.md")
	_drafts = maxi(1, int(_extract_argument("--drafts=", "1")))
	_blue_strategies = _parse_strategy_names(_extract_argument("--blue-strategies=", "native_softmax"))
	_red_strategies = _parse_strategy_names(_extract_argument("--red-strategies=", "native_softmax"))

	var checks: Array[Dictionary] = []
	if _input_path.is_empty():
		checks.append(_check("input_path", false, "--input required"))
		_write_report(false, checks)
		quit(1)
		return

	var file_check: Dictionary = _load_rows(_input_path)
	var file_checks: Array = file_check.get("checks", [])
	checks.append_array(file_checks)
	var rows: Array[Dictionary] = []
	for row_var in Array(file_check.get("rows", [])):
		var row: Dictionary = row_var
		rows.append(row)
	if rows.is_empty():
		_write_report(false, checks)
		quit(1)
		return

	checks.append(_check_row_count(rows.size()))
	checks.append(_check_rows(rows))

	var overall_pass: bool = true
	for check in checks:
		if not bool(check.get("pass", false)):
			overall_pass = false
			break

	_write_report(overall_pass, checks)
	print("native_draft_decision_rows_gate: %s" % ("PASS" if overall_pass else "FAIL"))
	quit(0 if overall_pass else 1)


func _load_rows(path: String) -> Dictionary:
	var checks: Array[Dictionary] = []
	var rows: Array[Dictionary] = []
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		checks.append(_check("file_exists", false, "could not open %s" % path))
		return {"checks": checks, "rows": rows}

	var header_line: String = f.get_line()
	if header_line.strip_edges().is_empty():
		checks.append(_check("non_empty", false, "missing header"))
		f.close()
		return {"checks": checks, "rows": rows}

	var header: Array = DraftValidationCsvScript.split_csv_line(header_line)
	var idx: Dictionary = DraftValidationCsvScript.header_index(header)
	var missing: Array[String] = []
	for column in REQUIRED_COLUMNS:
		if not idx.has(column):
			missing.append(column)
	var detail: String = "all required columns present"
	if not missing.is_empty():
		detail = "missing: %s" % ",".join(missing)
	checks.append(_check("required_columns", missing.is_empty(), detail))
	if not missing.is_empty():
		f.close()
		return {"checks": checks, "rows": rows}

	while not f.eof_reached():
		var line: String = f.get_line()
		if line.strip_edges().is_empty():
			continue
		var fields: Array = DraftValidationCsvScript.split_csv_line(line)
		var row: Dictionary = {}
		for key in idx.keys():
			var col: int = int(idx[key])
			if col < fields.size():
				row[key] = fields[col]
		rows.append(row)
	f.close()
	checks.append(_check("non_empty_rows", not rows.is_empty(), "%d data rows" % rows.size()))
	return {"checks": checks, "rows": rows}


func _check_row_count(row_count: int) -> Dictionary:
	var expected: int = _drafts * _blue_strategies.size() * _red_strategies.size() * SimConstantsScript.DRAFT_SEQUENCE.size()
	return _check("row_count", row_count == expected, "actual=%d expected=%d" % [row_count, expected])


func _check_rows(rows: Array[Dictionary]) -> Dictionary:
	var errors: Array[String] = []
	for i in range(rows.size()):
		var row: Dictionary = rows[i]
		_check_selected_in_pool(row, i + 2, errors)
		_check_outcome_fields(row, i + 2, errors)
		_check_step_fields(row, i + 2, errors)
		if errors.size() >= MAX_SAMPLE_ERRORS:
			break
	var detail: String = "all rows valid" if errors.is_empty() else "; ".join(errors)
	return _check("row_contract", errors.is_empty(), detail)


func _check_selected_in_pool(row: Dictionary, line_number: int, errors: Array[String]) -> void:
	var selected: String = String(row.get("selected", "")).strip_edges()
	var legal_pool: Array[String] = _split_list(String(row.get("legal_pool", "")))
	if selected.is_empty():
		errors.append("line %d selected is empty" % line_number)
	elif not legal_pool.has(selected):
		errors.append("line %d selected %s missing from legal_pool" % [line_number, selected])


func _check_outcome_fields(row: Dictionary, line_number: int, errors: Array[String]) -> void:
	for col in ["blue_wins", "red_wins", "draws", "total_matches"]:
		var value: String = String(row.get(col, "")).strip_edges()
		if not value.is_valid_int():
			errors.append("line %d invalid %s" % [line_number, col])
			return
	var total_matches: int = int(row["total_matches"])
	if total_matches <= 0:
		errors.append("line %d total_matches must be positive" % line_number)
		return
	for col in ["blue_winrate", "red_winrate", "drawrate"]:
		var value: String = String(row.get(col, "")).strip_edges()
		if not value.is_valid_float():
			errors.append("line %d invalid %s" % [line_number, col])
			return


func _check_step_fields(row: Dictionary, line_number: int, errors: Array[String]) -> void:
	var step_index_text: String = String(row.get("step_index", "")).strip_edges()
	if not step_index_text.is_valid_int():
		errors.append("line %d invalid step_index" % line_number)
		return
	var step_index: int = int(step_index_text)
	if step_index < 0 or step_index >= SimConstantsScript.DRAFT_SEQUENCE.size():
		errors.append("line %d step_index out of range" % line_number)
		return
	var turn: String = String(SimConstantsScript.DRAFT_SEQUENCE[step_index])
	if String(row.get("step_side", "")) != turn.substr(0, 1):
		errors.append("line %d step_side does not match DRAFT_SEQUENCE" % line_number)
	if String(row.get("step_action", "")) != turn.substr(2):
		errors.append("line %d step_action does not match DRAFT_SEQUENCE" % line_number)


func _write_report(overall_pass: bool, checks: Array[Dictionary]) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Decision Rows Gate Report")
	lines.append("")
	lines.append("Input: `%s`" % _input_path)
	lines.append("Drafts: %d" % _drafts)
	lines.append("Blue strategies: `%s`" % ",".join(_blue_strategies))
	lines.append("Red strategies: `%s`" % ",".join(_red_strategies))
	lines.append("")
	lines.append("## Checks")
	for check in checks:
		var status: String = "PASS" if bool(check.get("pass", false)) else "FAIL"
		lines.append("- %s: %s (%s)" % [String(check.get("name", "")), status, String(check.get("detail", ""))])
	lines.append("")
	lines.append("STATUS: %s" % ("PASS" if overall_pass else "FAIL"))

	var global_path: String = ProjectSettings.globalize_path(_output_path)
	var dir_path: String = global_path.get_base_dir()
	if not DirAccess.dir_exists_absolute(dir_path):
		DirAccess.make_dir_recursive_absolute(dir_path)
	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f == null:
		push_error("native_draft_decision_rows_gate: could not write %s" % _output_path)
		quit(1)
		return
	f.store_string("\n".join(lines) + "\n")
	f.close()
	print("native_draft_decision_rows_gate: wrote %s" % _output_path)


func _parse_strategy_names(s: String) -> Array[String]:
	var seen: Dictionary = {}
	var out: Array[String] = []
	for part in s.split(","):
		var trimmed: String = part.strip_edges()
		if trimmed.is_empty() or seen.has(trimmed):
			continue
		seen[trimmed] = true
		out.append(trimmed)
	return out


func _split_list(raw: String) -> Array[String]:
	var out: Array[String] = []
	for part in raw.split("|", false):
		var trimmed: String = part.strip_edges()
		if not trimmed.is_empty():
			out.append(trimmed)
	return out


func _check(name: String, passed: bool, detail: String) -> Dictionary:
	return {"name": name, "pass": passed, "detail": detail}
