extends SceneTree

## Structural gate for candidate-wide draft-state rows from self-play.

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
	"legal_pool_count",
	"selected",
	"candidate",
	"is_selected",
	"candidate_rank_native",
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
	"base_power",
	"ally_synergy",
	"enemy_counter_value",
	"counter_risk",
	"role_fit",
	"comp_fit",
	"total_score",
	"phase_label",
	"candidate_role",
]

const MAX_SAMPLE_ERRORS: int = 12

var _input_path: String = ""
var _output_path: String = "res://logs/native_draft_candidate_rows_gate_report.md"
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
	_output_path = _extract_argument("--output=", "res://logs/native_draft_candidate_rows_gate_report.md")
	_drafts = maxi(1, int(_extract_argument("--drafts=", "1")))
	_blue_strategies = _parse_strategy_names(_extract_argument("--blue-strategies=", "native_softmax"))
	_red_strategies = _parse_strategy_names(_extract_argument("--red-strategies=", "native_softmax"))

	var checks: Array[Dictionary] = []
	if _input_path.is_empty():
		checks.append(_check("input_path", false, "--input required"))
		_write_report(false, checks)
		quit(1)
		return

	var load_result: Dictionary = _load_rows(_input_path)
	checks.append_array(Array(load_result.get("checks", [])))
	var rows: Array[Dictionary] = []
	for row_var in Array(load_result.get("rows", [])):
		rows.append(row_var)
	if rows.is_empty():
		_write_report(false, checks)
		quit(1)
		return

	checks.append(_check_groups(rows))

	var overall_pass: bool = true
	for check in checks:
		if not bool(check.get("pass", false)):
			overall_pass = false
			break

	_write_report(overall_pass, checks)
	print("native_draft_candidate_rows_gate: %s" % ("PASS" if overall_pass else "FAIL"))
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
	checks.append(_check(
		"required_columns",
		missing.is_empty(),
		"all required columns present" if missing.is_empty() else "missing: %s" % ",".join(missing)
	))
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
			row[key] = String(fields[col]) if col < fields.size() else ""
		rows.append(row)
	f.close()
	checks.append(_check("non_empty_rows", not rows.is_empty(), "%d data rows" % rows.size()))
	return {"checks": checks, "rows": rows}


func _check_groups(rows: Array[Dictionary]) -> Dictionary:
	var errors: Array[String] = []
	var groups: Dictionary = {}
	for i in range(rows.size()):
		var row: Dictionary = rows[i]
		_check_row_fields(row, i + 2, errors)
		var group_key: String = _group_key(row)
		if not groups.has(group_key):
			groups[group_key] = []
		groups[group_key].append(row)
		if errors.size() >= MAX_SAMPLE_ERRORS:
			break

	var expected_groups: int = _drafts * _blue_strategies.size() * _red_strategies.size() * SimConstantsScript.DRAFT_SEQUENCE.size()
	if groups.size() != expected_groups:
		errors.append("group count actual=%d expected=%d" % [groups.size(), expected_groups])

	for key in groups.keys():
		_check_group(String(key), Array(groups[key]), errors)
		if errors.size() >= MAX_SAMPLE_ERRORS:
			break

	var detail: String = "all candidate groups valid" if errors.is_empty() else "; ".join(errors)
	return _check("candidate_row_contract", errors.is_empty(), detail)


func _check_row_fields(row: Dictionary, line_number: int, errors: Array[String]) -> void:
	for col in ["draft_seed", "pairing_index", "pairing_seed", "step_index", "legal_pool_count", "is_selected", "candidate_rank_native", "blue_wins", "red_wins", "draws", "total_matches"]:
		var value: String = String(row.get(col, "")).strip_edges()
		if not value.is_valid_int():
			errors.append("line %d invalid %s" % [line_number, col])
			return
	for col in ["blue_winrate", "red_winrate", "drawrate", "base_power", "ally_synergy", "enemy_counter_value", "counter_risk", "role_fit", "comp_fit", "total_score"]:
		var value: String = String(row.get(col, "")).strip_edges()
		if not value.is_valid_float():
			errors.append("line %d invalid %s" % [line_number, col])
			return
	if int(row["total_matches"]) <= 0:
		errors.append("line %d total_matches must be positive" % line_number)
	var step_index: int = int(row["step_index"])
	if step_index < 0 or step_index >= SimConstantsScript.DRAFT_SEQUENCE.size():
		errors.append("line %d step_index out of range" % line_number)
		return
	var turn: String = String(SimConstantsScript.DRAFT_SEQUENCE[step_index])
	if String(row.get("step_side", "")) != turn.substr(0, 1):
		errors.append("line %d step_side does not match DRAFT_SEQUENCE" % line_number)
	if String(row.get("step_action", "")) != turn.substr(2):
		errors.append("line %d step_action does not match DRAFT_SEQUENCE" % line_number)


func _check_group(key: String, rows: Array, errors: Array[String]) -> void:
	if rows.is_empty():
		errors.append("group %s is empty" % key)
		return
	var first: Dictionary = rows[0]
	var legal_pool: Array[String] = _split_list(String(first.get("legal_pool", "")))
	var legal_pool_count: int = int(first.get("legal_pool_count", "0"))
	if legal_pool_count != legal_pool.size():
		errors.append("group %s legal_pool_count=%d split_count=%d" % [key, legal_pool_count, legal_pool.size()])
	if rows.size() != legal_pool_count:
		errors.append("group %s row_count=%d legal_pool_count=%d" % [key, rows.size(), legal_pool_count])

	var selected: String = String(first.get("selected", "")).strip_edges()
	var selected_count: int = 0
	var candidate_seen: Dictionary = {}
	var rank_seen: Dictionary = {}
	for row_var in rows:
		var row: Dictionary = row_var
		var candidate: String = String(row.get("candidate", "")).strip_edges()
		if candidate.is_empty():
			errors.append("group %s has empty candidate" % key)
			return
		if candidate_seen.has(candidate):
			errors.append("group %s duplicate candidate %s" % [key, candidate])
			return
		if not legal_pool.has(candidate):
			errors.append("group %s candidate %s missing from legal_pool" % [key, candidate])
			return
		candidate_seen[candidate] = true
		if String(row.get("selected", "")) != selected:
			errors.append("group %s selected field is inconsistent" % key)
			return
		if String(row.get("is_selected", "")) == "1":
			selected_count += 1
			if candidate != selected:
				errors.append("group %s is_selected candidate does not match selected" % key)
				return
		var rank: int = int(row.get("candidate_rank_native", "0"))
		rank_seen[rank] = true
	if selected_count != 1:
		errors.append("group %s selected_count=%d" % [key, selected_count])
	if not candidate_seen.has(selected):
		errors.append("group %s selected %s missing from candidate rows" % [key, selected])
	for legal_candidate in legal_pool:
		if not candidate_seen.has(legal_candidate):
			errors.append("group %s legal candidate %s missing from candidate rows" % [key, legal_candidate])
			break
	for rank in range(1, rows.size() + 1):
		if not rank_seen.has(rank):
			errors.append("group %s missing native rank %d" % [key, rank])
			break


func _group_key(row: Dictionary) -> String:
	return "%s|%s|%s" % [
		String(row.get("draft_seed", "")),
		String(row.get("pairing_index", "")),
		String(row.get("step_index", "")),
	]


func _write_report(overall_pass: bool, checks: Array[Dictionary]) -> void:
	var lines: Array[String] = []
	lines.append("# Native Draft Candidate Rows Gate Report")
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
		push_error("native_draft_candidate_rows_gate: could not write %s" % _output_path)
		quit(1)
		return
	f.store_string("\n".join(lines) + "\n")
	f.close()
	print("native_draft_candidate_rows_gate: wrote %s" % _output_path)


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
