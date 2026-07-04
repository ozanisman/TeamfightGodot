class_name DraftEloGateCore
extends RefCounted

## In-process Elo ladder + ordering gate (native_draft_elo_ladder.gd + native_draft_elo_gate.gd core).

const DraftValidationCsvScript := preload("res://scripts/tools/draft_validation_csv.gd")
const DraftEloRatingScript := preload("res://scripts/tools/draft_elo_rating.gd")

const DEFAULT_MIN_GAP: float = 10.0

const DEFAULT_ORDERING: Array[Dictionary] = [
	{"higher": "native_full", "lower": "random"},
	{"higher": "native_softmax", "lower": "random"},
]


static func run_ladder_and_gate(params: Dictionary) -> Dictionary:
	var draft_summary_path: String = String(params.get("draft_summary_path", ""))
	var ladder_csv_path: String = String(params.get("ladder_csv_path", "res://model_stats/native_draft_elo_ladder.csv"))
	var strategy_names: Array[String] = _coerce_strategy_names(params.get("strategies", []))
	var initial_rating: float = float(params.get("initial_rating", DraftEloRatingScript.DEFAULT_INITIAL_RATING))
	var k_factor: float = float(params.get("k_factor", DraftEloRatingScript.DEFAULT_K_FACTOR))
	var iterations: int = maxi(1, int(params.get("iterations", 10)))
	var min_gap: float = float(params.get("min_gap", DEFAULT_MIN_GAP))
	var ordering: Array[Dictionary] = params.get("ordering", DEFAULT_ORDERING.duplicate(true))
	var require_fresh_ladder: bool = bool(params.get("require_fresh_ladder", true))

	if draft_summary_path.is_empty():
		return {"pass": false, "errors": ["draft_summary_path required"], "checks": []}

	var drafts: Array[Dictionary] = DraftValidationCsvScript.load_draft_summaries(draft_summary_path)
	if drafts.is_empty():
		return {"pass": false, "errors": ["no draft data found"], "checks": []}

	if strategy_names.is_empty():
		strategy_names = _detect_strategies(drafts)

	var ladder: Dictionary = DraftEloRatingScript.compute_ladder(
		drafts, strategy_names, initial_rating, k_factor, iterations
	)
	if not write_ladder_csv(ladder, strategy_names, ladder_csv_path):
		return {"pass": false, "errors": ["failed to write ladder CSV"], "checks": []}

	var freshness_errors: Array[String] = []
	if require_fresh_ladder:
		freshness_errors = _check_input_freshness(ladder_csv_path, draft_summary_path)

	var ratings: Dictionary = _load_ratings_from_ladder(ladder)
	var checks: Array[Dictionary] = _run_ordering_checks(ratings, ordering, min_gap)
	var overall_pass: bool = freshness_errors.is_empty()
	for check in checks:
		if not check["pass"]:
			overall_pass = false
			break

	return {
		"pass": overall_pass,
		"errors": freshness_errors,
		"checks": checks,
		"ratings": ratings,
		"ladder": ladder,
		"ladder_csv_path": ladder_csv_path,
		"draft_count": drafts.size(),
	}


static func _coerce_strategy_names(raw: Variant) -> Array[String]:
	var out: Array[String] = []
	if raw is Array:
		for item in raw:
			var trimmed: String = String(item).strip_edges()
			if not trimmed.is_empty():
				out.append(trimmed)
	elif raw is String:
		for part in String(raw).split(","):
			var trimmed: String = part.strip_edges()
			if not trimmed.is_empty():
				out.append(trimmed)
	return out


static func _detect_strategies(drafts: Array[Dictionary]) -> Array[String]:
	var found: Dictionary = {}
	for draft in drafts:
		found[String(draft["blue_strategy"])] = true
		found[String(draft["red_strategy"])] = true
	var out: Array[String] = []
	for key in found.keys():
		out.append(String(key))
	out.sort()
	return out


static func _rank_strategies(ratings: Dictionary, strategy_names: Array[String]) -> Array[String]:
	var ranked: Array[String] = strategy_names.duplicate()
	ranked.sort_custom(func(a: String, b: String) -> bool:
		if ratings[a] != ratings[b]:
			return ratings[a] > ratings[b]
		return a < b
	)
	return ranked


static func write_ladder_csv(ladder: Dictionary, strategy_names: Array[String], output_csv_path: String) -> bool:
	var ratings: Dictionary = ladder["ratings"]
	var stats: Dictionary = ladder["stats"]
	var ranked: Array[String] = _rank_strategies(ratings, strategy_names)

	var rows: Array[String] = ["rank,strategy,elo,games,wins,losses,draws,score_rate"]
	for i in range(ranked.size()):
		var strategy: String = ranked[i]
		var s: Dictionary = stats[strategy]
		var games: int = int(s["games"])
		var wins: int = int(s["wins"])
		var losses: int = int(s["losses"])
		var draws: int = int(s["draws"])
		var score_rate: float = DraftEloRatingScript.stats_score_rate(s)
		rows.append("%d,%s,%.2f,%d,%d,%d,%d,%.6f" % [
			i + 1, strategy, ratings[strategy], games, wins, losses, draws, score_rate
		])

	var f := FileAccess.open(ProjectSettings.globalize_path(output_csv_path), FileAccess.WRITE)
	if f == null:
		push_error("DraftEloGateCore: could not open output csv %s" % output_csv_path)
		return false
	f.store_string("\n".join(rows) + "\n")
	f.close()
	return true


static func check_input_freshness(ladder_csv_path: String, draft_summary_path: String) -> Array[String]:
	return _check_input_freshness(ladder_csv_path, draft_summary_path)


static func run_ordering_checks(
	ratings: Dictionary,
	ordering: Array[Dictionary],
	min_gap: float
) -> Array[Dictionary]:
	return _run_ordering_checks(ratings, ordering, min_gap)


static func _check_input_freshness(ladder_csv_path: String, draft_summary_path: String) -> Array[String]:
	var errors: Array[String] = []
	var ladder_global: String = ProjectSettings.globalize_path(ladder_csv_path)
	if not FileAccess.file_exists(ladder_global):
		errors.append("ladder CSV not found: %s" % ladder_csv_path)
		return errors

	var summary_global: String = ProjectSettings.globalize_path(draft_summary_path)
	if not FileAccess.file_exists(summary_global):
		errors.append("draft summary not found: %s" % draft_summary_path)
		return errors

	var ladder_mtime: int = FileAccess.get_modified_time(ladder_global)
	var summary_mtime: int = FileAccess.get_modified_time(summary_global)
	if ladder_mtime < summary_mtime:
		errors.append(
			"ladder CSV is older than draft summary (%s < %s)" % [ladder_csv_path, draft_summary_path]
		)
	return errors


static func _load_ratings_from_ladder(ladder: Dictionary) -> Dictionary:
	return ladder.get("ratings", {}).duplicate()


static func load_ratings_csv(path: String) -> Dictionary:
	var ratings: Dictionary = {}
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		return ratings
	var header: Array = DraftValidationCsvScript.split_csv_line(f.get_line())
	var idx: Dictionary = DraftValidationCsvScript.header_index(header)
	if not idx.has("strategy") or not idx.has("elo"):
		f.close()
		return ratings
	while not f.eof_reached():
		var line: String = f.get_line()
		if line.strip_edges().is_empty():
			continue
		var fields: Array = DraftValidationCsvScript.split_csv_line(line)
		if idx["strategy"] >= fields.size() or idx["elo"] >= fields.size():
			continue
		var strategy: String = String(fields[idx["strategy"]]).strip_edges()
		var elo_str: String = String(fields[idx["elo"]]).strip_edges()
		if not elo_str.is_valid_float():
			continue
		ratings[strategy] = float(elo_str)
	f.close()
	return ratings


static func _run_ordering_checks(
	ratings: Dictionary,
	ordering: Array[Dictionary],
	min_gap: float
) -> Array[Dictionary]:
	var checks: Array[Dictionary] = []
	for constraint in ordering:
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
			pass_ = gap >= min_gap
			if not pass_:
				reason = "gap below min"
		checks.append({
			"higher": higher,
			"lower": lower,
			"higher_elo": ratings.get(higher, 0.0),
			"lower_elo": ratings.get(lower, 0.0),
			"gap": gap,
			"min_gap": min_gap,
			"pass": higher_present and lower_present and pass_,
			"reason": reason,
		})
	return checks
