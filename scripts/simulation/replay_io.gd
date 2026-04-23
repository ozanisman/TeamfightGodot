class_name ReplayIO
extends RefCounted

const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const MatchReplaySummaryScript := preload("res://scripts/simulation/match_replay_summary.gd")

static func serialize_input(match_input) -> String:
	return JSON.stringify(match_input.to_dict(), "\t")

static func serialize_summary(summary) -> String:
	return JSON.stringify(summary.to_dict(), "\t")

static func serialize_summary_batch(summaries: Array) -> String:
	var payload: Array = []
	payload.resize(summaries.size())
	for index in range(summaries.size()):
		var summary: Variant = summaries[index]
		payload[index] = summary.to_dict() if summary is Object and summary.has_method("to_dict") else summary
	return JSON.stringify(payload, "\t")

static func parse_input(text: String):
	var parsed: Variant = JSON.parse_string(text)
	if not (parsed is Dictionary):
		push_error("Invalid replay input JSON.")
		return MatchReplayInputScript.new()
	return MatchReplayInputScript.from_dict(Dictionary(parsed))

static func parse_summary(text: String):
	var parsed: Variant = JSON.parse_string(text)
	if not (parsed is Dictionary):
		push_error("Invalid replay summary JSON.")
		return MatchReplaySummaryScript.new()
	return MatchReplaySummaryScript.from_dict(Dictionary(parsed))

static func load_input_file(path: String):
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		push_error("Failed to open replay input file: %s" % path)
		return MatchReplayInputScript.new()
	return parse_input(file.get_as_text())

static func save_summary_file(path: String, summary) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		push_error("Failed to write replay summary file: %s" % path)
		return
	file.store_string(serialize_summary(summary))

static func save_summary_batch_file(path: String, summaries: Array) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		push_error("Failed to write replay summary batch file: %s" % path)
		return
	file.store_string(serialize_summary_batch(summaries))
