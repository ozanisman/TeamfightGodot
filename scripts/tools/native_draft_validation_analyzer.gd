extends SceneTree

## Reads the per-step CSV from native_draft_validation_harness.gd (or a draft
## summary CSV) and emits grouped summary metrics. If --draft-summary is
## provided, it is used instead of inferring completed drafts from the per-step
## rows.
##
## Also emits an A/B report with per-matchup Wilson CIs and a two-proportion
## z-test when control/treatment matchups are specified.
##
## Usage:
##   godot --headless --script res://scripts/tools/native_draft_validation_analyzer.gd \
##     -- --input=res://model_stats/native_draft_validation.csv \
##     --output=res://model_stats/native_draft_validation_summary.csv \
##     --ab-output=res://model_stats/native_draft_validation_ab_report.csv
##
## Or:
##   godot --headless --script res://scripts/tools/native_draft_validation_analyzer.gd \
##     -- --draft-summary=res://model_stats/native_draft_validation_drafts.csv \
##     --output=res://model_stats/native_draft_validation_summary.csv
##
## Optional:
##     --native-strategy-names=native_full,native_softmax
##     --ab-control-blue=native_full --ab-control-red=random \
##     --ab-treatment-blue=native_full --ab-treatment-red=base_power_only \
##     --ab-mde=0.05 --ab-alpha=0.05 --ab-power=0.80

const Z_95: float = 1.959963984540054
const NORMAL_CDF_INV_ITERATIONS: int = 50

const DraftValidationCsvScript := preload("res://scripts/tools/draft_validation_csv.gd")

var _native_strategy_names: Array[String] = ["native_full"]

var _input_path: String = ""
var _draft_summary_path: String = ""
var _output_path: String = "res://model_stats/native_draft_validation_summary.csv"

var _ab_output_path: String = ""
var _ab_control_blue: String = ""
var _ab_control_red: String = ""
var _ab_treatment_blue: String = ""
var _ab_treatment_red: String = ""
var _ab_mde: float = 0.05
var _ab_alpha: float = 0.05
var _ab_power: float = 0.80


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
	_input_path = _extract_argument("--input=", "")
	_draft_summary_path = _extract_argument("--draft-summary=", "")
	_output_path = _extract_argument("--output=", "res://model_stats/native_draft_validation_summary.csv")
	_native_strategy_names = _parse_strategy_names(_extract_argument("--native-strategy-names=", "native_full"))
	_ab_output_path = _extract_argument("--ab-output=", "res://model_stats/native_draft_validation_ab_report.csv")
	_ab_control_blue = _extract_argument("--ab-control-blue=", "")
	_ab_control_red = _extract_argument("--ab-control-red=", "")
	_ab_treatment_blue = _extract_argument("--ab-treatment-blue=", "")
	_ab_treatment_red = _extract_argument("--ab-treatment-red=", "")
	_ab_mde = _parse_float_arg("--ab-mde=", "0.05", 0.001, INF)
	_ab_alpha = _parse_float_arg("--ab-alpha=", "0.05", 0.001, 0.5)
	_ab_power = _parse_float_arg("--ab-power=", "0.80", 0.5, 0.999)

	var drafts: Array[Dictionary]
	if not _draft_summary_path.is_empty():
		drafts = DraftValidationCsvScript.load_draft_summaries(_draft_summary_path)
	elif not _input_path.is_empty():
		drafts = DraftValidationCsvScript.infer_drafts_from_steps(_input_path)
	else:
		push_error("native_draft_validation_analyzer: --input or --draft-summary required")
		quit(1)
		return

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

	var ab_rows: Array[String] = _build_ab_report(drafts)
	if not ab_rows.is_empty():
		var ab_f := FileAccess.open(ProjectSettings.globalize_path(_ab_output_path), FileAccess.WRITE)
		if ab_f == null:
			push_error("native_draft_validation_analyzer: could not open A/B output %s" % _ab_output_path)
			quit(1)
			return
		ab_f.store_string("\n".join(ab_rows) + "\n")
		ab_f.close()
		print("native_draft_validation_analyzer: wrote A/B report %s (%d rows)" % [_ab_output_path, ab_rows.size() - 1])

	quit(0)


func _parse_float_arg(prefix: String, default_value: String, min_val: float, max_val: float) -> float:
	var raw: String = _extract_argument(prefix, default_value)
	if not raw.is_valid_float():
		push_warning("native_draft_validation_analyzer: invalid float for %s (%s), using default %s" % [prefix, raw, default_value])
		raw = default_value
	return clampf(float(raw), min_val, max_val)


func _split_csv_line(line: String) -> Array:
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


func _csv_field(s: String) -> String:
	if s.find(",") >= 0 or s.find("\"") >= 0 or s.find("\n") >= 0 or s.find("\r") >= 0:
		return "\"" + s.replace("\"", "\"\"") + "\""
	return s


func _format_metric_row(metric: String, grouping: String, trials: int, wins: int, losses: int, draws: int) -> String:
	var total_matches: int = wins + losses + draws
	var winrate: float = 0.0
	var drawrate: float = 0.0
	if total_matches > 0:
		winrate = float(wins) / float(total_matches)
		drawrate = float(draws) / float(total_matches)
	return "%s,%s,%d,%d,%d,%d,%d,%.6f,%.6f" % [
		_csv_field(metric), _csv_field(grouping), trials, total_matches, wins, losses, draws, winrate, drawrate
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
	var blue_is_native: bool = _native_strategy_names.has(draft["blue_strategy"])
	var red_is_native: bool = _native_strategy_names.has(draft["red_strategy"])
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


func _build_ab_report(drafts: Array[Dictionary]) -> Array[String]:
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
	rows.append(
		"metric,grouping,trials,wins,losses,decisive_winrate,ci_lower,ci_upper," +
		"control_winrate,control_ci_lower,control_ci_upper," +
		"treatment_winrate,treatment_ci_lower,treatment_ci_upper," +
		"delta,delta_ci_lower,delta_ci_upper,z,p_value,required_n_per_group"
	)

	for key in groups.keys():
		var g: Dictionary = groups[key]
		var wins: int = g["blue_wins"]
		var losses: int = g["red_wins"]
		var ci: Array = _wilson_ci(wins, losses, Z_95)
		var winrate: float = float(wins) / float(maxi(1, wins + losses))
		rows.append("matchup_winrate_ci,%s,%d,%d,%d,%.6f,%.6f,%.6f,,,,,,,,,,,," % [
			_csv_field(key), g["trials"], wins, losses, winrate, ci[0], ci[1]
		])

	if _ab_control_blue.is_empty() or _ab_control_red.is_empty():
		return rows

	var control_key: String = "blue=%s red=%s" % [_ab_control_blue, _ab_control_red]
	if not groups.has(control_key):
		push_warning("native_draft_validation_analyzer: control matchup not found: %s" % control_key)
		return rows

	var control: Dictionary = groups[control_key]
	var control_wins: int = control["blue_wins"]
	var control_losses: int = control["red_wins"]
	var control_n: int = control_wins + control_losses
	var control_p: float = float(control_wins) / float(maxi(1, control_n))
	var control_ci: Array = _wilson_ci(control_wins, control_losses, Z_95)

	if _ab_treatment_blue.is_empty() or _ab_treatment_red.is_empty():
		return rows

	var treatment_key: String = "blue=%s red=%s" % [_ab_treatment_blue, _ab_treatment_red]
	if not groups.has(treatment_key):
		push_warning("native_draft_validation_analyzer: treatment matchup not found: %s" % treatment_key)
		return rows

	var treatment: Dictionary = groups[treatment_key]
	var treatment_wins: int = treatment["blue_wins"]
	var treatment_losses: int = treatment["red_wins"]
	var treatment_n: int = treatment_wins + treatment_losses
	var treatment_p: float = float(treatment_wins) / float(maxi(1, treatment_n))
	var treatment_ci: Array = _wilson_ci(treatment_wins, treatment_losses, Z_95)

	if control_n <= 0 or treatment_n <= 0:
		push_warning("native_draft_validation_analyzer: A/B comparison requires decisive matches in both control and treatment")
		return rows

	var delta: float = treatment_p - control_p
	var se_delta: float = sqrt(
		control_p * (1.0 - control_p) / float(maxi(1, control_n)) +
		treatment_p * (1.0 - treatment_p) / float(maxi(1, treatment_n))
	)
	var delta_ci_lower: float = delta - Z_95 * se_delta
	var delta_ci_upper: float = delta + Z_95 * se_delta

	var z_test: Dictionary = _two_proportion_z_test(control_wins, control_n, treatment_wins, treatment_n)
	var z: float = z_test["z"]
	var p_value: float = z_test["p_value"]

	var required_n: int = _required_n_per_group(control_p, _ab_mde, _ab_alpha, _ab_power)

	var ab_grouping: String = "%s vs %s" % [control_key, treatment_key]
	rows.append(
		"ab_comparison,%s,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d" % [
			_csv_field(ab_grouping),
			control["trials"], control_wins, control_losses,
			control_p, control_ci[0], control_ci[1],
			control_p, control_ci[0], control_ci[1],
			treatment_p, treatment_ci[0], treatment_ci[1],
			delta, delta_ci_lower, delta_ci_upper,
			z, p_value, required_n
		]
	)

	print("A/B: %s (%.1f%% [%.1f%%-%.1f%%]) vs %s (%.1f%% [%.1f%%-%.1f%%]) => delta %.1fpp (95%% CI %.1fpp-%.1fpp), z=%.2f, p=%.4f, required n/group=%d (MDE=%.1f%%)" % [
		control_key, control_p * 100.0, control_ci[0] * 100.0, control_ci[1] * 100.0,
		treatment_key, treatment_p * 100.0, treatment_ci[0] * 100.0, treatment_ci[1] * 100.0,
		delta * 100.0, delta_ci_lower * 100.0, delta_ci_upper * 100.0,
		z, p_value, required_n, _ab_mde * 100.0
	])

	return rows


func _wilson_ci(wins: int, losses: int, z: float) -> Array:
	var n: int = wins + losses
	if n <= 0:
		return [0.0, 1.0]
	var p: float = float(wins) / float(n)
	var z2: float = z * z
	var denom: float = 1.0 + z2 / float(n)
	var center: float = (p + z2 / (2.0 * float(n))) / denom
	var margin: float = z * sqrt(p * (1.0 - p) / float(n) + z2 / (4.0 * float(n) * float(n))) / denom
	return [maxf(0.0, center - margin), minf(1.0, center + margin)]


func _two_proportion_z_test(x1: int, n1: int, x2: int, n2: int) -> Dictionary:
	var p1: float = float(x1) / float(maxf(1.0, float(n1)))
	var p2: float = float(x2) / float(maxf(1.0, float(n2)))
	var p_pool: float = float(x1 + x2) / float(maxf(1.0, float(n1 + n2)))
	var se: float = sqrt(p_pool * (1.0 - p_pool) * (1.0 / float(maxf(1.0, float(n1))) + 1.0 / float(maxf(1.0, float(n2)))))
	if se <= 0.0:
		return {"z": 0.0, "p_value": 1.0}
	var z: float = (p1 - p2) / se
	var p_value: float = 2.0 * (1.0 - _normal_cdf(absf(z)))
	return {"z": z, "p_value": p_value}


func _required_n_per_group(p0: float, delta: float, alpha: float, power: float) -> int:
	var p0_safe: float = clampf(p0, 0.001, 0.999)
	var p1: float = clampf(p0_safe + delta, 0.001, 0.999)
	var p_avg: float = (p0_safe + p1) / 2.0
	var z_alpha: float = _normal_cdf_inv(1.0 - alpha / 2.0)
	var z_beta: float = _normal_cdf_inv(power)
	var n: float = pow(
		z_alpha * sqrt(2.0 * p_avg * (1.0 - p_avg)) +
		z_beta * sqrt(p0_safe * (1.0 - p0_safe) + p1 * (1.0 - p1)),
		2.0
	) / (delta * delta)
	return maxi(1, int(ceilf(n)))


func _normal_cdf(x: float) -> float:
	if x >= 0.0:
		var p: float = 0.2316419
		var b1: float = 0.319381530
		var b2: float = -0.356563782
		var b3: float = 1.781477937
		var b4: float = -1.821255978
		var b5: float = 1.330274429
		var t: float = 1.0 / (1.0 + p * x)
		var phi: float = exp(-0.5 * x * x) / sqrt(2.0 * PI)
		return 1.0 - phi * (b1 * t + b2 * t * t + b3 * pow(t, 3.0) + b4 * pow(t, 4.0) + b5 * pow(t, 5.0))
	else:
		return 1.0 - _normal_cdf(-x)


func _normal_cdf_inv(p: float) -> float:
	p = clampf(p, 0.000001, 0.999999)
	var lo: float = -5.0
	var hi: float = 5.0
	var mid: float = 0.0
	for _i in range(NORMAL_CDF_INV_ITERATIONS):
		mid = (lo + hi) / 2.0
		if _normal_cdf(mid) < p:
			lo = mid
		else:
			hi = mid
	return (lo + hi) / 2.0
