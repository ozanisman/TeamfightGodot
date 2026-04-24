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
	# Stabilize fixture signatures when payloads match within float tolerance (binary noise in JSON hash).
	if typeof(value) == TYPE_FLOAT:
		return snappedf(value, 1e-9)
	return value

static func _escape_string(value: String) -> String:
	# JSON.stringify already produces valid JSON string escapes.
	return JSON.stringify(value)

static func _canonical_json(value: Variant) -> String:
	if value is Dictionary:
		var keys: Array = value.keys()
		keys.sort()
		var parts: Array[String] = []
		for key in keys:
			parts.append("%s:%s" % [_escape_string(String(key)), _canonical_json(value[key])])
		return "{%s}" % ",".join(parts)
	if value is Array:
		var parts: Array[String] = []
		for item in value:
			parts.append(_canonical_json(item))
		return "[%s]" % ",".join(parts)
	if value is String or value is StringName:
		return _escape_string(String(value))
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

static func fixture_set_signature(fixtures: Array) -> String:
	# Hash the fixture list itself (order-sensitive by design).
	# Each element is normalized before hashing to avoid key-order noise.
	var normalized: Array = []
	normalized.resize(fixtures.size())
	for index in range(fixtures.size()):
		var entry: Variant = fixtures[index]
		normalized[index] = _normalize(entry)
	return hash_payload(normalized)
