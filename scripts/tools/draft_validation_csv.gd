extends RefCounted
## Shared CSV parsing for native draft validation harness outputs.

const REQUIRED_DRAFT_COLUMNS: Array[String] = [
	"draft_seed", "blue_strategy", "red_strategy",
	"blue_wins", "red_wins", "draws",
	"blue_winrate", "red_winrate", "drawrate",
]


static func load_draft_summaries(path: String) -> Array[Dictionary]:
	var drafts: Array[Dictionary] = []
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("draft_validation_csv: could not open draft summary %s" % path)
		return drafts

	var header: Array = split_csv_line(f.get_line())
	var idx: Dictionary = header_index(header)
	for col in REQUIRED_DRAFT_COLUMNS:
		if not idx.has(col):
			push_error("draft_validation_csv: draft summary missing required column %s" % col)
			f.close()
			return drafts

	while not f.eof_reached():
		var line: String = f.get_line()
		if line.strip_edges().is_empty():
			continue
		var fields: Array = split_csv_line(line)
		var draft: Dictionary = draft_from_fields(fields, idx)
		if not draft.is_empty():
			drafts.append(draft)
	f.close()
	return drafts


static func infer_drafts_from_steps(path: String) -> Array[Dictionary]:
	var drafts: Array[Dictionary] = []
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("draft_validation_csv: could not open input %s" % path)
		return drafts

	var header: Array = split_csv_line(f.get_line())
	var idx: Dictionary = header_index(header)
	for col in REQUIRED_DRAFT_COLUMNS:
		if not idx.has(col):
			push_error("draft_validation_csv: per-step input missing required column %s" % col)
			f.close()
			return drafts
	if not idx.has("step_index"):
		push_error("draft_validation_csv: per-step input missing required column step_index")
		f.close()
		return drafts

	var best_by_key: Dictionary = {}
	while not f.eof_reached():
		var line: String = f.get_line()
		if line.strip_edges().is_empty():
			continue
		var fields: Array = split_csv_line(line)
		var draft: Dictionary = draft_from_fields(fields, idx)
		if draft.is_empty():
			continue
		if idx["step_index"] >= fields.size():
			push_warning("draft_validation_csv: malformed row, missing step_index")
			continue
		var step_index_str: String = fields[idx["step_index"]].strip_edges()
		if not step_index_str.is_valid_int():
			push_warning("draft_validation_csv: invalid step_index '%s', skipping row" % step_index_str)
			continue
		var step_index: int = int(step_index_str)
		var key: String = "%s|%s|%s" % [draft["draft_seed"], draft["blue_strategy"], draft["red_strategy"]]
		if not best_by_key.has(key) or step_index > best_by_key[key]["step_index"]:
			best_by_key[key] = draft
			best_by_key[key]["step_index"] = step_index
	f.close()

	for key in best_by_key.keys():
		best_by_key[key].erase("step_index")
		drafts.append(best_by_key[key])
	return drafts


static func header_index(header: Array) -> Dictionary:
	var idx: Dictionary = {}
	for i in range(header.size()):
		idx[header[i].strip_edges()] = i
	return idx


static func draft_from_fields(fields: Array, idx: Dictionary) -> Dictionary:
	for col in REQUIRED_DRAFT_COLUMNS:
		if not idx.has(col) or idx[col] >= fields.size():
			push_warning("draft_validation_csv: malformed row, missing column %s" % col)
			return {}

	var draft: Dictionary = {}
	if not _read_int_field(fields, idx, "draft_seed", draft):
		return {}
	draft["blue_strategy"] = String(fields[idx["blue_strategy"]])
	draft["red_strategy"] = String(fields[idx["red_strategy"]])
	if not _read_int_field(fields, idx, "blue_wins", draft):
		return {}
	if not _read_int_field(fields, idx, "red_wins", draft):
		return {}
	if not _read_int_field(fields, idx, "draws", draft):
		return {}
	if not _read_float_field(fields, idx, "blue_winrate", draft):
		return {}
	if not _read_float_field(fields, idx, "red_winrate", draft):
		return {}
	if not _read_float_field(fields, idx, "drawrate", draft):
		return {}
	return draft


static func split_csv_line(line: String) -> Array:
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
			out.append(current)
			current = ""
		else:
			current += c
		i += 1
	out.append(current)
	return out


static func _read_int_field(fields: Array, idx: Dictionary, col: String, out: Dictionary) -> bool:
	var s: String = fields[idx[col]].strip_edges()
	if not s.is_valid_int():
		push_warning("draft_validation_csv: malformed row, invalid %s '%s'" % [col, s])
		return false
	out[col] = int(s)
	return true


static func _read_float_field(fields: Array, idx: Dictionary, col: String, out: Dictionary) -> bool:
	var s: String = fields[idx[col]].strip_edges()
	if not s.is_valid_float():
		push_warning("draft_validation_csv: malformed row, invalid %s '%s'" % [col, s])
		return false
	out[col] = float(s)
	return true
