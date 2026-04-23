class_name ParityTools
extends RefCounted

const MatchReplaySummaryScript := preload("res://scripts/simulation/match_replay_summary.gd")

static func _normalize(value: Variant) -> Variant:
	if value is Dictionary:
		var normalized := {}
		var keys: Array = value.keys()
		keys.sort()
		for key in keys:
			normalized[key] = _normalize(value[key])
		return normalized
	if value is Array:
		var items: Array = []
		items.resize(value.size())
		for index in range(value.size()):
			items[index] = _normalize(value[index])
		return items
	return value

static func canonical_match_payload(summary) -> Dictionary:
	return _normalize(summary.to_dict())

static func match_signature(summary) -> String:
	var payload := canonical_match_payload(summary)
	var encoded := JSON.stringify(payload, "", true, true)
	return encoded.sha256_text()

static func hash_payload(payload: Variant) -> String:
	var encoded := JSON.stringify(_normalize(payload), "", true, true)
	return encoded.sha256_text()
