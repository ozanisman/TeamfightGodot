class_name DraftPersonaGateCore
extends RefCounted

const DraftValidationCsvScript := preload("res://scripts/tools/draft_validation_csv.gd")
const DraftEloGateCoreScript := preload("res://scripts/tools/draft_elo_gate_core.gd")

const CONTROL_STRATEGY: String = "native_softmax"
const DEFAULT_PERSONAS: Array[String] = [
	"native_softmax_safe",
	"native_softmax_ceiling",
	"native_softmax_counter_heavy",
]
const DEFAULT_ELO_MIN_GAP: float = 0.0
const DEFAULT_SIDE_BIAS_MARGIN_PP: float = 3.0
const DEFAULT_P_VALUE_THRESHOLD: float = 0.05
const DEFAULT_REALISM_MARGIN: float = 0.03
const SQRT_2: float = 1.4142135623730951
const DraftPersonaRealismCoreScript := preload("res://scripts/tools/draft_persona_realism_core.gd")


static func evaluate_from_paths(params: Dictionary) -> Dictionary:
	var summary_path: String = String(params.get("summary_path", ""))
	var ab_report_path: String = String(params.get("ab_report_path", ""))
	var elo_ladder_path: String = String(params.get("elo_ladder_path", ""))
	var draft_summary_path: String = String(params.get("draft_summary_path", ""))
	var realism_metrics_path: String = String(params.get("realism_metrics_path", ""))

	var errors: Array[String] = []
	if summary_path.is_empty():
		errors.append("summary_path required")
	if ab_report_path.is_empty():
		errors.append("ab_report_path required")
	if elo_ladder_path.is_empty():
		errors.append("elo_ladder_path required")
	if draft_summary_path.is_empty():
		errors.append("draft_summary_path required")
	if not errors.is_empty():
		return _error_result(errors)

	var summary_rows: Array[Dictionary] = DraftValidationCsvScript.load_metric_rows(summary_path)
	if summary_rows.is_empty():
		errors.append("summary empty or missing: %s" % summary_path)

	var ab_rows: Array[Dictionary] = DraftValidationCsvScript.load_metric_rows(ab_report_path)
	if ab_rows.is_empty():
		errors.append("A/B report empty or missing: %s" % ab_report_path)

	var ratings: Dictionary = DraftEloGateCoreScript.load_ratings_csv(elo_ladder_path)
	if ratings.is_empty():
		errors.append("Elo ladder empty or missing: %s" % elo_ladder_path)

	var realism_metrics: Dictionary = {}
	if not realism_metrics_path.is_empty():
		realism_metrics = DraftPersonaRealismCoreScript.load_metrics_csv(realism_metrics_path)

	errors.append_array(DraftEloGateCoreScript.check_input_freshness(elo_ladder_path, draft_summary_path))
	if not errors.is_empty():
		return _error_result(errors)

	var eval_params: Dictionary = params.duplicate()
	eval_params["realism_metrics"] = realism_metrics
	return evaluate_rows(summary_rows, ab_rows, ratings, [], eval_params)


static func evaluate_rows(
	summary_rows: Array[Dictionary],
	ab_rows: Array[Dictionary],
	ratings: Dictionary,
	freshness_errors: Array[String],
	params: Dictionary
) -> Dictionary:
	var control: String = String(params.get("control", CONTROL_STRATEGY))
	var personas: Array[String] = _coerce_strategy_names(params.get("personas", DEFAULT_PERSONAS))
	var elo_min_gap: float = float(params.get("elo_min_gap", DEFAULT_ELO_MIN_GAP))
	var side_bias_margin_pp: float = float(params.get("side_bias_margin_pp", DEFAULT_SIDE_BIAS_MARGIN_PP))
	var p_value_threshold: float = float(params.get("p_value_threshold", DEFAULT_P_VALUE_THRESHOLD))
	var realism_margin: float = float(params.get("realism_margin", DEFAULT_REALISM_MARGIN))
	var realism_metrics: Dictionary = params.get("realism_metrics", {})

	var errors: Array[String] = freshness_errors.duplicate()
	if personas.is_empty():
		errors.append("no personas configured")
	if not ratings.has(control):
		errors.append("control strategy missing from Elo ladder: %s" % control)

	var control_bias: float = _self_play_bias_pp(summary_rows, control)
	if is_nan(control_bias):
		errors.append("control self-play row missing from analyzer summary: %s" % control)

	if not errors.is_empty():
		return _error_result(errors)

	var decisions: Array[Dictionary] = []
	var overall_pass: bool = true
	for persona in personas:
		var decision: Dictionary = _evaluate_persona(
			persona,
			control,
			summary_rows,
			ab_rows,
			ratings,
			control_bias,
			elo_min_gap,
			side_bias_margin_pp,
			p_value_threshold,
			realism_metrics,
			realism_margin
		)
		if String(decision.get("status", "")) == "REJECT":
			overall_pass = false
		decisions.append(decision)

	return {
		"pass": overall_pass,
		"errors": [],
		"decisions": decisions,
		"control": control,
		"control_self_play_bias_pp": control_bias,
		"realism_evaluated": not realism_metrics.is_empty(),
	}


static func self_test() -> Dictionary:
	var cases: Array[Dictionary] = []
	cases.append(_self_test_case(
		"promote when Elo, A/B, side-bias, realism pass",
		{"native_softmax": 1500.0, "native_softmax_safe": 1510.0},
		_self_play_rows(0.52, 0.53),
		_ab_rows(0.52),
		{"realism_metrics": _realism_metrics(false)},
		true,
		"PROMOTE"
	))
	cases.append(_self_test_case(
		"validation-only when realism is missing",
		{"native_softmax": 1500.0, "native_softmax_safe": 1510.0},
		_self_play_rows(0.52, 0.53),
		_ab_rows(0.52),
		{},
		true,
		"VALIDATION_ONLY"
	))
	cases.append(_self_test_case(
		"validation-only when A/B is missing",
		{"native_softmax": 1500.0, "native_softmax_safe": 1510.0},
		_self_play_rows(0.52, 0.53),
		[],
		{"realism_metrics": _realism_metrics(false)},
		true,
		"VALIDATION_ONLY"
	))
	cases.append(_self_test_case(
		"reject on realism regression",
		{"native_softmax": 1500.0, "native_softmax_safe": 1510.0},
		_self_play_rows(0.52, 0.53),
		_ab_rows(0.52),
		{"realism_metrics": _realism_metrics(true)},
		false,
		"REJECT"
	))
	cases.append(_self_test_case(
		"promote at realism threshold boundary",
		{"native_softmax": 1500.0, "native_softmax_safe": 1510.0},
		_self_play_rows(0.52, 0.53),
		_ab_rows(0.52),
		{"realism_metrics": _realism_metrics_boundary()},
		true,
		"PROMOTE"
	))
	cases.append(_self_test_case(
		"reject on invalid realism metrics",
		{"native_softmax": 1500.0, "native_softmax_safe": 1510.0},
		_self_play_rows(0.52, 0.53),
		_ab_rows(0.52),
		{"realism_metrics": _invalid_realism_metrics()},
		false,
		"REJECT"
	))
	cases.append(_self_test_case(
		"reject on Elo regression",
		{"native_softmax": 1500.0, "native_softmax_safe": 1499.0},
		_self_play_rows(0.52, 0.53),
		_ab_rows(0.52),
		{"realism_metrics": _realism_metrics(false)},
		false,
		"REJECT"
	))
	cases.append(_self_test_case(
		"reject on side-bias regression",
		{"native_softmax": 1500.0, "native_softmax_safe": 1510.0},
		_self_play_rows(0.52, 0.62),
		_ab_rows(0.52),
		{"realism_metrics": _realism_metrics(false)},
		false,
		"REJECT"
	))
	cases.append(_self_test_case(
		"reject on significant A/B regression",
		{"native_softmax": 1500.0, "native_softmax_safe": 1510.0},
		_self_play_rows(0.52, 0.53),
		_ab_rows(0.40),
		{"realism_metrics": _realism_metrics(false)},
		false,
		"REJECT"
	))
	cases.append(_self_test_case(
		"fail on stale input paths",
		{"native_softmax": 1500.0, "native_softmax_safe": 1510.0},
		_self_play_rows(0.52, 0.53),
		_ab_rows(0.52),
		{"freshness_errors": ["ladder CSV is older than draft summary"]},
		false,
		""
	))
	cases.append(_self_test_path_case("fail on missing required input paths", {}, false))

	var failed: Array[String] = []
	for case in cases:
		if not bool(case["pass"]):
			failed.append(String(case["name"]))
	return {
		"pass": failed.is_empty(),
		"failed": failed,
		"cases": cases,
	}


static func _self_test_case(
	name: String,
	ratings: Dictionary,
	summary_rows: Array[Dictionary],
	ab_rows: Array[Dictionary],
	params: Dictionary,
	expected_pass: bool,
	expected_status: String
) -> Dictionary:
	var eval_params: Dictionary = {
		"control": CONTROL_STRATEGY,
		"personas": ["native_softmax_safe"],
	}
	eval_params.merge(params, true)
	var freshness_errors: Array[String] = []
	if params.has("freshness_errors"):
		for err in Array(params["freshness_errors"]):
			freshness_errors.append(str(err))
	var result: Dictionary = evaluate_rows(summary_rows, ab_rows, ratings, freshness_errors, eval_params)
	var actual_pass: bool = bool(result.get("pass", false))
	var actual_status: String = ""
	var decisions: Array = result.get("decisions", [])
	if not decisions.is_empty():
		var decision: Dictionary = decisions[0]
		actual_status = String(decision.get("status", ""))
	var ok: bool = actual_pass == expected_pass
	if expected_status != "":
		ok = ok and actual_status == expected_status
	return {
		"name": name,
		"pass": ok,
		"expected_pass": expected_pass,
		"actual_pass": actual_pass,
		"expected_status": expected_status,
		"actual_status": actual_status,
		"errors": result.get("errors", []),
	}


static func _self_test_path_case(name: String, params: Dictionary, expected_pass: bool) -> Dictionary:
	var result: Dictionary = evaluate_from_paths(params)
	var actual_pass: bool = bool(result.get("pass", false))
	return {
		"name": name,
		"pass": actual_pass == expected_pass,
		"expected_pass": expected_pass,
		"actual_pass": actual_pass,
		"expected_status": "",
		"actual_status": "",
		"errors": result.get("errors", []),
	}


static func _evaluate_persona(
	persona: String,
	control: String,
	summary_rows: Array[Dictionary],
	ab_rows: Array[Dictionary],
	ratings: Dictionary,
	control_bias: float,
	elo_min_gap: float,
	side_bias_margin_pp: float,
	p_value_threshold: float,
	realism_metrics: Dictionary,
	realism_margin: float
) -> Dictionary:
	var reasons: Array[String] = []
	var persona_elo: float = float(ratings.get(persona, NAN))
	var control_elo: float = float(ratings.get(control, NAN))
	var elo_gap: float = persona_elo - control_elo
	if is_nan(persona_elo):
		reasons.append("persona missing from Elo ladder")
	elif elo_gap < elo_min_gap:
		reasons.append("Elo gap below %.2f" % elo_min_gap)

	var persona_bias: float = _self_play_bias_pp(summary_rows, persona)
	var side_bias_limit: float = control_bias + side_bias_margin_pp
	if is_nan(persona_bias):
		reasons.append("persona self-play row missing from analyzer summary")
	elif persona_bias > side_bias_limit:
		reasons.append("side-bias %.2fpp exceeds %.2fpp" % [persona_bias, side_bias_limit])

	var ab: Dictionary = _ab_regression(ab_rows, control, persona, p_value_threshold)
	if bool(ab.get("significant_regression", false)):
		reasons.append("significant A/B regression")

	var realism: Dictionary = _realism_check(realism_metrics, control, persona, realism_margin)
	var realism_status: String = String(realism.get("status", "NOT_EVALUATED"))
	for reason in Array(realism.get("reasons", [])):
		reasons.append(String(reason))

	var blockers: Array[String] = []
	if String(ab.get("status", "NOT_EVALUATED")) == "NOT_EVALUATED":
		blockers.append("A/B not evaluated")
	if realism_status == "NOT_EVALUATED":
		blockers.append("realism not evaluated")
	var status: String = "PROMOTE"
	if not reasons.is_empty():
		status = "REJECT"
	elif not blockers.is_empty():
		status = "VALIDATION_ONLY"

	return {
		"persona": persona,
		"status": status,
		"reasons": reasons,
		"promotion_blockers": blockers,
		"elo": persona_elo,
		"control_elo": control_elo,
		"elo_gap": elo_gap,
		"elo_min_gap": elo_min_gap,
		"self_play_bias_pp": persona_bias,
		"control_self_play_bias_pp": control_bias,
		"side_bias_limit_pp": side_bias_limit,
		"ab_delta": ab.get("delta", NAN),
		"ab_p_value": ab.get("p_value", NAN),
		"ab_status": String(ab.get("status", "NOT_EVALUATED")),
		"realism_status": realism_status,
	}


static func _realism_check(metrics: Dictionary, control: String, persona: String, margin: float) -> Dictionary:
	if metrics.is_empty():
		return {"status": "NOT_EVALUATED", "reasons": []}
	if not metrics.has(control) or not metrics.has(persona):
		return {"status": "NOT_EVALUATED", "reasons": []}

	var control_row: Dictionary = metrics[control]
	var persona_row: Dictionary = metrics[persona]
	var reasons: Array[String] = []
	for key in ["pick_entropy_norm", "ban_entropy_norm", "unique_pick_rate", "unique_ban_rate"]:
		var control_metric: Dictionary = _metric_float(control_row, key)
		var persona_metric: Dictionary = _metric_float(persona_row, key)
		if not bool(control_metric.get("ok", false)) or not bool(persona_metric.get("ok", false)):
			reasons.append("%s missing or invalid" % key)
			continue
		var control_value: float = float(control_metric.get("value", 0.0))
		var persona_value: float = float(persona_metric.get("value", 0.0))
		if persona_value < control_value - margin:
			reasons.append("%s regressed by more than %.2fpp" % [key, margin * 100.0])
	for key in ["top_pick_concentration", "top_ban_concentration", "repeated_opener_rate"]:
		var control_metric: Dictionary = _metric_float(control_row, key)
		var persona_metric: Dictionary = _metric_float(persona_row, key)
		if not bool(control_metric.get("ok", false)) or not bool(persona_metric.get("ok", false)):
			reasons.append("%s missing or invalid" % key)
			continue
		var control_value: float = float(control_metric.get("value", 0.0))
		var persona_value: float = float(persona_metric.get("value", 0.0))
		if persona_value > control_value + margin:
			reasons.append("%s increased by more than %.2fpp" % [key, margin * 100.0])

	return {
		"status": "PASS" if reasons.is_empty() else "FAIL",
		"reasons": reasons,
	}


static func _metric_float(row: Dictionary, key: String) -> Dictionary:
	if not row.has(key):
		return {"ok": false, "value": 0.0}
	var raw: Variant = row.get(key, 0.0)
	if raw is float or raw is int:
		var value: float = float(raw)
		return {"ok": is_finite(value) and value >= 0.0, "value": value}
	var text: String = str(raw).strip_edges()
	if text.is_valid_float():
		var value: float = float(text)
		return {"ok": is_finite(value) and value >= 0.0, "value": value}
	return {"ok": false, "value": 0.0}


static func _self_play_bias_pp(summary_rows: Array[Dictionary], strategy: String) -> float:
	var grouping: String = "blue=%s red=%s" % [strategy, strategy]
	for row in summary_rows:
		if String(row.get("metric", "")) != "matchup":
			continue
		if String(row.get("grouping", "")) != grouping:
			continue
		var wins: int = int(row.get("wins", 0))
		var losses: int = int(row.get("losses", 0))
		var decisive: int = wins + losses
		if decisive <= 0:
			return NAN
		var blue_rate: float = float(wins) / float(decisive)
		return absf(2.0 * blue_rate - 1.0) * 100.0
	return NAN


static func _ab_regression(
	ab_rows: Array[Dictionary],
	control: String,
	persona: String,
	p_value_threshold: float
) -> Dictionary:
	var derived: Dictionary = _derived_ab_regression(ab_rows, control, persona, p_value_threshold)
	if not derived.is_empty():
		return derived
	return {
		"status": "NOT_EVALUATED",
		"delta": NAN,
		"p_value": NAN,
		"significant_regression": false,
	}


static func _derived_ab_regression(
	ab_rows: Array[Dictionary],
	control: String,
	persona: String,
	p_value_threshold: float
) -> Dictionary:
	var persona_blue_key: String = "blue=%s red=%s" % [persona, control]
	var control_blue_key: String = "blue=%s red=%s" % [control, persona]
	var persona_blue_row: Dictionary = {}
	var control_blue_row: Dictionary = {}
	for row in ab_rows:
		if String(row.get("metric", "")) != "matchup_winrate_ci":
			continue
		var grouping: String = String(row.get("grouping", ""))
		if grouping == persona_blue_key:
			persona_blue_row = row
		elif grouping == control_blue_key:
			control_blue_row = row
	if persona_blue_row.is_empty() or control_blue_row.is_empty():
		return {}

	var persona_wins: int = int(persona_blue_row.get("wins", 0)) + int(control_blue_row.get("losses", 0))
	var control_wins: int = int(persona_blue_row.get("losses", 0)) + int(control_blue_row.get("wins", 0))
	var decisive: int = persona_wins + control_wins
	if decisive <= 0:
		return {}

	var control_p: float = float(control_wins) / float(decisive)
	var persona_p: float = float(persona_wins) / float(decisive)
	var delta: float = persona_p - control_p
	var z: float = _one_proportion_z(persona_wins, decisive, 0.5)
	var p_value: float = 2.0 * (1.0 - _normal_cdf(absf(z)))
	var significant_regression: bool = delta < 0.0 and p_value <= p_value_threshold
	return {
		"status": "REGRESSION" if significant_regression else "PASS",
		"delta": delta,
		"p_value": p_value,
		"significant_regression": significant_regression,
	}


static func _one_proportion_z(wins: int, total: int, expected_rate: float) -> float:
	if total <= 0:
		return 0.0
	var observed: float = float(wins) / float(total)
	var se: float = sqrt(expected_rate * (1.0 - expected_rate) / float(total))
	if se <= 0.0:
		return 0.0
	return (observed - expected_rate) / se


static func _normal_cdf(x: float) -> float:
	return 0.5 * (1.0 + _erf_approx(x / SQRT_2))


static func _erf_approx(x: float) -> float:
	var sign: float = -1.0 if x < 0.0 else 1.0
	var ax: float = absf(x)
	var t: float = 1.0 / (1.0 + 0.3275911 * ax)
	var y: float = 1.0 - (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t * exp(-ax * ax)
	return sign * y


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


static func _error_result(errors: Array[String]) -> Dictionary:
	return {
		"pass": false,
		"errors": errors,
		"decisions": [],
		"control": CONTROL_STRATEGY,
		"control_self_play_bias_pp": NAN,
		"realism_evaluated": false,
	}


static func _self_play_rows(control_rate: float, persona_rate: float) -> Array[Dictionary]:
	return [
		_matchup_row(CONTROL_STRATEGY, control_rate),
		_matchup_row("native_softmax_safe", persona_rate),
	]


static func _matchup_row(strategy: String, blue_rate: float) -> Dictionary:
	var decisive: int = 100
	var wins: int = int(round(blue_rate * float(decisive)))
	return {
		"metric": "matchup",
		"grouping": "blue=%s red=%s" % [strategy, strategy],
		"trials": 10,
		"wins": wins,
		"losses": decisive - wins,
		"draws": 0,
	}


static func _ab_rows(persona_rate: float) -> Array[Dictionary]:
	var decisive_per_side: int = 50
	var persona_blue_wins: int = int(round(persona_rate * float(decisive_per_side)))
	var persona_red_wins: int = int(round(persona_rate * float(decisive_per_side)))
	return [
		{
			"metric": "matchup_winrate_ci",
			"grouping": "blue=native_softmax_safe red=native_softmax",
			"wins": persona_blue_wins,
			"losses": decisive_per_side - persona_blue_wins,
		},
		{
			"metric": "matchup_winrate_ci",
			"grouping": "blue=native_softmax red=native_softmax_safe",
			"wins": decisive_per_side - persona_red_wins,
			"losses": persona_red_wins,
		},
	]


static func _realism_metrics(regressed: bool) -> Dictionary:
	var persona_pick_entropy: float = 0.70 if regressed else 0.80
	var persona_top_pick: float = 0.24 if regressed else 0.18
	return {
		"native_softmax": {
			"pick_entropy_norm": 0.80,
			"ban_entropy_norm": 0.80,
			"unique_pick_rate": 0.70,
			"unique_ban_rate": 0.70,
			"top_pick_concentration": 0.18,
			"top_ban_concentration": 0.18,
			"repeated_opener_rate": 0.10,
			"counter_pick_rate": "NOT_EVALUATED",
		},
		"native_softmax_safe": {
			"pick_entropy_norm": persona_pick_entropy,
			"ban_entropy_norm": 0.80,
			"unique_pick_rate": 0.70,
			"unique_ban_rate": 0.70,
			"top_pick_concentration": persona_top_pick,
			"top_ban_concentration": 0.18,
			"repeated_opener_rate": 0.10,
			"counter_pick_rate": "NOT_EVALUATED",
		},
	}


static func _realism_metrics_boundary() -> Dictionary:
	return {
		"native_softmax": {
			"pick_entropy_norm": 0.80,
			"ban_entropy_norm": 0.80,
			"unique_pick_rate": 0.70,
			"unique_ban_rate": 0.70,
			"top_pick_concentration": 0.18,
			"top_ban_concentration": 0.18,
			"repeated_opener_rate": 0.10,
			"counter_pick_rate": "NOT_EVALUATED",
		},
		"native_softmax_safe": {
			"pick_entropy_norm": 0.77,
			"ban_entropy_norm": 0.77,
			"unique_pick_rate": 0.67,
			"unique_ban_rate": 0.67,
			"top_pick_concentration": 0.21,
			"top_ban_concentration": 0.21,
			"repeated_opener_rate": 0.13,
			"counter_pick_rate": "NOT_EVALUATED",
		},
	}


static func _invalid_realism_metrics() -> Dictionary:
	var metrics: Dictionary = _realism_metrics(false)
	metrics["native_softmax_safe"].erase("unique_pick_rate")
	return metrics
