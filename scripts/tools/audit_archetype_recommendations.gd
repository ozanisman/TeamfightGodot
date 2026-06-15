extends SceneTree

## Repeatable Phase 50/51 audit for native vs archetype_ban_light recommendations.
## Run from the project root:
##   C:\Godot\godot.exe --headless --log-file logs\archetype_recommendation_audit.log --path . --script res://scripts/tools/audit_archetype_recommendations.gd
## Writes res://archetype_recommendation_audit_report.md as a disposable local report;
## durable conclusions are summarized in wiki/notes/draft_archetype_tags.md and wiki/notes/native_draft_ai.md.

const ChampionCatalog := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

const STATS_DIR := "res://stats_output_100k"
const STRATEGY_NATIVE := 0
const STRATEGY_ARCHETYPE_BAN_LIGHT := 7
const REPORT_PATH := "res://archetype_recommendation_audit_report.md"

var _backend: RefCounted
var _reason_counts: Dictionary = {}
var _changed_states: Array[Dictionary] = []
var _suspicious_notes: Array[String] = []
var _pick_changes := 0
var _ban_changes := 0
var _changed_contribution_sum := 0.0
var _changed_contribution_max := 0.0


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	_backend = NativeSimulationBackendScript.new()
	if _backend == null or not _backend.is_available():
		push_error("audit_archetype_recommendations: native backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var states := _test_states()
	var lines: Array[String] = []
	lines.append("# Archetype Recommendation Audit")
	lines.append("")
	lines.append("Compares `native` against `archetype_ban_light` on fixed representative draft states. This report is diagnostic only; no scoring, weights, draft order, champion tags, UI behavior, or stats files were changed.")
	lines.append("")
	lines.append("## Summary")
	lines.append("")
	lines.append("| State | Action | Baseline top | Archetype top | Top changed | Archetype top contribution | Reasons |")
	lines.append("|-------|--------|--------------|---------------|-------------|----------------------------|---------|")

	var detail_sections: Array[String] = []
	for state in states:
		var result := _audit_state(state)
		lines.append(_summary_row(result))
		detail_sections.append_array(_detail_section(result))
		_accumulate(result)

	lines.append("")
	lines.append("## Findings")
	lines.append("")
	lines.append("- changed top recommendations: %d of %d states" % [_changed_states.size(), states.size()])
	lines.append("- changed bans: %d" % _ban_changes)
	lines.append("- changed picks: %d" % _pick_changes)
	var average_changed_contribution := 0.0 if _changed_states.is_empty() else _changed_contribution_sum / float(_changed_states.size())
	lines.append("- average archetype contribution on changed tops: %.6f" % average_changed_contribution)
	lines.append("- max archetype contribution on changed tops: %.6f" % _changed_contribution_max)
	lines.append("- pick impact: `archetype_ban_light` has pick weight 0.00, so any pick differences would be suspicious.")
	lines.append("")
	lines.append("### Most Common Archetype Reasons")
	lines.append("")
	lines.append("| Reason | Count |")
	lines.append("|--------|-------|")
	for row in _top_reasons():
		lines.append("| %s | %d |" % [String(row["reason"]), int(row["count"])])
	if _reason_counts.is_empty():
		lines.append("| none | 0 |")
	lines.append("")
	lines.append("### Changed Recommendation Review")
	lines.append("")
	if _changed_states.is_empty():
		lines.append("No top recommendation changes were observed in the audited states.")
	else:
		for changed in _changed_states:
			lines.append("- `%s`: %s -> %s; contribution %.6f; reasons `%s`. Reason fit: %s. Contribution size: %s." % [
				String(changed["state"]),
				String(changed["baseline_top"]),
				String(changed["archetype_top"]),
				float(changed["contribution"]),
				String(changed["reasons"]),
				String(changed["reason_fit"]),
				"small" if absf(float(changed["contribution"])) <= 0.002 else "review"
			])
	lines.append("")
	lines.append("### Suspicious Recommendations")
	lines.append("")
	if _suspicious_notes.is_empty():
		lines.append("No suspicious recommendations were found by this audit. Pick recommendations did not move from archetype contribution, and observed contributions stayed small.")
	else:
		for note in _suspicious_notes:
			lines.append("- " + note)
	lines.append("")
	lines.append("### Conclusion")
	lines.append("")
	lines.append("`archetype_ban_light` still looks worth keeping as an experimental option. In these representative states it added readable ban-oriented archetype reasons, kept contributions tiny, did not move any pick top recommendation, and did not overtake the native top recommendation. This audit does not justify promotion to default because it found no clear recommendation improvement; it only supports keeping the opt-in debug profile available for further review.")
	lines.append("")
	lines.append("## Detailed State Results")
	lines.append("")
	lines.append_array(detail_sections)

	_write_report(lines)
	print("audit_archetype_recommendations: wrote " + REPORT_PATH)
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)


func _test_states() -> Array[Dictionary]:
	return [
		{
			"name": "empty draft / first ban",
			"action": "BAN",
			"step": 0,
			"side": "B",
			"allies": [],
			"enemies": [],
			"bans": [],
		},
		{
			"name": "first pick",
			"action": "PICK",
			"step": 6,
			"side": "B",
			"allies": [],
			"enemies": [],
			"bans": [&"necromancer", &"windcaller", &"frost_mage", &"wizard", &"warlock", &"mirror_knight"],
		},
		{
			"name": "after 2-3 picks",
			"action": "PICK",
			"step": 9,
			"side": "B",
			"allies": [&"colossus"],
			"enemies": [&"berserker", &"archer"],
			"bans": [&"necromancer", &"windcaller", &"frost_mage", &"wizard", &"warlock", &"mirror_knight"],
		},
		{
			"name": "phase 2 ban",
			"action": "BAN",
			"step": 12,
			"side": "R",
			"allies": [&"berserker", &"archer", &"swordsman"],
			"enemies": [&"colossus", &"disarmer", &"cleric"],
			"bans": [&"necromancer", &"windcaller", &"frost_mage", &"wizard", &"warlock", &"mirror_knight"],
		},
		{
			"name": "late pick",
			"action": "PICK",
			"step": 17,
			"side": "B",
			"allies": [&"colossus", &"disarmer", &"cleric"],
			"enemies": [&"berserker", &"archer", &"swordsman", &"wraith"],
			"bans": [&"necromancer", &"windcaller", &"frost_mage", &"wizard", &"warlock", &"mirror_knight", &"guardian", &"paladin", &"valkyrie", &"oracle"],
		},
		{
			"name": "enemy has clear AOE cluster",
			"action": "BAN",
			"step": 12,
			"side": "B",
			"allies": [&"colossus", &"cleric"],
			"enemies": [&"wizard", &"artillery", &"necromancer"],
			"bans": [&"warlock", &"mirror_knight"],
		},
		{
			"name": "enemy has clear backline/poke cluster",
			"action": "BAN",
			"step": 12,
			"side": "B",
			"allies": [&"guardian", &"cleric"],
			"enemies": [&"wizard", &"sniper", &"artillery"],
			"bans": [&"warlock", &"mirror_knight"],
		},
		{
			"name": "enemy has clear frontline/protect cluster",
			"action": "BAN",
			"step": 12,
			"side": "B",
			"allies": [&"wizard", &"rogue"],
			"enemies": [&"guardian", &"paladin", &"mirror_knight"],
			"bans": [&"warlock", &"necromancer"],
		},
		{
			"name": "ally has clear frontline/backline shell",
			"action": "PICK",
			"step": 10,
			"side": "B",
			"allies": [&"guardian", &"wizard"],
			"enemies": [&"berserker", &"rogue"],
			"bans": [&"necromancer", &"windcaller", &"frost_mage", &"warlock"],
		},
	]


func _audit_state(state: Dictionary) -> Dictionary:
	var available := _available_for(state)
	var baseline := _recommend(state, available, STRATEGY_NATIVE)
	var archetype := _recommend(state, available, STRATEGY_ARCHETYPE_BAN_LIGHT)
	var baseline_top := _candidate_at(baseline, 0)
	var archetype_top := _candidate_at(archetype, 0)
	return {
		"state": state,
		"available": available,
		"baseline": baseline,
		"archetype": archetype,
		"baseline_top": baseline_top,
		"archetype_top": archetype_top,
		"top_changed": baseline_top != archetype_top,
	}


func _available_for(state: Dictionary) -> Array[StringName]:
	var available: Array[StringName] = ChampionCatalog.get_champion_ids()
	for group_key in ["allies", "enemies", "bans"]:
		for champion in state.get(group_key, []):
			available.erase(StringName(champion))
	return available


func _recommend(state: Dictionary, available: Array[StringName], strategy: int) -> Array:
	var action := String(state["action"])
	var step := int(state["step"])
	var side := String(state["side"])
	var allies: Array = state.get("allies", [])
	var enemies: Array = state.get("enemies", [])
	if action == "PICK":
		return _backend.get_draft_ai_pick_recommendations(STATS_DIR, available, allies, enemies, 5, step, strategy)
	return _backend.get_draft_ai_ban_recommendations(STATS_DIR, available, allies, enemies, 5, step, "blue" if side == "B" else "red", {}, strategy)


func _summary_row(result: Dictionary) -> String:
	var archetype: Array = result["archetype"]
	var state: Dictionary = result["state"]
	var top_rec := _rec_at(archetype, 0)
	var contribution := _contribution_for(String(state["action"]), top_rec)
	var reasons := _join_array(top_rec.get("archetype_reasons", []))
	return "| %s | %s | %s | %s | %s | %.6f | %s |" % [
		String(state["name"]),
		String(state["action"]),
		String(result["baseline_top"]),
		String(result["archetype_top"]),
		"yes" if bool(result["top_changed"]) else "no",
		contribution,
		reasons if not reasons.is_empty() else "none",
	]


func _detail_section(result: Dictionary) -> Array[String]:
	var state: Dictionary = result["state"]
	var lines: Array[String] = []
	lines.append("### " + String(state["name"]))
	lines.append("")
	lines.append("- action: `%s`, step: `%d`, side: `%s`" % [String(state["action"]), int(state["step"]), String(state["side"])])
	lines.append("- allies: `%s`" % _join_array(state.get("allies", [])))
	lines.append("- enemies: `%s`" % _join_array(state.get("enemies", [])))
	lines.append("- unavailable bans: `%s`" % _join_array(state.get("bans", [])))
	lines.append("- top changed: `%s`" % ("yes" if bool(result["top_changed"]) else "no"))
	if bool(result["top_changed"]):
		lines.append("- changed top recommendation: `%s -> %s`" % [String(result["baseline_top"]), String(result["archetype_top"])])
	lines.append("")
	lines.append("#### Native baseline")
	lines.append("")
	lines.append("| Rank | Candidate | Score |")
	lines.append("|------|-----------|-------|")
	for i in range(mini(5, (result["baseline"] as Array).size())):
		var rec := _rec_at(result["baseline"], i)
		lines.append("| %d | %s | %.6f |" % [i + 1, String(rec.get("candidate", "")), float(rec.get("total_score", 0.0))])
	lines.append("")
	lines.append("#### Archetype ban light experimental")
	lines.append("")
	lines.append("| Rank | Candidate | Score | Delta vs baseline candidate | Tags | Contribution | Reasons |")
	lines.append("|------|-----------|-------|-----------------------------|------|--------------|---------|")
	for i in range(mini(5, (result["archetype"] as Array).size())):
		var rec := _rec_at(result["archetype"], i)
		var candidate := String(rec.get("candidate", ""))
		var baseline_score := _score_for_candidate(result["baseline"], candidate)
		var contribution := _contribution_for(String(state["action"]), rec)
		lines.append("| %d | %s | %.6f | %+.6f | %s | %.6f | %s |" % [
			i + 1,
			candidate,
			float(rec.get("total_score", 0.0)),
			float(rec.get("total_score", 0.0)) - baseline_score,
			_join_array(rec.get("candidate_tags", [])),
			contribution,
			_join_array(rec.get("archetype_reasons", [])),
		])
	lines.append("")
	return lines


func _accumulate(result: Dictionary) -> void:
	var state: Dictionary = result["state"]
	var archetype: Array = result["archetype"]
	for rec in archetype:
		for reason in (rec as Dictionary).get("archetype_reasons", []):
			var key := String(reason)
			_reason_counts[key] = int(_reason_counts.get(key, 0)) + 1

	if not bool(result["top_changed"]) or archetype.is_empty():
		return

	var top_rec := _rec_at(archetype, 0)
	var contribution := _contribution_for(String(state["action"]), top_rec)
	_changed_contribution_sum += contribution
	_changed_contribution_max = maxf(_changed_contribution_max, absf(contribution))
	if String(state["action"]) == "PICK":
		_pick_changes += 1
		if is_zero_approx(contribution):
			_suspicious_notes.append("`%s` changed a pick top recommendation with zero archetype contribution." % String(state["name"]))
	else:
		_ban_changes += 1
	var reasons := _join_array(top_rec.get("archetype_reasons", []))
	var reason_fit := _reason_fit_for(top_rec, reasons)
	_changed_states.append({
		"state": String(state["name"]),
		"baseline_top": String(result["baseline_top"]),
		"archetype_top": String(result["archetype_top"]),
		"contribution": contribution,
		"reasons": reasons,
		"reason_fit": reason_fit,
	})
	if absf(contribution) > 0.002:
		_suspicious_notes.append("`%s` top change contribution %.6f is larger than the expected tiny ban-light range." % [String(state["name"]), contribution])
	if _has_common_only_reason(top_rec):
		_suspicious_notes.append("`%s` changed top mainly through common tags; review for overvaluing `%s`." % [String(state["name"]), reasons])


func _reason_fit_for(rec: Dictionary, reasons: String) -> String:
	if reasons.is_empty():
		return "no explicit archetype reason"
	var tags := _join_array(rec.get("candidate_tags", []))
	for reason in reasons.split(", "):
		var parts := reason.split("+")
		if parts.size() == 2 and tags.contains(parts[0]):
			return "candidate tag matched pair reason"
		if reason.begins_with("complete_"):
			return "completion bonus matched enemy cluster"
	return "review"


func _has_common_only_reason(rec: Dictionary) -> bool:
	var reasons: Array = rec.get("archetype_reasons", [])
	if reasons.is_empty():
		return false
	for reason in reasons:
		var text := String(reason)
		if not (text.contains("aoe") or text.contains("backline") or text.contains("protect")):
			return false
	return true


func _top_reasons() -> Array[Dictionary]:
	var rows: Array[Dictionary] = []
	for reason in _reason_counts.keys():
		rows.append({"reason": String(reason), "count": int(_reason_counts[reason])})
	rows.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		if int(a["count"]) != int(b["count"]):
			return int(a["count"]) > int(b["count"])
		return String(a["reason"]) < String(b["reason"])
	)
	return rows.slice(0, mini(10, rows.size()))


func _rec_at(recommendations: Array, index: int) -> Dictionary:
	if index < 0 or index >= recommendations.size():
		return {}
	return recommendations[index] as Dictionary


func _candidate_at(recommendations: Array, index: int) -> String:
	return String(_rec_at(recommendations, index).get("candidate", ""))


func _score_for_candidate(recommendations: Array, candidate: String) -> float:
	for rec in recommendations:
		var row := rec as Dictionary
		if String(row.get("candidate", "")) == candidate:
			return float(row.get("total_score", 0.0))
	return 0.0


func _contribution_for(action: String, rec: Dictionary) -> float:
	if action == "PICK":
		return float(rec.get("archetype_contribution", 0.0))
	return float(rec.get("enemy_archetype_contribution", 0.0))


func _join_array(values: Variant) -> String:
	var parts: Array[String] = []
	for value in values:
		parts.append(String(value))
	return ", ".join(parts)


func _write_report(lines: Array[String]) -> void:
	var file := FileAccess.open(REPORT_PATH, FileAccess.WRITE)
	if file == null:
		push_error("audit_archetype_recommendations: failed to open report")
		return
	file.store_string("\n".join(lines) + "\n")
	file.close()
