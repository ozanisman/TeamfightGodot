extends SceneTree

const ChampionCatalog := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

const STRATEGY_NATIVE := 0
const PROFILES := {
	"archetype_full": 4,
	"archetype_light": 5,
	"archetype_pick_light": 6,
	"archetype_ban_light": 7,
}

const DRAFT_SEQUENCE := [
	{"side": "B", "action": "BAN"},
	{"side": "R", "action": "BAN"},
	{"side": "B", "action": "BAN"},
	{"side": "R", "action": "BAN"},
	{"side": "B", "action": "BAN"},
	{"side": "R", "action": "BAN"},
	{"side": "B", "action": "PICK"},
	{"side": "R", "action": "PICK"},
	{"side": "R", "action": "PICK"},
	{"side": "B", "action": "PICK"},
	{"side": "B", "action": "PICK"},
	{"side": "R", "action": "PICK"},
	{"side": "R", "action": "BAN"},
	{"side": "B", "action": "BAN"},
	{"side": "R", "action": "BAN"},
	{"side": "B", "action": "BAN"},
	{"side": "R", "action": "PICK"},
	{"side": "B", "action": "PICK"},
	{"side": "B", "action": "PICK"},
	{"side": "R", "action": "PICK"},
]

var _backend: RefCounted = null
var _stats_dir := "res://stats_output_100k"
var _stats := {}


func _init() -> void:
	call_deferred("_run")


func _new_stats() -> Dictionary:
	return {
		"count": 0,
		"sum": 0.0,
		"max": -INF,
		"min": INF,
		"top_changes": 0,
		"rank_flips": [],
		"reasons": {},
	}


func _run() -> void:
	_backend = NativeSimulationBackendScript.new()
	if _backend == null or not _backend.is_available():
		push_error("diagnose_archetype_scoring: native backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	for profile_name in PROFILES.keys():
		_stats[profile_name] = _new_stats()

	var available: Array = ChampionCatalog.get_champion_ids()
	var blue_picks: Array = []
	var red_picks: Array = []
	var blue_bans: Array = []
	var red_bans: Array = []

	for step in range(DRAFT_SEQUENCE.size()):
		var entry: Dictionary = DRAFT_SEQUENCE[step]
		var side := String(entry["side"])
		var action := String(entry["action"])
		var allies := blue_picks if side == "B" else red_picks
		var enemies := red_picks if side == "B" else blue_picks
		var native_recs := _recommend(action, STRATEGY_NATIVE, available, allies, enemies, step, side)
		if native_recs.is_empty():
			continue

		for profile_name in PROFILES.keys():
			var profile_recs := _recommend(action, int(PROFILES[profile_name]), available, allies, enemies, step, side)
			_accumulate(profile_name, action, step, native_recs, profile_recs)

		var selected := StringName(native_recs[0].candidate)
		available.erase(selected)
		if action == "PICK":
			if side == "B":
				blue_picks.append(selected)
			else:
				red_picks.append(selected)
		else:
			if side == "B":
				blue_bans.append(selected)
			else:
				red_bans.append(selected)

	_write_report()
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)


func _recommend(action: String, strategy: int, available: Array, allies: Array, enemies: Array, step: int, side: String) -> Array:
	if action == "PICK":
		return _backend.get_draft_ai_pick_recommendations(_stats_dir, available, allies, enemies, available.size(), step, strategy)
	return _backend.get_draft_ai_ban_recommendations(_stats_dir, available, allies, enemies, available.size(), step, "blue" if side == "B" else "red", {}, strategy)


func _accumulate(profile_name: String, action: String, step: int, native_recs: Array, profile_recs: Array) -> void:
	if profile_recs.is_empty():
		return
	var stat: Dictionary = _stats[profile_name]
	if String(native_recs[0].candidate) != String(profile_recs[0].candidate):
		stat["top_changes"] = int(stat["top_changes"]) + 1

	var native_ranks := {}
	for i in range(native_recs.size()):
		native_ranks[String(native_recs[i].candidate)] = i + 1

	for i in range(profile_recs.size()):
		var rec: Dictionary = profile_recs[i]
		var contribution := _contribution_for(action, rec)
		stat["count"] = int(stat["count"]) + 1
		stat["sum"] = float(stat["sum"]) + contribution
		stat["max"] = maxf(float(stat["max"]), contribution)
		stat["min"] = minf(float(stat["min"]), contribution)

		for reason in rec.get("archetype_reasons", []):
			var reason_key := String(reason)
			stat["reasons"][reason_key] = int(stat["reasons"].get(reason_key, 0)) + 1

		var candidate := String(rec.candidate)
		if native_ranks.has(candidate):
			var native_rank := int(native_ranks[candidate])
			var profile_rank := i + 1
			var delta := native_rank - profile_rank
			if abs(delta) > 0:
				stat["rank_flips"].append({
					"abs_delta": abs(delta),
					"delta": delta,
					"profile_rank": profile_rank,
					"native_rank": native_rank,
					"candidate": candidate,
					"action": action,
					"step": step,
					"contribution": contribution,
					"reasons": rec.get("archetype_reasons", []),
				})


func _contribution_for(action: String, rec: Dictionary) -> float:
	if action == "PICK":
		return float(rec.get("archetype_contribution", 0.0))
	return float(rec.get("enemy_archetype_contribution", 0.0))


func _top_rank_flips(flips: Array) -> Array:
	flips.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		if int(a["abs_delta"]) != int(b["abs_delta"]):
			return int(a["abs_delta"]) > int(b["abs_delta"])
		return float(a["contribution"]) > float(b["contribution"])
	)
	return flips.slice(0, mini(10, flips.size()))


func _top_reasons(reason_counts: Dictionary) -> Array:
	var rows: Array = []
	for reason in reason_counts.keys():
		rows.append({"reason": String(reason), "count": int(reason_counts[reason])})
	rows.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		if int(a["count"]) != int(b["count"]):
			return int(a["count"]) > int(b["count"])
		return String(a["reason"]) < String(b["reason"])
	)
	return rows.slice(0, mini(10, rows.size()))


func _write_report() -> void:
	var lines: Array[String] = []
	lines.append("# Archetype Scoring Diagnostic")
	lines.append("")
	lines.append("Baseline recommendations are `native`; profile recommendations use the same draft states and all available candidates.")
	lines.append("")

	for profile_name in PROFILES.keys():
		var stat: Dictionary = _stats[profile_name]
		var count := int(stat["count"])
		var average := 0.0 if count == 0 else float(stat["sum"]) / float(count)
		lines.append("## " + profile_name)
		lines.append("")
		lines.append("- candidates evaluated: %d" % count)
		lines.append("- average archetype contribution: %.6f" % average)
		lines.append("- max archetype contribution: %.6f" % float(stat["max"]))
		lines.append("- min archetype contribution: %.6f" % float(stat["min"]))
		lines.append("- top recommendation changes: %d of %d draft decisions" % [int(stat["top_changes"]), DRAFT_SEQUENCE.size()])
		lines.append("")
		lines.append("### Top 10 Biggest Rank Flips")
		lines.append("")
		lines.append("| Step | Action | Candidate | Native rank | Profile rank | Delta | Contribution | Reasons |")
		lines.append("|------|--------|-----------|-------------|--------------|-------|--------------|---------|")
		for flip in _top_rank_flips(stat["rank_flips"]):
			lines.append("| %d | %s | %s | %d | %d | %+d | %.6f | %s |" % [
				int(flip["step"]),
				String(flip["action"]),
				String(flip["candidate"]),
				int(flip["native_rank"]),
				int(flip["profile_rank"]),
				int(flip["delta"]),
				float(flip["contribution"]),
				", ".join(PackedStringArray(flip["reasons"])),
			])
		lines.append("")
		lines.append("### Most Common Archetype Reasons")
		lines.append("")
		lines.append("| Reason | Count |")
		lines.append("|--------|-------|")
		for row in _top_reasons(stat["reasons"]):
			lines.append("| %s | %d |" % [String(row["reason"]), int(row["count"])])
		lines.append("")

	var file := FileAccess.open("archetype_scoring_diagnostic_report.md", FileAccess.WRITE)
	if file == null:
		push_error("Failed to write archetype_scoring_diagnostic_report.md")
		return
	file.store_string("\n".join(lines) + "\n")
	file.close()
	print("Archetype scoring diagnostic written to archetype_scoring_diagnostic_report.md")
