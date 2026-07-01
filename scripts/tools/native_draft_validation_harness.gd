extends SceneTree

## Deterministic full-draft validation harness.
## Runs full 5v5 snake drafts using NATIVE_FULL and baseline strategies,
## simulates the completed teams, and emits per-step CSV diagnostics.
##
## Usage:
##   godot --headless --script res://scripts/tools/native_draft_validation_harness.gd \
##     -- --trials=25 --sims-per-draft=25 \
##     --blue-strategies=native_full \
##     --red-strategies=random,base_power_only \
##     --output=res://model_stats/native_draft_validation.csv \
##     --draft-summary-output=res://model_stats/native_draft_validation_drafts.csv

const TEAM_SIZE: int = 5

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SpawnSpecScript := preload("res://scripts/simulation/spawn_spec.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

const DraftStrategyRandomPath := "res://scripts/tools/draft_strategy_random.gd"
const DraftStrategyNativePath := "res://scripts/tools/draft_strategy_native.gd"
const DraftStrategyBasePowerOnlyPath := "res://scripts/tools/draft_strategy_base_power_only.gd"
const DraftStrategyNativeAblationPath := "res://scripts/tools/draft_strategy_native_ablation.gd"

var _backend: RefCounted = null
var _stats_dir: String = "res://model_stats/stats_output_100k"
var _output_path: String = "res://model_stats/native_draft_validation.csv"
var _draft_summary_output_path: String = ""
var _trials: int = 50
var _sims_per_draft: int = 25
var _base_seed: int = 100000
var _blue_strategy_names: Array[String] = ["native_full"]
var _red_strategy_names: Array[String] = ["random", "base_power_only"]


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
	OS.set_environment("TEAMFIGHT_STATS_EXPORT_MINIMAL", "1")
	ChampionCatalogScript.build_catalog()
	ChampionCatalogScript.build_role_configs()
	ChampionCatalogScript.build_passive_registry()
	ChampionCatalogScript.build_minion_catalog()
	ChampionCatalogScript.freeze_built_specs_for_worker_reuse()

	_trials = maxi(1, int(_extract_argument("--trials=", "50")))
	_sims_per_draft = maxi(1, int(_extract_argument("--sims-per-draft=", "25")))
	_base_seed = int(_extract_argument("--base-seed=", "100000"))
	_stats_dir = _extract_argument("--stats-dir=", "res://model_stats/stats_output_100k")
	_output_path = _extract_argument("--output=", "res://model_stats/native_draft_validation.csv")
	_draft_summary_output_path = _extract_argument("--draft-summary-output=", "")
	_blue_strategy_names = _parse_strategy_names(_extract_argument("--blue-strategies=", "native_full"))
	_red_strategy_names = _parse_strategy_names(_extract_argument("--red-strategies=", "random,base_power_only"))

	print("native_draft_validation_harness: trials=%d sims/draft=%d stats_dir=%s" % [
		_trials, _sims_per_draft, _stats_dir
	])

	_backend = NativeSimulationBackendScript.new()
	if not _backend.is_available():
		push_error("native_draft_validation_harness: native backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	if not _backend.has_method("run_matches_stats"):
		push_error("native_draft_validation_harness: backend missing run_matches_stats()")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var required_files: Array[String] = ["combat_stats.csv", "matchup_with.csv", "matchup_vs.csv"]
	for f in required_files:
		var path: String = _stats_dir.path_join(f)
		if not FileAccess.file_exists(path):
			push_error("native_draft_validation_harness: missing required stats file %s" % path)
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("native_draft_validation_harness: not enough champions")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var blue_strategies: Dictionary = _build_strategy_map(_blue_strategy_names)
	var red_strategies: Dictionary = _build_strategy_map(_red_strategy_names)
	if not _validate_strategy_map(blue_strategies, "blue") or not _validate_strategy_map(red_strategies, "red"):
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	var csv_rows: Array[String] = []
	csv_rows.append(
		"draft_seed,blue_strategy,red_strategy,step_index,step_side,step_action," +
		"chosen_candidate,blue_picks,red_picks,blue_bans,red_bans," +
		"blue_wins,red_wins,draws,blue_winrate,red_winrate,drawrate," +
		"base_power,ally_synergy,enemy_counter_value,counter_risk,role_fit,comp_fit," +
		"total_score,phase_label,candidate_role,comp_fingerprint"
	)

	var draft_summary_rows: Array[String] = []
	var emit_draft_summary: bool = not _draft_summary_output_path.is_empty()
	if emit_draft_summary:
		draft_summary_rows.append(
			"draft_seed,blue_strategy,red_strategy,blue_picks,red_picks," +
			"blue_bans,red_bans,blue_wins,red_wins,draws,blue_winrate,red_winrate,drawrate,total_matches"
		)

	var pool_rng := RandomNumberGenerator.new()
	var pairing_index: int = 0

	for trial in range(_trials):
		var draft_seed: int = _base_seed + trial
		pool_rng.seed = draft_seed
		var pool: Array[StringName] = champion_ids.duplicate()
		_shuffle(pool_rng, pool)

		for blue_name in _blue_strategy_names:
			var blue_strat = blue_strategies[blue_name]
			for red_name in _red_strategy_names:
				var red_strat = red_strategies[red_name]

				seed(draft_seed + pairing_index * 1000)
				pairing_index += 1

				var draft_result: Dictionary = _run_full_draft(
					pool.duplicate(), blue_strat, red_strat, draft_seed, blue_name, red_name
				)
				csv_rows.append_array(draft_result.rows)
				if emit_draft_summary:
					draft_summary_rows.append(_format_draft_summary_row(draft_seed, blue_name, red_name, draft_result))

				if trial == 0 and blue_name == _blue_strategy_names[0] and red_name == _red_strategy_names[0]:
					print("sample draft: blue=%s red=%s" % [draft_result.blue_picks, draft_result.red_picks])

	var f := FileAccess.open(ProjectSettings.globalize_path(_output_path), FileAccess.WRITE)
	if f == null:
		push_error("native_draft_validation_harness: could not open output %s" % _output_path)
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	f.store_string("\n".join(csv_rows) + "\n")
	f.close()

	print("native_draft_validation_harness: wrote %s (%d rows)" % [_output_path, csv_rows.size() - 1])

	if emit_draft_summary:
		var df := FileAccess.open(ProjectSettings.globalize_path(_draft_summary_output_path), FileAccess.WRITE)
		if df == null:
			push_error("native_draft_validation_harness: could not open draft summary %s" % _draft_summary_output_path)
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return
		df.store_string("\n".join(draft_summary_rows) + "\n")
		df.close()
		print("native_draft_validation_harness: wrote %s (%d rows)" % [_draft_summary_output_path, draft_summary_rows.size() - 1])

	if _backend.has_method("clear"):
		_backend.call("clear")

	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)


func _build_strategy_map(names: Array[String]) -> Dictionary:
	var out: Dictionary = {}
	for name in names:
		out[name] = _build_strategy(name)
	return out


func _validate_strategy_map(map: Dictionary, side: String) -> bool:
	var valid: bool = true
	for name in map.keys():
		if map[name] == null:
			push_error("native_draft_validation_harness: failed to build %s strategy '%s'" % [side, name])
			valid = false
	return valid


func _build_strategy(name: String):
	match name:
		"native_full":
			return load(DraftStrategyNativePath).new(_stats_dir)
		"random":
			return load(DraftStrategyRandomPath).new()
		"base_power_only":
			return load(DraftStrategyBasePowerOnlyPath).new(_stats_dir)
		_:
			if name.begins_with("native_ablation_"):
				var variant: String = name.substr("native_ablation_".length())
				var ablation = load(DraftStrategyNativeAblationPath).new(_stats_dir, variant)
				if ablation._pick_zero.is_empty() and ablation._ban_zero.is_empty():
					push_error("native_draft_validation_harness: unknown ablation variant '%s'" % variant)
					return null
				return ablation
			push_error("native_draft_validation_harness: unknown strategy '%s'" % name)
			return null


func _run_full_draft(
	available: Array[StringName],
	blue_strat: Object,
	red_strat: Object,
	draft_seed: int,
	blue_name: String,
	red_name: String
) -> Dictionary:
	var blue_picks: Array[StringName] = []
	var red_picks: Array[StringName] = []
	var blue_bans: Array[StringName] = []
	var red_bans: Array[StringName] = []
	var step_records: Array[Dictionary] = []
	var sequence: Array = SimConstantsScript.DRAFT_SEQUENCE

	for step_index in range(sequence.size()):
		var turn: String = sequence[step_index]
		var side: String = turn.substr(0, 1)
		var action: String = turn.substr(2)
		var allies: Array[StringName] = blue_picks if side == "B" else red_picks
		var enemies: Array[StringName] = red_picks if side == "B" else blue_picks
		var strategy = blue_strat if side == "B" else red_strat
		var acting_side: String = "blue" if side == "B" else "red"

		var chosen: StringName
		if action == "PICK":
			chosen = strategy.recommend_next_pick(allies, enemies, available, step_index)
		else:
			chosen = strategy.recommend_next_ban(allies, enemies, available, step_index, acting_side, {})

		if chosen.is_empty() or not chosen in available:
			push_error("native_draft_validation_harness: invalid selection at step %d side %s" % [step_index, side])
			chosen = available[0]

		var diag: Dictionary = _diagnostic_for_chosen(
			action, allies, enemies, available, step_index, acting_side, chosen
		)
		step_records.append({
			"step_index": step_index,
			"side": side,
			"action": action,
			"chosen": chosen,
			"diagnostic": diag
		})

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

	var sim: Dictionary = _simulate_matchup(blue_picks, red_picks, _sims_per_draft, draft_seed)

	var rows: Array[String] = []
	for rec in step_records:
		rows.append(_format_row(
			draft_seed, blue_name, red_name,
			rec["step_index"], rec["side"], rec["action"], rec["chosen"],
			blue_picks, red_picks, blue_bans, red_bans,
			sim["blue_wins"], sim["red_wins"], sim["draws"],
			sim["blue_winrate"], sim["red_winrate"], sim["drawrate"],
			rec["diagnostic"]
		))

	return {
		"rows": rows,
		"blue_picks": blue_picks,
		"red_picks": red_picks,
		"blue_bans": blue_bans,
		"red_bans": red_bans,
		"blue_wins": sim["blue_wins"],
		"red_wins": sim["red_wins"],
		"draws": sim["draws"],
		"blue_winrate": sim["blue_winrate"],
		"red_winrate": sim["red_winrate"],
		"drawrate": sim["drawrate"]
	}


func _diagnostic_for_chosen(
	action: String,
	allies: Array[StringName],
	enemies: Array[StringName],
	available: Array[StringName],
	step_index: int,
	acting_side: String,
	chosen: StringName
) -> Dictionary:
	var diag: Dictionary = {
		"base_power": 0.0,
		"ally_synergy": 0.0,
		"enemy_counter_value": 0.0,
		"counter_risk": 0.0,
		"role_fit": 0.0,
		"comp_fit": 0.0,
		"total_score": 0.0,
		"phase_label": "",
		"candidate_role": "",
		"comp_fingerprint": ""
	}
	if _backend == null or not _backend.is_available():
		return diag

	var recommendations: Array
	if action == "PICK":
		recommendations = _backend.get_draft_ai_pick_recommendations(
			_stats_dir, available, allies, enemies, available.size(), step_index
		)
	else:
		recommendations = _backend.get_draft_ai_ban_recommendations(
			_stats_dir, available, allies, enemies, available.size(), step_index, acting_side, {}
		)

	for rec in recommendations:
		if StringName(rec.get("candidate", "")) == chosen:
			if action == "PICK":
				diag["base_power"] = float(rec.get("base_power", 0.0))
				diag["ally_synergy"] = float(rec.get("ally_synergy", 0.0))
				diag["enemy_counter_value"] = float(rec.get("enemy_counter_value", 0.0))
				diag["counter_risk"] = float(rec.get("counter_risk", 0.0))
				diag["role_fit"] = float(rec.get("role_fit", 0.0))
				diag["comp_fit"] = float(rec.get("comp_fit", 0.0))
				diag["comp_fingerprint"] = String(rec.get("comp_fingerprint", ""))
			else:
				diag["base_power"] = float(rec.get("enemy_pick_value", 0.0))
				diag["ally_synergy"] = float(rec.get("enemy_synergy", 0.0))
				diag["enemy_counter_value"] = float(rec.get("counters_my_team", 0.0))
				diag["counter_risk"] = 0.0
				diag["role_fit"] = float(rec.get("fills_enemy_role_need", 0.0))
				diag["comp_fit"] = float(rec.get("enemy_comp_fit", 0.0))
				diag["comp_fingerprint"] = String(rec.get("enemy_comp_fingerprint", ""))
			diag["total_score"] = float(rec.get("total_score", 0.0))
			diag["phase_label"] = String(rec.get("phase_label", ""))
			diag["candidate_role"] = String(rec.get("candidate_role", ""))
			break
	return diag


func _simulate_matchup(blue_team: Array, red_team: Array, sims: int, seed: int) -> Dictionary:
	var blue_wins: int = 0
	var red_wins: int = 0
	var draws: int = 0

	var player_units: Array = _build_units(blue_team, &"player")
	var enemy_units: Array = _build_units(red_team, &"enemy")

	var inputs: Array = []
	for s in range(sims):
		inputs.append(MatchReplayInputScript.new(
			seed + s * 1000, SimConstantsScript.DEFAULT_TICK_RATE,
			player_units, enemy_units
		))

	var summaries_var: Variant = _backend.run_matches_stats(inputs)
	if typeof(summaries_var) != TYPE_ARRAY:
		push_error("native_draft_validation_harness: run_matches_stats returned non-array")
		return {"blue_wins": 0, "red_wins": 0, "draws": 0,
			"blue_winrate": 0.0, "red_winrate": 0.0, "drawrate": 0.0}

	var summaries: Array = summaries_var
	for summary in summaries:
		if summary.has("winner_team"):
			if summary.winner_team == "player":
				blue_wins += 1
			elif summary.winner_team == "enemy":
				red_wins += 1
			else:
				draws += 1

	var total: int = blue_wins + red_wins + draws
	if total == 0:
		total = 1
	return {
		"blue_wins": blue_wins,
		"red_wins": red_wins,
		"draws": draws,
		"blue_winrate": float(blue_wins) / float(total),
		"red_winrate": float(red_wins) / float(total),
		"drawrate": float(draws) / float(total)
	}


func _build_units(ids: Array, team: StringName) -> Array:
	var units: Array = []
	for index in range(ids.size()):
		var pos: Vector2 = SimConstantsScript.spawn_position(index, team)
		units.append(SpawnSpecScript.new(ids[index], team, pos.x, pos.y))
	return units


func _format_row(
	draft_seed: int,
	blue_name: String,
	red_name: String,
	step_index: int,
	step_side: String,
	step_action: String,
	chosen: StringName,
	blue_picks: Array,
	red_picks: Array,
	blue_bans: Array,
	red_bans: Array,
	blue_wins: int,
	red_wins: int,
	draws: int,
	blue_winrate: float,
	red_winrate: float,
	drawrate: float,
	diag: Dictionary
) -> String:
	var parts: Array[String] = [
		str(draft_seed),
		blue_name,
		red_name,
		str(step_index),
		step_side,
		step_action,
		String(chosen),
		"|".join(blue_picks),
		"|".join(red_picks),
		"|".join(blue_bans),
		"|".join(red_bans),
		str(blue_wins),
		str(red_wins),
		str(draws),
		"%.6f" % blue_winrate,
		"%.6f" % red_winrate,
		"%.6f" % drawrate,
		"%.6f" % float(diag.get("base_power", 0.0)),
		"%.6f" % float(diag.get("ally_synergy", 0.0)),
		"%.6f" % float(diag.get("enemy_counter_value", 0.0)),
		"%.6f" % float(diag.get("counter_risk", 0.0)),
		"%.6f" % float(diag.get("role_fit", 0.0)),
		"%.6f" % float(diag.get("comp_fit", 0.0)),
		"%.6f" % float(diag.get("total_score", 0.0)),
		String(diag.get("phase_label", "")),
		String(diag.get("candidate_role", "")),
		String(diag.get("comp_fingerprint", ""))
	]
	return ",".join(parts)


func _format_draft_summary_row(
	draft_seed: int,
	blue_name: String,
	red_name: String,
	result: Dictionary
) -> String:
	var total_matches: int = int(result["blue_wins"]) + int(result["red_wins"]) + int(result["draws"])
	var parts: Array[String] = [
		str(draft_seed),
		blue_name,
		red_name,
		"|".join(result["blue_picks"]),
		"|".join(result["red_picks"]),
		"|".join(result["blue_bans"]),
		"|".join(result["red_bans"]),
		str(result["blue_wins"]),
		str(result["red_wins"]),
		str(result["draws"]),
		"%.6f" % float(result["blue_winrate"]),
		"%.6f" % float(result["red_winrate"]),
		"%.6f" % float(result["drawrate"]),
		str(total_matches)
	]
	return ",".join(parts)


func _shuffle(rng: RandomNumberGenerator, arr: Array) -> void:
	for i in range(arr.size() - 1, 0, -1):
		var j: int = rng.randi() % (i + 1)
		var tmp = arr[i]
		arr[i] = arr[j]
		arr[j] = tmp
