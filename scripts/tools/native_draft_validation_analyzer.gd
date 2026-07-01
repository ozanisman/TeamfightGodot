extends SceneTree

## Reads the per-step CSV from native_draft_validation_harness.gd and emits
## grouped summary metrics. If --draft-summary is provided, it is used instead
## of inferring completed drafts from the per-step rows.
##
## Usage:
##   godot --headless --script res://scripts/tools/native_draft_validation_analyzer.gd \
##     -- --input=res://model_stats/native_draft_validation.csv \
##     --output=res://model_stats/native_draft_validation_summary.csv
##
## Optional:
##     --draft-summary=res://model_stats/native_draft_validation_drafts.csv

const NATIVE_NAME: String = "native_full"

var _input_path: String = ""
var _draft_summary_path: String = ""
var _output_path: String = "res://model_stats/native_draft_validation_summary.csv"


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
	_output_path = _extract_argument("--output=", "res://model_stats/native_draft_validation_summary.csv")

	if _input_path.is_empty():
		push_error("native_draft_validation_analyzer: --input required")
		quit(1)
		return

	var drafts: Array[Dictionary]
	if not _draft_summary_path.is_empty():
		drafts = _load_draft_summaries(_draft_summary_path)
	else:
		drafts = _infer_drafts_from_steps(_input_path)

	if drafts.is_empty():
		push_error("native_draft_validation_analyzer: no draft data found")
		quit(1)
		return

	var rows: Array[String] = []
	rows.append("metric,grouping,trials,total_matches,wins,losses,draws,winrate,drawrate")
	rows.append_array(_matchup_rows(drafts))
	rows.append_array(_native_by_opponent_rows(drafts))
	rows.append_array(_native_by_side_rows(drafts))
	rows.append_array(_side_overall_rows(drafts))

	var f := FileAccess.open(ProjectSettings.globalize_path(_output_path), FileAccess.WRITE)
	if f == null:
		push_error("native_draft_validation_analyzer: could not open output %s" % _output_path)
		quit(1)
		return
	f.store_string("\n".join(rows) + "\n")
	f.close()

	print("native_draft_validation_analyzer: wrote %s (%d rows)" % [_output_path, rows.size() - 1])
	quit(0)


func _load_draft_summaries(path: String) -> Array[Dictionary]:
	var drafts: Array[Dictionary] = []
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("native_draft_validation_analyzer: could not open draft summary %s" % path)
		return drafts

	var header: Array = _split_csv_line(f.get_line())
	var idx: Dictionary = _header_index(header)
	if not idx.has("draft_seed") or not idx.has("blue_strategy") or not idx.has("red_strategy"):
		push_error("native_draft_validation_analyzer: draft summary missing required columns")
		return drafts

	while not f.eof_reached():
		var line: String = f.get_line()
		if line.strip_edges().is_empty():
			continue
		var fields: Array = _split_csv_line(line)
		drafts.append(_draft_from_fields(fields, idx))
	f.close()
	return drafts


func _infer_drafts_from_steps(path: String) -> Array[Dictionary]:
	var drafts: Array[Dictionary] = []
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("native_draft_validation_analyzer: could not open input %s" % path)
		return drafts

	var header: Array = _split_csv_line(f.get_line())
	var idx: Dictionary = _header_index(header)
	if not idx.has("draft_seed") or not idx.has("blue_strategy") or not idx.has("red_strategy") or not idx.has("step_index"):
		push_error("native_draft_validation_analyzer: per-step input missing required columns")
		return drafts

	var best_by_key: Dictionary = {}
	while not f.eof_reached():
		var line: String = f.get_line()
		if line.strip_edges().is_empty():
			continue
		var fields: Array = _split_csv_line(line)
		var draft: Dictionary = _draft_from_fields(fields, idx)
		var step_index: int = int(fields[idx["step_index"]])
		var key: String = "%s|%s|%s" % [draft["draft_seed"], draft["blue_strategy"], draft["red_strategy"]]
		if not best_by_key.has(key) or step_index > best_by_key[key]["step_index"]:
			best_by_key[key] = draft
			best_by_key[key]["step_index"] = step_index
	f.close()

	for key in best_by_key.keys():
		best_by_key[key].erase("step_index")
		drafts.append(best_by_key[key])
	return drafts


func _header_index(header: Array) -> Dictionary:
	var idx: Dictionary = {}
	for i in range(header.size()):
		idx[header[i]] = i
	return idx


func _draft_from_fields(fields: Array, idx: Dictionary) -> Dictionary:
	var draft: Dictionary = {
		"draft_seed": int(fields[idx["draft_seed"]]),
		"blue_strategy": String(fields[idx["blue_strategy"]]),
		"red_strategy": String(fields[idx["red_strategy"]]),
		"blue_wins": int(fields[idx["blue_wins"]]),
		"red_wins": int(fields[idx["red_wins"]]),
		"draws": int(fields[idx["draws"]]),
		"blue_winrate": float(fields[idx["blue_winrate"]]),
		"red_winrate": float(fields[idx["red_winrate"]]),
		"drawrate": float(fields[idx["drawrate"]])
	}
	return draft


func _split_csv_line(line: String) -> Array:
	var out: Array = []
	for field in line.split(","):
		out.append(field.strip_edges())
	return out


func _format_metric_row(metric: String, grouping: String, trials: int, wins: int, losses: int, draws: int) -> String:
	var total_matches: int = wins + losses + draws
	if total_matches <= 0:
		total_matches = 1
	var winrate: float = float(wins) / float(total_matches)
	var drawrate: float = float(draws) / float(total_matches)
	return "%s,%s,%d,%d,%d,%d,%d,%.6f,%.6f" % [
		metric, grouping, trials, total_matches, wins, losses, draws, winrate, drawrate
	]


func _matchup_rows(drafts: Array[Dictionary]) -> Array[String]:
	var groups: Dictionary = {}
	for draft in drafts:
		var key: String = "blue=%s red=%s" % [draft["blue_strategy"], draft["red_strategy"]]
		if not groups.has(key):
			groups[key] = {
				"blue_strategy": draft["blue_strategy"],
				"red_strategy": draft["red_strategy"],
				"trials": 0,
				"blue_wins": 0,
				"red_wins": 0,
				"draws": 0
			}
		var g: Dictionary = groups[key]
		g["trials"] += 1
		g["blue_wins"] += int(draft["blue_wins"])
		g["red_wins"] += int(draft["red_wins"])
		g["draws"] += int(draft["draws"])

	var rows: Array[String] = []
	for key in groups.keys():
		var g: Dictionary = groups[key]
		rows.append(_format_metric_row(
			"matchup", key, g["trials"], g["blue_wins"], g["red_wins"], g["draws"]
		))
	return rows


func _native_by_opponent_rows(drafts: Array[Dictionary]) -> Array[String]:
	var groups: Dictionary = {}
	for draft in drafts:
		var native_info: Dictionary = _native_side(draft)
		if native_info.is_empty():
			continue
		var key: String = "native=%s opponent=%s" % [native_info["native_strategy"], native_info["opponent_strategy"]]
		if not groups.has(key):
			groups[key] = {
				"native_strategy": native_info["native_strategy"],
				"opponent_strategy": native_info["opponent_strategy"],
				"trials": 0,
				"native_wins": 0,
				"opponent_wins": 0,
				"draws": 0
			}
		var g: Dictionary = groups[key]
		g["trials"] += 1
		g["native_wins"] += int(native_info["native_wins"])
		g["opponent_wins"] += int(native_info["opponent_wins"])
		g["draws"] += int(native_info["draws"])

	var rows: Array[String] = []
	for key in groups.keys():
		var g: Dictionary = groups[key]
		rows.append(_format_metric_row(
			"native_winrate_by_opponent", key, g["trials"], g["native_wins"], g["opponent_wins"], g["draws"]
		))
	return rows


func _native_by_side_rows(drafts: Array[Dictionary]) -> Array[String]:
	var groups: Dictionary = {}
	for draft in drafts:
		var native_info: Dictionary = _native_side(draft)
		if native_info.is_empty():
			continue
		var key: String = "native=%s side=%s" % [native_info["native_strategy"], native_info["native_side"]]
		if not groups.has(key):
			groups[key] = {
				"native_strategy": native_info["native_strategy"],
				"native_side": native_info["native_side"],
				"trials": 0,
				"native_wins": 0,
				"opponent_wins": 0,
				"draws": 0
			}
		var g: Dictionary = groups[key]
		g["trials"] += 1
		g["native_wins"] += int(native_info["native_wins"])
		g["opponent_wins"] += int(native_info["opponent_wins"])
		g["draws"] += int(native_info["draws"])

	var rows: Array[String] = []
	for key in groups.keys():
		var g: Dictionary = groups[key]
		rows.append(_format_metric_row(
			"native_winrate_by_side", key, g["trials"], g["native_wins"], g["opponent_wins"], g["draws"]
		))
	return rows


func _side_overall_rows(drafts: Array[Dictionary]) -> Array[String]:
	var blue_wins: int = 0
	var red_wins: int = 0
	var draws: int = 0
	var trials: int = drafts.size()
	for draft in drafts:
		blue_wins += int(draft["blue_wins"])
		red_wins += int(draft["red_wins"])
		draws += int(draft["draws"])

	var rows: Array[String] = []
	rows.append(_format_metric_row("blue_side_winrate", "overall", trials, blue_wins, red_wins, draws))
	rows.append(_format_metric_row("red_side_winrate", "overall", trials, red_wins, blue_wins, draws))
	rows.append(_format_metric_row("drawrate", "overall", trials, draws, blue_wins + red_wins, 0))
	return rows


func _native_side(draft: Dictionary) -> Dictionary:
	var blue_is_native: bool = draft["blue_strategy"] == NATIVE_NAME
	var red_is_native: bool = draft["red_strategy"] == NATIVE_NAME
	if blue_is_native == red_is_native:
		return {}
	if blue_is_native:
		return {
			"native_strategy": draft["blue_strategy"],
			"opponent_strategy": draft["red_strategy"],
			"native_side": "blue",
			"native_wins": int(draft["blue_wins"]),
			"opponent_wins": int(draft["red_wins"]),
			"draws": int(draft["draws"])
		}
	return {
		"native_strategy": draft["red_strategy"],
		"opponent_strategy": draft["blue_strategy"],
		"native_side": "red",
		"native_wins": int(draft["red_wins"]),
		"opponent_wins": int(draft["blue_wins"]),
		"draws": int(draft["draws"])
	}
