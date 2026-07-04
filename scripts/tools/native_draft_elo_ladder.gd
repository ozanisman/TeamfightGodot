extends SceneTree

## Computes Elo ratings for draft strategies from harness draft-summary CSV output.
##
## Usage:
##   godot --headless --script res://scripts/tools/native_draft_elo_ladder.gd \
##     -- --draft-summary=res://model_stats/native_draft_validation_drafts.csv \
##        --strategies=native_full,native_softmax,random,base_power_only
##
## Self-test:
##   godot --headless --script res://scripts/tools/native_draft_elo_ladder.gd -- --self-test

const DraftValidationCsvScript := preload("res://scripts/tools/draft_validation_csv.gd")
const DraftEloRatingScript := preload("res://scripts/tools/draft_elo_rating.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

var _draft_summary_path: String = ""
var _strategy_names: Array[String] = []
var _initial_rating: float = DraftEloRatingScript.DEFAULT_INITIAL_RATING
var _k_factor: float = DraftEloRatingScript.DEFAULT_K_FACTOR
var _iterations: int = 10
var _output_csv_path: String = "res://model_stats/native_draft_elo_ladder.csv"
var _output_path: String = "res://logs/native_draft_elo_ladder_report.md"
var _self_test: bool = false


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _parse_strategy_names(s: String) -> Array[String]:
	var out: Array[String] = []
	for part in s.split(","):
		var trimmed: String = part.strip_edges()
		if not trimmed.is_empty():
			out.append(trimmed)
	return out


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	_self_test = OS.get_cmdline_user_args().has("--self-test")
	if _self_test:
		var ok: bool = DraftEloRatingScript.run_self_test()
		print("native_draft_elo_ladder self-test: %s" % ("PASS" if ok else "FAIL"))
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 0 if ok else 1)
		return

	_draft_summary_path = _extract_argument("--draft-summary=", "")
	_output_csv_path = _extract_argument("--output-csv=", "res://model_stats/native_draft_elo_ladder.csv")
	_output_path = _extract_argument("--output=", "res://logs/native_draft_elo_ladder_report.md")
	_initial_rating = _parse_float_arg("--initial-rating=", str(DraftEloRatingScript.DEFAULT_INITIAL_RATING))
	_k_factor = _parse_float_arg("--k-factor=", str(DraftEloRatingScript.DEFAULT_K_FACTOR))
	_iterations = _parse_int_arg("--iterations=", 10, 1)

	var strategies_arg: String = _extract_argument("--strategies=", "")
	if not strategies_arg.is_empty():
		_strategy_names = _parse_strategy_names(strategies_arg)

	if _draft_summary_path.is_empty():
		push_error("native_draft_elo_ladder: --draft-summary required")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var drafts: Array[Dictionary] = DraftValidationCsvScript.load_draft_summaries(_draft_summary_path)
	if drafts.is_empty():
		push_error("native_draft_elo_ladder: no draft data found in %s" % _draft_summary_path)
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	if _strategy_names.is_empty():
		_strategy_names = _detect_strategies(drafts)

	var ladder: Dictionary = DraftEloRatingScript.compute_ladder(
		drafts, _strategy_names, _initial_rating, _k_factor, _iterations
	)
	if not _write_csv(ladder):
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not _write_report(ladder, drafts.size()):
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	print("native_draft_elo_ladder: wrote %s and %s" % [_output_csv_path, _output_path])
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)


func _parse_int_arg(prefix: String, default_value: int, min_value: int) -> int:
	var raw: String = _extract_argument(prefix, str(default_value))
	if not raw.is_valid_int():
		push_warning("native_draft_elo_ladder: invalid int for %s (%s), using default %s" % [prefix, raw, default_value])
		raw = str(default_value)
	return maxi(min_value, int(raw))


func _parse_float_arg(prefix: String, default_value: String) -> float:
	var raw: String = _extract_argument(prefix, default_value)
	if not raw.is_valid_float():
		push_warning("native_draft_elo_ladder: invalid float for %s (%s), using default %s" % [prefix, raw, default_value])
		raw = default_value
	return float(raw)


func _detect_strategies(drafts: Array[Dictionary]) -> Array[String]:
	var found: Dictionary = {}
	for draft in drafts:
		found[String(draft["blue_strategy"])] = true
		found[String(draft["red_strategy"])] = true
	var out: Array[String] = []
	for key in found.keys():
		out.append(String(key))
	out.sort()
	return out


func _rank_strategies(ratings: Dictionary) -> Array[String]:
	var ranked: Array[String] = _strategy_names.duplicate()
	ranked.sort_custom(func(a: String, b: String) -> bool:
		if ratings[a] != ratings[b]:
			return ratings[a] > ratings[b]
		return a < b
	)
	return ranked


func _write_csv(ladder: Dictionary) -> bool:
	var ratings: Dictionary = ladder["ratings"]
	var stats: Dictionary = ladder["stats"]
	var ranked: Array[String] = _rank_strategies(ratings)

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

	var f := FileAccess.open(ProjectSettings.globalize_path(_output_csv_path), FileAccess.WRITE)
	if f == null:
		push_error("native_draft_elo_ladder: could not open output csv %s" % _output_csv_path)
		return false
	f.store_string("\n".join(rows) + "\n")
	f.close()
	return true


func _write_report(ladder: Dictionary, draft_count: int) -> bool:
	var ratings: Dictionary = ladder["ratings"]
	var stats: Dictionary = ladder["stats"]
	var pairings: Dictionary = ladder["pairings"]
	var ranked: Array[String] = _rank_strategies(ratings)

	var lines: Array[String] = []
	lines.append("# Native Draft Elo Ladder Report")
	lines.append("")
	lines.append("Generated: " + Time.get_datetime_string_from_system())
	lines.append("")
	lines.append("Input: %s (%d draft rows)" % [_draft_summary_path, draft_count])
	lines.append("Initial rating: %.1f | K-factor: %.1f | Iterations: %d" % [_initial_rating, _k_factor, _iterations])
	lines.append("")
	lines.append("> Ratings are as-played (blue vs red as recorded). Self-play pairings are excluded from Elo updates and aggregate stats because side bias is not a strength signal. For fair coverage, put every strategy in both `--blue-strategies` and `--red-strategies` when running the harness.")
	lines.append("")
	lines.append("## Rankings")
	lines.append("")
	lines.append("| Rank | Strategy | Elo | Games | Wins | Losses | Draws | Score rate |")
	lines.append("| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |")
	for i in range(ranked.size()):
		var strategy: String = ranked[i]
		var s: Dictionary = stats[strategy]
		var games: int = int(s["games"])
		var wins: int = int(s["wins"])
		var losses: int = int(s["losses"])
		var draws: int = int(s["draws"])
		var score_rate: float = DraftEloRatingScript.stats_score_rate(s)
		lines.append("| %d | %s | %.1f | %d | %d | %d | %d | %.1f%% |" % [
			i + 1, strategy, ratings[strategy], games, wins, losses, draws, score_rate * 100.0
		])
	lines.append("")
	lines.append("Score rate = (wins + 0.5·draws) / games across non-self-play pairings only.")
	lines.append("")
	lines.append("## Pairing Matrix (blue score %: wins + 0.5·draws)")
	lines.append("")
	lines.append("| Blue \\ Red | " + " | ".join(ranked) + " |")
	lines.append("| --- | " + " | ".join(ranked.map(func(_s: String) -> String: return "---:")) + " |")
	for blue in ranked:
		var cells: Array[String] = [blue]
		for red in ranked:
			if blue == red:
				var self_key: String = "%s|%s" % [blue, red]
				if pairings.has(self_key):
					var self_pct: float = DraftEloRatingScript.pairing_blue_score_rate(pairings[self_key]) * 100.0
					cells.append("%.1f%%*" % self_pct)
				else:
					cells.append("—")
				continue
			var key: String = "%s|%s" % [blue, red]
			if pairings.has(key):
				var p: Dictionary = pairings[key]
				var pct: float = DraftEloRatingScript.pairing_blue_score_rate(p) * 100.0
				cells.append("%.1f%%" % pct)
			else:
				cells.append("n/a")
		lines.append("| " + " | ".join(cells) + " |")
	lines.append("")
	lines.append("*Self-play cells show pooled blue score rate for reference only; excluded from Elo and aggregate stats.")
	lines.append("")
	lines.append("## Overall")
	lines.append("STATUS: OK")

	var f := FileAccess.open(ProjectSettings.globalize_path(_output_path), FileAccess.WRITE)
	if f == null:
		push_error("native_draft_elo_ladder: could not open report %s" % _output_path)
		return false
	f.store_string("\n".join(lines) + "\n")
	f.close()
	return true
