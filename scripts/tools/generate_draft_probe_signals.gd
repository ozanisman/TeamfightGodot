extends SceneTree

## Generates simulation-derived draft probe signals.
##
## Each champion is inserted into a fixed 5-unit shell and run against fixed
## opponent templates. Mirror mode swaps sides with the same seeds and counts the
## candidate team's win from either side, removing player-slot bias.

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const SpawnSpecScript := preload("res://scripts/simulation/spawn_spec.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

const DEFAULT_OUTPUT := "res://model_stats/certified_pairwise_training_250k/draft_probe_signals.csv"
const DEFAULT_SIM_BASE_SEED: int = 3000000

const PROBE_TEMPLATES: Array[Dictionary] = [
	{"name": "balanced_default", "allies": [&"guardian", &"archer", &"wizard", &"cleric"], "enemies": [&"swordsman", &"archer", &"guardian", &"wizard", &"cleric"]},
	{"name": "tank_wall", "allies": [&"guardian", &"paladin", &"wizard", &"archer"], "enemies": [&"guardian", &"paladin", &"colossus", &"earthbender", &"mirror_knight"]},
	{"name": "burst_dive", "allies": [&"guardian", &"rogue", &"wizard", &"cleric"], "enemies": [&"rogue", &"ninja", &"wraith", &"silencer", &"berserker"]},
	{"name": "sustain_shell", "allies": [&"guardian", &"paladin", &"archer", &"wizard"], "enemies": [&"paladin", &"cleric", &"oracle", &"mistcaller", &"guardian"]},
	{"name": "cc_control", "allies": [&"guardian", &"archer", &"wizard", &"cleric"], "enemies": [&"guardian", &"colossus", &"frost_mage", &"siren", &"disarmer"]},
	{"name": "long_range_poke", "allies": [&"guardian", &"archer", &"wizard", &"cleric"], "enemies": [&"sniper", &"artillery", &"archer", &"wizard", &"windcaller"]},
	{"name": "melee_brawl", "allies": [&"guardian", &"berserker", &"cleric", &"wizard"], "enemies": [&"swordsman", &"berserker", &"monk", &"valkyrie", &"disarmer"]},
	{"name": "summon_attrition", "allies": [&"guardian", &"archer", &"cleric", &"wizard"], "enemies": [&"necromancer", &"warlock", &"mistcaller", &"paladin", &"guardian"]},
	{"name": "anti_carry", "allies": [&"guardian", &"archer", &"wizard", &"cleric"], "enemies": [&"silencer", &"disarmer", &"wraith", &"frost_mage", &"ninja"]},
	{"name": "mixed_backline", "allies": [&"guardian", &"paladin", &"cleric", &"wizard"], "enemies": [&"sniper", &"artillery", &"wizard", &"oracle", &"mistcaller"]},
	{"name": "high_ehp_frontline", "allies": [&"archer", &"wizard", &"cleric", &"siren"], "enemies": [&"colossus", &"guardian", &"earthbender", &"paladin", &"mirror_knight"]},
	{"name": "high_dps_race", "allies": [&"guardian", &"cleric", &"archer", &"wizard"], "enemies": [&"rogue", &"berserker", &"sniper", &"artillery", &"swordsman"]},
]


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _flag_enabled(prefix: String) -> bool:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg == prefix:
			return true
		if arg.begins_with(prefix + "="):
			var tail: String = arg.substr(prefix.length() + 1)
			return tail != "0" and tail.to_lower() != "false"
	return false


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	OS.set_environment("TEAMFIGHT_STATS_EXPORT_MINIMAL", "1")
	ChampionCatalogScript.build_catalog()
	ChampionCatalogScript.build_role_configs()
	ChampionCatalogScript.build_passive_registry()
	ChampionCatalogScript.build_minion_catalog()
	ChampionCatalogScript.freeze_built_specs_for_worker_reuse()

	var template_count := clampi(int(_extract_argument("--templates=", "12")), 1, PROBE_TEMPLATES.size())
	var seeds_per_template := maxi(1, int(_extract_argument("--seeds-per-template=", "100")))
	var mirror := _flag_enabled("--mirror")
	var output_path := _extract_argument("--output=", DEFAULT_OUTPUT)
	var sim_base_seed := int(_extract_argument("--sim-base-seed=", str(DEFAULT_SIM_BASE_SEED)))

	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("generate_draft_probe_signals: native simulation backend unavailable")
		quit(1)
		return
	if not backend.has_method("run_matches_stats"):
		push_error("generate_draft_probe_signals: backend missing run_matches_stats()")
		quit(1)
		return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	var templates: Array = PROBE_TEMPLATES.slice(0, template_count)
	var lines: Array[String] = [_csv_header(templates)]

	print("generate_draft_probe_signals: champions=%d templates=%d seeds/template=%d mirror=%s" % [
		champion_ids.size(), templates.size(), seeds_per_template, str(mirror)
	])

	for champion_index in range(champion_ids.size()):
		var champion: StringName = champion_ids[champion_index]
		var probe_values: Array[float] = []
		for template_index in range(templates.size()):
			var template: Dictionary = templates[template_index]
			var team: Array[StringName] = _candidate_team(champion, template["allies"], template["enemies"], champion_ids)
			var enemies: Array[StringName] = _clean_team(template["enemies"], [champion], champion_ids)
			var p: float = _run_probe(
				backend, team, enemies, champion_index, template_index, seeds_per_template, sim_base_seed, mirror
			)
			probe_values.append(p)
		lines.append(_csv_row(champion, probe_values))
		print("  %s %d/%d" % [String(champion), champion_index + 1, champion_ids.size()])

	var f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f == null:
		push_error("generate_draft_probe_signals: could not open %s" % output_path)
		quit(1)
		return
	f.store_string("\n".join(lines) + "\n")
	f.close()
	if backend.has_method("clear"):
		backend.call("clear")

	print("generate_draft_probe_signals: wrote %s" % output_path)
	quit(0)


func _candidate_team(champion: StringName, allies: Array, enemies: Array, champion_ids: Array[StringName]) -> Array[StringName]:
	var team: Array[StringName] = [champion]
	for ally in allies:
		var id := StringName(String(ally))
		if id != champion and id not in team and id not in enemies:
			team.append(id)
	_clean_fill(team, enemies, champion_ids, 5)
	return team


func _clean_team(raw_team: Array, excluded: Array, champion_ids: Array[StringName]) -> Array[StringName]:
	var team: Array[StringName] = []
	for value in raw_team:
		var id := StringName(String(value))
		if id not in team and id not in excluded:
			team.append(id)
	_clean_fill(team, excluded, champion_ids, 5)
	return team


func _clean_fill(team: Array[StringName], excluded: Array, champion_ids: Array[StringName], target_size: int) -> void:
	for id in champion_ids:
		if team.size() >= target_size:
			return
		if id in team or id in excluded:
			continue
		team.append(id)


func _run_probe(
	backend: RefCounted,
	team: Array[StringName],
	enemies: Array[StringName],
	champion_index: int,
	template_index: int,
	seeds_per_template: int,
	sim_base_seed: int,
	mirror: bool
) -> float:
	var inputs: Array = []
	inputs.resize(seeds_per_template * (2 if mirror else 1))
	for s in range(seeds_per_template):
		var sim_seed: int = sim_base_seed + champion_index * 100000 + template_index * seeds_per_template + s
		inputs[s] = MatchReplayInputScript.new(
			sim_seed,
			SimConstantsScript.DEFAULT_TICK_RATE,
			_build_units(team, &"player"),
			_build_units(enemies, &"enemy")
		)
		if mirror:
			inputs[seeds_per_template + s] = MatchReplayInputScript.new(
				sim_seed,
				SimConstantsScript.DEFAULT_TICK_RATE,
				_build_units(enemies, &"player"),
				_build_units(team, &"enemy")
			)
	var summaries_var: Variant = backend.run_matches_stats(inputs)
	if typeof(summaries_var) != TYPE_ARRAY:
		push_error("generate_draft_probe_signals: run_matches_stats returned non-array")
		return 0.5
	var summaries: Array = summaries_var
	var wins: int = 0
	var decisive: int = 0
	for i in range(summaries.size()):
		var winner := String(Dictionary(summaries[i]).get("winner_team", ""))
		if winner != "player" and winner != "enemy":
			continue
		decisive += 1
		var normal_orientation := i < seeds_per_template
		if (normal_orientation and winner == "player") or ((not normal_orientation) and winner == "enemy"):
			wins += 1
	return float(wins) / float(decisive) if decisive > 0 else 0.5


func _build_units(ids: Array[StringName], team: StringName) -> Array:
	var units: Array = []
	for index in range(ids.size()):
		var pos := SimConstantsScript.spawn_position(index, team)
		units.append(SpawnSpecScript.new(ids[index], team, pos.x, pos.y))
	return units


func _csv_header(templates: Array) -> String:
	var cols: Array[String] = ["champion"]
	for template in templates:
		cols.append("probe_%s" % String(template["name"]))
	cols.append_array(["probe_mean", "probe_min", "probe_max", "probe_variance"])
	return ",".join(cols)


func _csv_row(champion: StringName, values: Array[float]) -> String:
	var cols: Array[String] = [String(champion)]
	for value in values:
		cols.append("%.4f" % value)
	var mean := _mean(values)
	var min_value: float = values.min() if not values.is_empty() else 0.5
	var max_value: float = values.max() if not values.is_empty() else 0.5
	cols.append("%.4f" % mean)
	cols.append("%.4f" % min_value)
	cols.append("%.4f" % max_value)
	cols.append("%.4f" % _variance(values, mean))
	return ",".join(cols)


func _mean(values: Array[float]) -> float:
	if values.is_empty():
		return 0.5
	var sum: float = 0.0
	for value in values:
		sum += value
	return sum / float(values.size())


func _variance(values: Array[float], mean: float) -> float:
	if values.size() < 2:
		return 0.0
	var sum_sq: float = 0.0
	for value in values:
		var delta: float = value - mean
		sum_sq += delta * delta
	return sum_sq / float(values.size())
