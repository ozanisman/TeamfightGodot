extends SceneTree

## Per-step lookahead diagnostic for native draft AI (Workstream B.1).
##
## Usage:
##   godot --headless --path . --script res://scripts/tools/native_draft_lookahead_diagnostic.gd \
##     -- --trials=1 --draft-seed=424242 \
##        --config-mode=legacy \
##        --output=res://logs/native_draft_lookahead_diagnostic.csv \
##        --turn-table \
##        --self-test

const DraftHarnessCoreScript := preload("res://scripts/tools/draft_harness_core.gd")
const DraftAiConfigScript := preload("res://scripts/tools/draft_ai_config.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

const STRATEGY_LOOKAHEAD: int = 1
const STRATEGY_NATIVE_FULL: int = 0

const LOOKAHEAD_ADJUSTMENT_EPSILON: float = 0.0001

var _stats_dir: String = "res://model_stats/stats_output_100k"
var _draft_seed: int = 424242
var _trials: int = 1
var _output_path: String = "res://logs/native_draft_lookahead_diagnostic.csv"
var _config_path: String = DraftAiConfigScript.LEGACY_LOOKAHEAD_CONFIG_PATH
var _self_test: bool = false
var _write_turn_table: bool = false


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	await process_frame
	_stats_dir = _extract_argument("--stats-dir=", _stats_dir)
	_draft_seed = int(_extract_argument("--draft-seed=", str(_draft_seed)))
	_trials = maxi(1, int(_extract_argument("--trials=", str(_trials))))
	_output_path = _extract_argument("--output=", _output_path)
	_self_test = "--self-test" in OS.get_cmdline_user_args()
	_write_turn_table = "--turn-table" in OS.get_cmdline_user_args()
	_config_path = _resolve_config_path(_extract_argument("--config-mode=", "legacy"))

	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("native_draft_lookahead_diagnostic: native backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	if _self_test:
		var ok: bool = _run_softmax_self_test(backend)
		print("native_draft_lookahead_diagnostic self-test: %s" % ("PASS" if ok else "FAIL"))
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 0 if ok else 1)
		return

	if _write_turn_table:
		_write_turn_logic_table(backend)

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	var pool_rng := RandomNumberGenerator.new()
	var rows: Array[String] = []
	rows.append(
		"draft_seed,step_index,side,action,lookahead_applied,pool_size,results_count,truncated," +
		"immediate_score,lookahead_adjustment,opponent_response_score,chosen"
	)

	var agg: Dictionary = {
		"B": {"lookahead_steps": 0, "abs_adjustment_sum": 0.0},
		"R": {"lookahead_steps": 0, "abs_adjustment_sum": 0.0},
	}
	var truncation_count: int = 0

	for trial in range(_trials):
		var draft_seed: int = _draft_seed + trial
		pool_rng.seed = draft_seed
		var pool: Array[StringName] = champion_ids.duplicate()
		DraftHarnessCoreScript.shuffle_pool(pool_rng, pool)
		truncation_count += _collect_draft_rows(backend, pool, draft_seed, rows, agg, _config_path)

	var global_path: String = ProjectSettings.globalize_path(_output_path)
	_ensure_parent_dir(global_path)
	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f == null:
		push_error("native_draft_lookahead_diagnostic: could not write %s" % _output_path)
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	f.store_string("\n".join(rows) + "\n")
	f.close()

	print("native_draft_lookahead_diagnostic: wrote %s (%d data rows)" % [_output_path, rows.size() - 1])
	print("lookahead-active steps B=%d R=%d truncation_flags=%d" % [
		agg["B"]["lookahead_steps"], agg["R"]["lookahead_steps"], truncation_count
	])
	for side in ["B", "R"]:
		var steps: int = int(agg[side]["lookahead_steps"])
		if steps > 0:
			print("mean |adjustment| side %s: %.4f" % [
				side, float(agg[side]["abs_adjustment_sum"]) / float(steps)
			])
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)


func _resolve_config_path(config_mode: String) -> String:
	match config_mode.to_lower():
		"softmax", "native_lookahead_softmax":
			return DraftAiConfigScript.LOOKAHEAD_SOFTMAX_CONFIG_PATH
		"legacy", "native_lookahead":
			return DraftAiConfigScript.LEGACY_LOOKAHEAD_CONFIG_PATH
		_:
			push_warning("native_draft_lookahead_diagnostic: unknown config-mode '%s', using legacy" % config_mode)
			return DraftAiConfigScript.LEGACY_LOOKAHEAD_CONFIG_PATH


func _write_turn_logic_table(backend: RefCounted) -> void:
	if not backend.has_method("debug_lookahead_turn_diagnostic"):
		push_error("native_draft_lookahead_diagnostic: backend missing debug_lookahead_turn_diagnostic()")
		return
	backend.debug_lookahead_turn_diagnostic()
	var cwd_report := "lookahead_turn_logic_report.txt"
	var dest := ProjectSettings.globalize_path("res://logs/native_draft_lookahead_turn_logic_report.txt")
	_ensure_parent_dir(dest)
	if FileAccess.file_exists(cwd_report):
		var src := FileAccess.open(cwd_report, FileAccess.READ)
		var dst := FileAccess.open(dest, FileAccess.WRITE)
		if src != null and dst != null:
			dst.store_string(src.get_as_text())
			src.close()
			dst.close()
			print("native_draft_lookahead_diagnostic: wrote res://logs/native_draft_lookahead_turn_logic_report.txt")
		else:
			push_warning("native_draft_lookahead_diagnostic: could not copy turn logic report to logs/")
	else:
		push_warning("native_draft_lookahead_diagnostic: turn logic report not found at %s" % cwd_report)


func _collect_draft_rows(
	backend: RefCounted,
	pool: Array[StringName],
	draft_seed: int,
	rows: Array[String],
	agg: Dictionary,
	config_path: String
) -> int:
	var truncation_count: int = 0
	var blue_picks: Array[StringName] = []
	var red_picks: Array[StringName] = []
	var blue_bans: Array[StringName] = []
	var red_bans: Array[StringName] = []
	var available: Array[StringName] = pool.duplicate()
	var sequence: Array = SimConstantsScript.DRAFT_SEQUENCE

	for step_index in range(sequence.size()):
		var turn: String = sequence[step_index]
		var side: String = turn.substr(0, 1)
		var action: String = turn.substr(2)
		var allies: Array[StringName] = blue_picks if side == "B" else red_picks
		var enemies: Array[StringName] = red_picks if side == "B" else blue_picks
		var acting_side: String = "blue" if side == "B" else "red"

		var pool_size: int = available.size()
		var recommendations: Array
		var chosen: StringName
		if action == "PICK":
			recommendations = backend.get_draft_ai_pick_recommendations(
				_stats_dir, available, allies, enemies, pool_size, step_index,
				STRATEGY_LOOKAHEAD, config_path
			)
			chosen = StringName(recommendations[0].get("candidate", "")) if not recommendations.is_empty() else StringName("")
		else:
			recommendations = backend.get_draft_ai_ban_recommendations(
				_stats_dir, available, allies, enemies, pool_size, step_index, acting_side, {},
				STRATEGY_LOOKAHEAD, config_path
			)
			chosen = StringName(recommendations[0].get("candidate", "")) if not recommendations.is_empty() else StringName("")

		var chosen_rec: Dictionary = {}
		for rec in recommendations:
			if StringName(rec.get("candidate", "")) == chosen:
				chosen_rec = rec
				break
		if chosen_rec.is_empty() and not recommendations.is_empty():
			chosen_rec = recommendations[0]

		var lookahead_applied: bool = absf(float(chosen_rec.get("lookahead_adjustment", 0.0))) > LOOKAHEAD_ADJUSTMENT_EPSILON
		var results_count: int = recommendations.size()
		var truncated: bool = lookahead_applied and results_count < pool_size
		if truncated:
			truncation_count += 1

		if lookahead_applied:
			agg[side]["lookahead_steps"] = int(agg[side]["lookahead_steps"]) + 1
			agg[side]["abs_adjustment_sum"] = float(agg[side]["abs_adjustment_sum"]) + absf(float(chosen_rec.get("lookahead_adjustment", 0.0)))

		rows.append(",".join([
			str(draft_seed),
			str(step_index),
			side,
			action,
			"1" if lookahead_applied else "0",
			str(pool_size),
			str(results_count),
			"1" if truncated else "0",
			"%.6f" % float(chosen_rec.get("immediate_score", chosen_rec.get("total_score", 0.0))),
			"%.6f" % float(chosen_rec.get("lookahead_adjustment", 0.0)),
			"%.6f" % float(chosen_rec.get("opponent_response_score", 0.0)),
			String(chosen),
		]))

		available.erase(chosen)
		if action == "PICK":
			if side == "B":
				blue_picks.append(chosen)
			else:
				red_picks.append(chosen)
		else:
			if side == "B":
				blue_bans.append(chosen)
			else:
				red_bans.append(chosen)

	return truncation_count


func _run_softmax_self_test(backend: RefCounted) -> bool:
	var available: Array[StringName] = [
		&"valkyrie", &"mirror_knight", &"mistcaller", &"monk", &"rogue",
		&"colossus", &"silencer", &"wizard", &"berserker", &"windcaller",
	]
	var allies: Array[StringName] = []
	var enemies: Array[StringName] = []
	var step_index: int = 6
	var next_step: int = 7
	var top_k: int = DraftAiConfigScript.get_softmax_top_k()
	var temperature: float = DraftAiConfigScript.get_softmax_temperature()
	var scale: float = DraftAiConfigScript.get_softmax_scale()

	var lookahead_recs: Array = backend.get_draft_ai_pick_recommendations(
		_stats_dir, available, allies, enemies, 8, step_index,
		STRATEGY_LOOKAHEAD, DraftAiConfigScript.LOOKAHEAD_SOFTMAX_CONFIG_PATH
	)
	if lookahead_recs.is_empty():
		push_error("native_draft_lookahead_diagnostic self-test: no lookahead recommendations")
		return false

	var top: Dictionary = lookahead_recs[0]
	var picked: StringName = StringName(top.get("candidate", ""))
	var new_allies: Array[StringName] = allies.duplicate()
	new_allies.append(picked)
	var new_available: Array[StringName] = []
	for candidate in available:
		if candidate != picked:
			new_available.append(candidate)

	var enemy_responses: Array = backend.get_draft_ai_pick_recommendations(
		_stats_dir, new_available, enemies, new_allies, top_k, next_step,
		STRATEGY_NATIVE_FULL, DraftAiConfigScript.DEFAULT_CONFIG_PATH
	)
	if enemy_responses.is_empty():
		push_error("native_draft_lookahead_diagnostic self-test: no enemy responses")
		return false

	var expected: float = _softmax_expected_score(enemy_responses, temperature, scale)
	var cpp_score: float = float(top.get("opponent_response_score", 0.0))
	var delta: float = absf(cpp_score - expected)
	print("softmax self-test expected=%.6f cpp=%.6f delta=%.6f" % [expected, cpp_score, delta])
	return delta < 0.0001


func _softmax_expected_score(recommendations: Array, temperature: float, scale: float) -> float:
	var scores: Array[float] = []
	for rec in recommendations:
		scores.append(float(rec.get("total_score", 0.0)))
	if scores.is_empty():
		return 0.0
	var max_scaled: float = scores[0] * scale
	for score in scores:
		max_scaled = maxf(max_scaled, score * scale)
	var exp_sum: float = 0.0
	var weighted_sum: float = 0.0
	for score in scores:
		var exp_val: float = exp((score * scale - max_scaled) / temperature)
		exp_sum += exp_val
		weighted_sum += exp_val * score
	if exp_sum <= 0.0:
		return 0.0
	return weighted_sum / exp_sum


func _ensure_parent_dir(path: String) -> void:
	var dir_path: String = path.get_base_dir()
	if not DirAccess.dir_exists_absolute(dir_path):
		DirAccess.make_dir_recursive_absolute(dir_path)
