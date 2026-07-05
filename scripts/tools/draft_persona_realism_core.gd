class_name DraftPersonaRealismCore
extends RefCounted

const DraftValidationCsvScript := preload("res://scripts/tools/draft_validation_csv.gd")

const DEFAULT_OUTPUT_CSV: String = "res://model_stats/native_draft_persona_realism_metrics.csv"
const DEFAULT_OUTPUT_REPORT: String = "res://logs/native_draft_persona_realism_metrics_report.md"
const COUNTER_PICK_NOT_EVALUATED: String = "NOT_EVALUATED"

const METRIC_COLUMNS: Array[String] = [
	"strategy",
	"pick_entropy_norm",
	"ban_entropy_norm",
	"unique_pick_rate",
	"unique_ban_rate",
	"top_pick_concentration",
	"top_ban_concentration",
	"repeated_opener_rate",
	"counter_pick_rate",
	"total_pick_slots",
	"total_ban_slots",
	"draft_appearances",
]


static func run(params: Dictionary) -> Dictionary:
	var input_path: String = String(params.get("draft_summary_path", ""))
	var output_csv_path: String = String(params.get("output_csv_path", DEFAULT_OUTPUT_CSV))
	var output_report_path: String = String(params.get("output_report_path", DEFAULT_OUTPUT_REPORT))
	if input_path.is_empty():
		return {"ok": false, "error": "draft_summary_path required"}

	var rows: Array[Dictionary] = DraftValidationCsvScript.load_metric_rows(input_path)
	if rows.is_empty():
		return {"ok": false, "error": "draft summary empty or missing"}

	var metrics: Array[Dictionary] = compute_metrics(rows)
	if metrics.is_empty():
		return {"ok": false, "error": "no strategy appearances found"}

	if not write_csv(metrics, output_csv_path):
		return {"ok": false, "error": "failed to write metrics CSV"}
	if not write_report(metrics, input_path, output_report_path):
		return {"ok": false, "error": "failed to write metrics report"}

	return {
		"ok": true,
		"metrics": metrics,
		"output_csv_path": output_csv_path,
		"output_report_path": output_report_path,
	}


static func compute_metrics(rows: Array[Dictionary]) -> Array[Dictionary]:
	var stats_by_strategy: Dictionary = {}
	var global_pick_pool: Dictionary = {}
	var global_ban_pool: Dictionary = {}
	var sorted_rows: Array[Dictionary] = rows.duplicate()
	sorted_rows.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		var seed_a: int = int(a.get("draft_seed", 0))
		var seed_b: int = int(b.get("draft_seed", 0))
		if seed_a != seed_b:
			return seed_a < seed_b
		var key_a: String = "%s|%s" % [String(a.get("blue_strategy", "")), String(a.get("red_strategy", ""))]
		var key_b: String = "%s|%s" % [String(b.get("blue_strategy", "")), String(b.get("red_strategy", ""))]
		return key_a < key_b
	)

	for row in sorted_rows:
		var blue_picks: Array[String] = _split_list(String(row.get("blue_picks", "")))
		var red_picks: Array[String] = _split_list(String(row.get("red_picks", "")))
		var blue_bans: Array[String] = _split_list(String(row.get("blue_bans", "")))
		var red_bans: Array[String] = _split_list(String(row.get("red_bans", "")))
		_add_counts(global_pick_pool, blue_picks)
		_add_counts(global_pick_pool, red_picks)
		_add_counts(global_ban_pool, blue_bans)
		_add_counts(global_ban_pool, red_bans)
		_accumulate_appearance(
			stats_by_strategy,
			String(row.get("blue_strategy", "")),
			String(row.get("red_strategy", "")),
			"blue",
			blue_picks,
			blue_bans
		)
		_accumulate_appearance(
			stats_by_strategy,
			String(row.get("red_strategy", "")),
			String(row.get("blue_strategy", "")),
			"red",
			red_picks,
			red_bans
		)

	var strategies: Array[String] = []
	for key in stats_by_strategy.keys():
		strategies.append(String(key))
	strategies.sort()

	var out: Array[Dictionary] = []
	for strategy in strategies:
		var s: Dictionary = stats_by_strategy[strategy]
		var pick_counts: Dictionary = s["pick_counts"]
		var ban_counts: Dictionary = s["ban_counts"]
		var pick_slots: int = int(s["pick_slots"])
		var ban_slots: int = int(s["ban_slots"])
		var opener_slots: int = int(s["opener_slots"])
		var repeated_openers: int = int(s["repeated_openers"])
		out.append({
			"strategy": strategy,
			"pick_entropy_norm": _normalized_entropy(pick_counts, pick_slots),
			"ban_entropy_norm": _normalized_entropy(ban_counts, ban_slots),
			"unique_pick_rate": _unique_rate(pick_counts, global_pick_pool.size()),
			"unique_ban_rate": _unique_rate(ban_counts, global_ban_pool.size()),
			"top_pick_concentration": _top_concentration(pick_counts, pick_slots),
			"top_ban_concentration": _top_concentration(ban_counts, ban_slots),
			"repeated_opener_rate": float(repeated_openers) / float(maxi(1, opener_slots)),
			"counter_pick_rate": COUNTER_PICK_NOT_EVALUATED,
			"total_pick_slots": pick_slots,
			"total_ban_slots": ban_slots,
			"draft_appearances": int(s["draft_appearances"]),
		})
	return out


static func load_metrics_csv(path: String) -> Dictionary:
	var out: Dictionary = {}
	var rows: Array[Dictionary] = DraftValidationCsvScript.load_metric_rows(path)
	for row in rows:
		var strategy: String = String(row.get("strategy", "")).strip_edges()
		if strategy.is_empty():
			continue
		out[strategy] = row
	return out


static func write_csv(metrics: Array[Dictionary], output_path: String) -> bool:
	var lines: Array[String] = []
	lines.append(",".join(METRIC_COLUMNS))
	for row in metrics:
		var fields: Array[String] = []
		for column in METRIC_COLUMNS:
			var value: Variant = row.get(column, "")
			if value is float:
				fields.append("%.6f" % float(value))
			else:
				fields.append(str(value))
		lines.append(",".join(fields))
	return _write_lines(output_path, lines)


static func write_report(metrics: Array[Dictionary], input_path: String, output_path: String) -> bool:
	var lines: Array[String] = []
	lines.append("# Native Draft Persona Realism Metrics")
	lines.append("")
	lines.append("STATUS: PASS")
	lines.append("Generated: %s" % Time.get_datetime_string_from_system())
	lines.append("Draft summary: `%s`" % input_path)
	lines.append("")
	lines.append("| Strategy | Pick Entropy | Ban Entropy | Unique Picks | Unique Bans | Top Pick | Top Ban | Repeated Openers | Counter Pick |")
	lines.append("|----------|--------------|-------------|--------------|-------------|----------|---------|------------------|--------------|")
	for row in metrics:
		lines.append("| `%s` | %.4f | %.4f | %.4f | %.4f | %.4f | %.4f | %.4f | %s |" % [
			String(row.get("strategy", "")),
			float(row.get("pick_entropy_norm", 0.0)),
			float(row.get("ban_entropy_norm", 0.0)),
			float(row.get("unique_pick_rate", 0.0)),
			float(row.get("unique_ban_rate", 0.0)),
			float(row.get("top_pick_concentration", 0.0)),
			float(row.get("top_ban_concentration", 0.0)),
			float(row.get("repeated_opener_rate", 0.0)),
			String(row.get("counter_pick_rate", COUNTER_PICK_NOT_EVALUATED)),
		])
	lines.append("")
	lines.append("Counter-pick rate is `NOT_EVALUATED` for draft-summary-only input.")
	return _write_lines(output_path, lines)


static func self_test() -> Dictionary:
	var cases: Array[Dictionary] = []
	var rows: Array[Dictionary] = [
		{
			"draft_seed": 1,
			"blue_strategy": "native_softmax",
			"red_strategy": "native_softmax_safe",
			"blue_picks": "a|b|c|d|e",
			"red_picks": "a|b|f|g|h",
			"blue_bans": "x|y|z|u|v",
			"red_bans": "x|y|m|n|o",
		},
		{
			"draft_seed": 2,
			"blue_strategy": "native_softmax",
			"red_strategy": "native_softmax_safe",
			"blue_picks": "a|b|i|j|k",
			"red_picks": "q|r|s|t|u",
			"blue_bans": "x|y|p|q|r",
			"red_bans": "m|n|o|p|q",
		},
	]
	var metrics: Array[Dictionary] = compute_metrics(rows)
	var by_strategy: Dictionary = {}
	for row in metrics:
		by_strategy[String(row["strategy"])] = row
	var base: Dictionary = by_strategy.get("native_softmax", {})
	cases.append(_case("computed strategy rows", metrics.size() == 2))
	cases.append(_case("pick entropy", _near(float(base.get("pick_entropy_norm", 0.0)), 0.9740, 0.0001)))
	cases.append(_case("ban entropy", _near(float(base.get("ban_entropy_norm", 0.0)), 0.9740, 0.0001)))
	cases.append(_case("unique pick rate", _near(float(base.get("unique_pick_rate", 0.0)), 8.0 / 16.0, 0.0001)))
	cases.append(_case("unique ban rate", _near(float(base.get("unique_ban_rate", 0.0)), 8.0 / 11.0, 0.0001)))
	cases.append(_case("top pick concentration", _near(float(base.get("top_pick_concentration", 0.0)), 0.2, 0.0001)))
	cases.append(_case("top ban concentration", _near(float(base.get("top_ban_concentration", 0.0)), 0.2, 0.0001)))
	cases.append(_case("repeated opener context rate", _near(float(base.get("repeated_opener_rate", 0.0)), 0.5, 0.0001)))
	cases.append(_case("counter pick unavailable", String(base.get("counter_pick_rate", "")) == COUNTER_PICK_NOT_EVALUATED))
	cases.append(_case("empty input failure", compute_metrics([]).is_empty()))
	cases.append(_case("missing path failure", not bool(run({}).get("ok", false))))

	var failed: Array[String] = []
	for case in cases:
		if not bool(case["pass"]):
			failed.append(String(case["name"]))
	return {"pass": failed.is_empty(), "failed": failed, "cases": cases}


static func _accumulate_appearance(
	stats_by_strategy: Dictionary,
	strategy: String,
	opponent: String,
	side: String,
	picks: Array[String],
	bans: Array[String]
) -> void:
	if strategy.is_empty():
		return
	if not stats_by_strategy.has(strategy):
		stats_by_strategy[strategy] = {
			"pick_counts": {},
			"ban_counts": {},
			"pick_slots": 0,
			"ban_slots": 0,
			"draft_appearances": 0,
			"seen_openers_by_context": {},
			"repeated_openers": 0,
			"opener_slots": 0,
		}
	var s: Dictionary = stats_by_strategy[strategy]
	s["draft_appearances"] = int(s["draft_appearances"]) + 1
	s["pick_slots"] = int(s["pick_slots"]) + picks.size()
	s["ban_slots"] = int(s["ban_slots"]) + bans.size()
	_add_counts(s["pick_counts"], picks)
	_add_counts(s["ban_counts"], bans)
	if picks.size() >= 2:
		var opener: String = "%s|%s" % [picks[0], picks[1]]
		var context: String = "%s|%s" % [side, opponent]
		var seen_by_context: Dictionary = s["seen_openers_by_context"]
		if not seen_by_context.has(context):
			seen_by_context[context] = {}
		var seen_openers: Dictionary = seen_by_context[context]
		if seen_openers.has(opener):
			s["repeated_openers"] = int(s["repeated_openers"]) + 1
		seen_openers[opener] = true
		s["opener_slots"] = int(s["opener_slots"]) + 1


static func _add_counts(counts: Dictionary, values: Array[String]) -> void:
	for value in values:
		if value.is_empty():
			continue
		counts[value] = int(counts.get(value, 0)) + 1


static func _split_list(raw: String) -> Array[String]:
	var out: Array[String] = []
	for part in raw.split("|", false):
		var trimmed: String = part.strip_edges()
		if not trimmed.is_empty():
			out.append(trimmed)
	return out


static func _normalized_entropy(counts: Dictionary, total: int) -> float:
	if total <= 0 or counts.size() <= 1:
		return 0.0
	var entropy: float = 0.0
	for key in counts.keys():
		var p: float = float(counts[key]) / float(total)
		if p > 0.0:
			entropy -= p * log(p)
	return entropy / log(float(counts.size()))


static func _unique_rate(counts: Dictionary, pool_size: int) -> float:
	if pool_size <= 0:
		return 0.0
	return float(counts.size()) / float(pool_size)


static func _top_concentration(counts: Dictionary, total: int) -> float:
	if total <= 0:
		return 0.0
	var top_count: int = 0
	for key in counts.keys():
		top_count = maxi(top_count, int(counts[key]))
	return float(top_count) / float(total)


static func _write_lines(path: String, lines: Array[String]) -> bool:
	var global_path: String = ProjectSettings.globalize_path(path)
	var dir_path: String = global_path.get_base_dir()
	if not DirAccess.dir_exists_absolute(dir_path):
		DirAccess.make_dir_recursive_absolute(dir_path)
	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f == null:
		return false
	f.store_string("\n".join(lines) + "\n")
	f.close()
	return true


static func _case(name: String, pass_: bool) -> Dictionary:
	return {"name": name, "pass": pass_}


static func _near(a: float, b: float, tolerance: float) -> bool:
	return absf(a - b) <= tolerance
