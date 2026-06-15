class_name StatsCsvAggregator
extends RefCounted

## Rolls up native match summaries into CSV rows matching [StatsDashboardLoader].

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const MatchupAggregatorScript := preload("res://scripts/tools/matchup_aggregator.gd")

var _by_size: Dictionary = {}
var _role_by_hero: Dictionary = {}
var _match_logs: Array = []
var _matchup_aggregator: MatchupAggregatorScript
var _write_match_log: bool = true


func reset() -> void:
	_by_size.clear()
	_role_by_hero.clear()
	_match_logs.clear()
	if _matchup_aggregator == null:
		_matchup_aggregator = MatchupAggregatorScript.new()
	else:
		_matchup_aggregator.reset()


func set_write_match_log(enabled: bool) -> void:
	_write_match_log = enabled
	if not _write_match_log:
		_match_logs.clear()


func preload_roles(role_by_hero: Dictionary) -> void:
	for k in role_by_hero.keys():
		_role_by_hero[String(k)] = String(role_by_hero[k])


func consume_summary(team_size: int, summary_value: Variant) -> void:
	if summary_value is Dictionary and summary_value.has("match_results"):
		if _matchup_aggregator == null:
			_matchup_aggregator = MatchupAggregatorScript.new()
		_matchup_aggregator.consume_chunk_result(summary_value)

		var match_results: Array = Array(summary_value.get("match_results", []))
		for result in match_results:
			_consume_individual_summary(team_size, result)
	elif summary_value is Dictionary and summary_value.has("stats_partial"):
		var partial_entry: Dictionary = Dictionary(summary_value)
		if _matchup_aggregator == null:
			_matchup_aggregator = MatchupAggregatorScript.new()
		_matchup_aggregator.consume_chunk_result(partial_entry)
		consume_partial(Dictionary(partial_entry.get("stats_partial", {})))
	else:
		_consume_individual_summary(team_size, summary_value)


func get_match_logs() -> Array:
	return _match_logs.duplicate(true)


func to_partial_dict(include_match_log: bool = true) -> Dictionary:
	return {
		"by_size": _by_size.duplicate(true),
		"match_logs": _match_logs.duplicate(true) if include_match_log else [],
	}


func consume_partial(partial: Dictionary) -> void:
	var partial_by_size: Dictionary = Dictionary(partial.get("by_size", {}))
	for size_key in partial_by_size.keys():
		var source_bucket: Dictionary = Dictionary(partial_by_size[size_key])
		var target_bucket: Dictionary = _bucket(int(size_key))
		for count_key in ["p1", "p2", "draws", "total"]:
			target_bucket[count_key] = int(target_bucket.get(count_key, 0)) + int(source_bucket.get(count_key, 0))
		_merge_named_entries(target_bucket, "heroes", Dictionary(source_bucket.get("heroes", {})))
		_merge_named_entries(target_bucket, "roles", Dictionary(source_bucket.get("roles", {})))
		_merge_named_entries(target_bucket, "combos", Dictionary(source_bucket.get("combos", {})))
	if _write_match_log:
		for log_entry in Array(partial.get("match_logs", [])):
			_match_logs.append(log_entry)


static func _merge_numeric_fields(target: Dictionary, source: Dictionary) -> void:
	for field in source.keys():
		if not target.has(field):
			target[field] = source[field]
			continue
		var current_value: Variant = target[field]
		var source_value: Variant = source[field]
		if current_value is int and source_value is int:
			target[field] = int(current_value) + int(source_value)
		else:
			target[field] = float(current_value) + float(source_value)


static func _merge_named_entries(target_bucket: Dictionary, group_name: String, source_group: Dictionary) -> void:
	var target_group: Dictionary = Dictionary(target_bucket[group_name])
	for name_key in source_group.keys():
		var source_entry: Dictionary = Dictionary(source_group[name_key])
		var name: String = String(name_key)
		if target_group.has(name):
			var target_entry: Dictionary = Dictionary(target_group[name])
			_merge_numeric_fields(target_entry, source_entry)
			target_group[name] = target_entry
		else:
			target_group[name] = source_entry.duplicate(true)
	target_bucket[group_name] = target_group

static func _unit_float(data: Dictionary, key: String, default_value: float = 0.0) -> float:
	return float(data.get(key, default_value))


static func _unit_int(data: Dictionary, key: String, default_value: int = 0) -> int:
	return int(data.get(key, default_value))


func _consume_individual_summary(team_size: int, summary_value: Variant) -> void:
	if summary_value is Dictionary:
		_consume_individual_summary_dict(team_size, Dictionary(summary_value))
		return
	var summary: Object = summary_value as Object
	if summary == null:
		push_error("StatsCsvAggregator: bad summary")
		return
	_consume_individual_summary_object(team_size, summary)


func _consume_individual_summary_object(team_size: int, summary: Object) -> void:
	var seed: int = int(summary.seed)
	var winner_team: StringName = summary.winner_team
	var sudden_death_ticks: int = int(summary.sudden_death_ticks)
	var duration: float = float(summary.duration)
	var unit_stats: Array = Array(summary.unit_stats)
	var player_comp: Array = Array(summary.player_comp)
	var enemy_comp: Array = Array(summary.enemy_comp)
	_consume_individual_summary_common(team_size, seed, winner_team, sudden_death_ticks, duration, unit_stats, player_comp, enemy_comp, true)


func _consume_individual_summary_dict(team_size: int, summary: Dictionary) -> void:
	var seed: int = int(summary.get("seed", 0))
	var winner_value: Variant = summary.get("winner_team", "")
	var winner_team: StringName = StringName(String(winner_value))
	var sudden_death_ticks: int = int(summary.get("sudden_death_ticks", 0))
	var duration: float = float(summary.get("duration", 0.0))
	var unit_stats: Array = Array(summary.get("unit_stats", []))
	var player_comp: Array = Array(summary.get("player_comp", []))
	var enemy_comp: Array = Array(summary.get("enemy_comp", []))
	_consume_individual_summary_common(team_size, seed, winner_team, sudden_death_ticks, duration, unit_stats, player_comp, enemy_comp, false)


func _consume_individual_summary_common(
	team_size: int,
	seed: int,
	winner_team: StringName,
	sudden_death_ticks: int,
	duration: float,
	unit_stats: Array,
	player_comp: Array,
	enemy_comp: Array,
	summary_is_object: bool
) -> void:
	if _write_match_log:
		_match_logs.append({
			"team_size": team_size,
			"seed": seed,
			"winner": String(winner_team),
			"sudden_death_ticks": sudden_death_ticks,
			"duration": duration,
			"player_comp": player_comp.duplicate(),
			"enemy_comp": enemy_comp.duplicate(),
		})

	_ensure_roles()
	var bucket: Dictionary = _bucket(team_size)
	var wt: StringName = winner_team
	if wt == &"player":
		bucket["p1"] = int(bucket["p1"]) + 1
	elif wt == &"enemy":
		bucket["p2"] = int(bucket["p2"]) + 1
	else:
		bucket["draws"] = int(bucket["draws"]) + 1
	bucket["total"] = int(bucket["total"]) + 1

	for item in unit_stats:
		if item is Dictionary:
			var u: Dictionary = Dictionary(item)
			var hero: String = String(u.get("unit_id", u.get("archetype", "")))
			if hero.is_empty():
				continue
			# Skip minions - stats are for champions only
			var role: String = str(u.get("role", ""))
			if role.to_lower() == "minion":
				continue
			_acc_hero_dict(bucket, hero, wt, u)
			role = str(_role_by_hero.get(hero, "unknown"))
			_acc_role_dict(bucket, role, wt, u)
		elif summary_is_object and item is Object:
			var uo: Object = item as Object
			if uo == null:
				continue
			var hero_obj: String = String(uo.unit_id)
			if hero_obj.is_empty():
				continue
			# Skip minions - stats are for champions only
			var role_obj: String = str(uo.role) if uo.has_property("role") else ""
			if role_obj.to_lower() == "minion":
				continue
			_acc_hero(bucket, hero_obj, wt, uo)
			role_obj = str(_role_by_hero.get(hero_obj, "unknown"))
			_acc_role(bucket, role_obj, wt, uo)

	if team_size > 1:
		_acc_combo(bucket, player_comp, enemy_comp, wt)


func write_to_dir(dir_path: String) -> Error:
	if _by_size.is_empty():
		return ERR_INVALID_DATA
	var abs_base := ProjectSettings.globalize_path(dir_path)
	var mk := _ensure_output_dir(dir_path)
	if mk != OK and mk != ERR_ALREADY_EXISTS:
		push_error("StatsCsvAggregator.write_to_dir: mkdir failed %s for %s" % [error_string(mk), abs_base])
		return mk
	var summary_csv := _build_summary_csv()
	var hero_csv := _build_combat_csv()
	var role_csv := _build_role_csv()
	var combo_csv := _build_combo_csv()
	var csv_files: Array = [
		["summary_stats.csv", summary_csv],
		["combat_stats.csv", hero_csv],
		["role_stats.csv", role_csv],
		["role_combinations.csv", combo_csv],
	]
	if _write_match_log:
		csv_files.append(["match_log.csv", _build_match_log_csv()])
	for pair: Array in csv_files:
		var name: String = String(pair[0])
		var text: String = String(pair[1])
		var err := _write_text_atomic("%s/%s" % [dir_path.rstrip("/"), name], text)
		if err != OK:
			push_error("StatsCsvAggregator.write_to_dir: write failed %s for %s" % [error_string(err), name])
			return err
	return OK


static func _ensure_output_dir(dir_path: String) -> Error:
	var abs_path := ProjectSettings.globalize_path(dir_path)
	var mk := DirAccess.make_dir_recursive_absolute(abs_path)
	if DirAccess.dir_exists_absolute(abs_path):
		return OK
	if dir_path.begins_with("user://"):
		var user_dir := DirAccess.open("user://")
		if user_dir == null:
			return ERR_CANT_OPEN
		return user_dir.make_dir_recursive(dir_path.substr("user://".length()))
	if dir_path.begins_with("res://"):
		var res_dir := DirAccess.open("res://")
		if res_dir == null:
			return ERR_CANT_OPEN
		return res_dir.make_dir_recursive(dir_path.substr("res://".length()))
	return mk


static func _path_parent(path: String) -> String:
	var slash := path.rfind("/")
	if slash < 0:
		return ""
	return path.substr(0, slash)


func _bucket(sz: int) -> Dictionary:
	if not _by_size.has(sz):
		_by_size[sz] = {
			"p1": 0,
			"p2": 0,
			"draws": 0,
			"total": 0,
			"heroes": {},
			"roles": {},
			"combos": {},
		}
	return _by_size[sz]


func _ensure_roles() -> void:
	if not _role_by_hero.is_empty():
		return
	var cat: Dictionary = ChampionCatalogScript.build_catalog()
	for k in cat.keys():
		var spec: Variant = cat[k]
		_role_by_hero[String(k)] = String(spec.stats.role)


func _hero_entry(bucket: Dictionary, hero: String) -> Dictionary:
	var heroes: Dictionary = bucket["heroes"]
	if not heroes.has(hero):
		heroes[hero] = {
			"w": 0,
			"l": 0,
			"d": 0,
			"dmg_d": 0.0,
			"dmg_r": 0.0,
			"dmg_m": 0.0,
			"heal": 0.0,
			"heal_auto": 0.0,
			"heal_ability": 0.0,
			"heal_ultimate": 0.0,
			"heal_passive": 0.0,
			"shield": 0.0,
			"shield_auto": 0.0,
			"shield_ability": 0.0,
			"shield_ultimate": 0.0,
			"shield_passive": 0.0,
			"stuns": 0,
			"kills": 0,
			"deaths": 0,
			"assists": 0,
			"d_auto": 0.0,
			"d_ab": 0.0,
			"d_ult": 0.0,
			"d_passive": 0.0,
			"minion_dmg_d": 0.0,
			"minion_dmg_r": 0.0,
			"minion_dmg_m": 0.0,
		}
	return heroes[hero]


func _acc_hero(bucket: Dictionary, hero: String, wt: StringName, u: Object) -> void:
	var h: Dictionary = _hero_entry(bucket, hero)
	if wt == &"draw":
		h["d"] = int(h["d"]) + 1
	elif bool(u.won):
		h["w"] = int(h["w"]) + 1
	else:
		h["l"] = int(h["l"]) + 1
	h["dmg_d"] = float(h["dmg_d"]) + float(u.damage_dealt)
	h["dmg_r"] = float(h["dmg_r"]) + float(u.damage_received)
	h["dmg_m"] = float(h["dmg_m"]) + float(u.damage_mitigated)
	h["heal"] = float(h["heal"]) + float(u.healing_done)
	h["heal_auto"] = float(h["heal_auto"]) + float(u.healing_done_auto)
	h["heal_ability"] = float(h["heal_ability"]) + float(u.healing_done_ability)
	h["heal_ultimate"] = float(h["heal_ultimate"]) + float(u.healing_done_ultimate)
	h["heal_passive"] = float(h["heal_passive"]) + float(u.healing_done_passive)
	h["shield"] = float(h["shield"]) + float(u.shielding_done)
	h["shield_auto"] = float(h["shield_auto"]) + float(u.shielding_done_auto)
	h["shield_ability"] = float(h["shield_ability"]) + float(u.shielding_done_ability)
	h["shield_ultimate"] = float(h["shield_ultimate"]) + float(u.shielding_done_ultimate)
	h["shield_passive"] = float(h["shield_passive"]) + float(u.shielding_done_passive)
	h["stuns"] = int(h["stuns"]) + int(u.stuns)
	h["kills"] = int(h["kills"]) + int(u.kills)
	h["deaths"] = int(h["deaths"]) + int(u.deaths)
	h["assists"] = int(h["assists"]) + int(u.assists)
	h["d_auto"] = float(h["d_auto"]) + float(u.damage_dealt_auto)
	h["d_ab"] = float(h["d_ab"]) + float(u.damage_dealt_ability)
	h["d_ult"] = float(h["d_ult"]) + float(u.damage_dealt_ultimate)
	h["d_passive"] = float(h["d_passive"]) + float(u.damage_dealt_passive)
	var minion_dmg_d_val = u.get("minion_damage_dealt")
	h["minion_dmg_d"] = float(h["minion_dmg_d"]) + (float(minion_dmg_d_val) if minion_dmg_d_val != null else 0.0)
	var minion_dmg_r_val = u.get("minion_damage_received")
	h["minion_dmg_r"] = float(h["minion_dmg_r"]) + (float(minion_dmg_r_val) if minion_dmg_r_val != null else 0.0)
	var minion_dmg_m_val = u.get("minion_damage_mitigated")
	h["minion_dmg_m"] = float(h["minion_dmg_m"]) + (float(minion_dmg_m_val) if minion_dmg_m_val != null else 0.0)


func _acc_hero_dict(bucket: Dictionary, hero: String, wt: StringName, u: Dictionary) -> void:
	var h: Dictionary = _hero_entry(bucket, hero)
	if wt == &"draw":
		h["d"] = int(h["d"]) + 1
	elif bool(u.get("won", false)):
		h["w"] = int(h["w"]) + 1
	else:
		h["l"] = int(h["l"]) + 1
	h["dmg_d"] = float(h["dmg_d"]) + _unit_float(u, "damage_dealt")
	h["dmg_r"] = float(h["dmg_r"]) + _unit_float(u, "damage_received")
	h["dmg_m"] = float(h["dmg_m"]) + _unit_float(u, "damage_mitigated")
	h["heal"] = float(h["heal"]) + _unit_float(u, "healing_done")
	h["heal_auto"] = float(h["heal_auto"]) + _unit_float(u, "healing_done_auto")
	h["heal_ability"] = float(h["heal_ability"]) + _unit_float(u, "healing_done_ability")
	h["heal_ultimate"] = float(h["heal_ultimate"]) + _unit_float(u, "healing_done_ultimate")
	h["heal_passive"] = float(h["heal_passive"]) + _unit_float(u, "healing_done_passive")
	h["shield"] = float(h["shield"]) + _unit_float(u, "shielding_done")
	h["shield_auto"] = float(h["shield_auto"]) + _unit_float(u, "shielding_done_auto")
	h["shield_ability"] = float(h["shield_ability"]) + _unit_float(u, "shielding_done_ability")
	h["shield_ultimate"] = float(h["shield_ultimate"]) + _unit_float(u, "shielding_done_ultimate")
	h["shield_passive"] = float(h["shield_passive"]) + _unit_float(u, "shielding_done_passive")
	h["stuns"] = int(h["stuns"]) + _unit_int(u, "stuns")
	h["kills"] = int(h["kills"]) + _unit_int(u, "kills")
	h["deaths"] = int(h["deaths"]) + _unit_int(u, "deaths")
	h["assists"] = int(h["assists"]) + _unit_int(u, "assists")
	h["d_auto"] = float(h["d_auto"]) + _unit_float(u, "damage_dealt_auto")
	h["d_ab"] = float(h["d_ab"]) + _unit_float(u, "damage_dealt_ability")
	h["d_ult"] = float(h["d_ult"]) + _unit_float(u, "damage_dealt_ultimate")
	h["d_passive"] = float(h["d_passive"]) + _unit_float(u, "damage_dealt_passive")
	h["minion_dmg_d"] = float(h["minion_dmg_d"]) + _unit_float(u, "minion_damage_dealt")
	h["minion_dmg_r"] = float(h["minion_dmg_r"]) + _unit_float(u, "minion_damage_received")
	h["minion_dmg_m"] = float(h["minion_dmg_m"]) + _unit_float(u, "minion_damage_mitigated")


func _role_entry(bucket: Dictionary, role: String) -> Dictionary:
	var roles: Dictionary = bucket["roles"]
	if not roles.has(role):
		roles[role] = {
			"w": 0,
			"l": 0,
			"d": 0,
			"dmg_d": 0.0,
			"dmg_r": 0.0,
			"dmg_m": 0.0,
			"heal": 0.0,
			"heal_auto": 0.0,
			"heal_ability": 0.0,
			"heal_ultimate": 0.0,
			"heal_passive": 0.0,
			"shield": 0.0,
			"shield_auto": 0.0,
			"shield_ability": 0.0,
			"shield_ultimate": 0.0,
			"shield_passive": 0.0,
			"stuns": 0,
			"d_auto": 0.0,
			"d_ab": 0.0,
			"d_ult": 0.0,
			"d_passive": 0.0,
		}
	return roles[role]


func _acc_role(bucket: Dictionary, role: String, wt: StringName, u: Object) -> void:
	var r: Dictionary = _role_entry(bucket, role)
	if wt == &"draw":
		r["d"] = int(r["d"]) + 1
	elif bool(u.won):
		r["w"] = int(r["w"]) + 1
	else:
		r["l"] = int(r["l"]) + 1
	r["dmg_d"] = float(r["dmg_d"]) + float(u.damage_dealt)
	r["dmg_r"] = float(r["dmg_r"]) + float(u.damage_received)
	r["dmg_m"] = float(r["dmg_m"]) + float(u.damage_mitigated)
	r["heal"] = float(r["heal"]) + float(u.healing_done)
	r["heal_auto"] = float(r["heal_auto"]) + float(u.healing_done_auto)
	r["heal_ability"] = float(r["heal_ability"]) + float(u.healing_done_ability)
	r["heal_ultimate"] = float(r["heal_ultimate"]) + float(u.healing_done_ultimate)
	r["heal_passive"] = float(r["heal_passive"]) + float(u.healing_done_passive)
	r["shield"] = float(r["shield"]) + float(u.shielding_done)
	r["shield_auto"] = float(r["shield_auto"]) + float(u.shielding_done_auto)
	r["shield_ability"] = float(r["shield_ability"]) + float(u.shielding_done_ability)
	r["shield_ultimate"] = float(r["shield_ultimate"]) + float(u.shielding_done_ultimate)
	r["shield_passive"] = float(r["shield_passive"]) + float(u.shielding_done_passive)
	r["stuns"] = int(r["stuns"]) + int(u.stuns)
	r["d_auto"] = float(r["d_auto"]) + float(u.damage_dealt_auto)
	r["d_ab"] = float(r["d_ab"]) + float(u.damage_dealt_ability)
	r["d_ult"] = float(r["d_ult"]) + float(u.damage_dealt_ultimate)
	r["d_passive"] = float(r["d_passive"]) + float(u.damage_dealt_passive)


func _acc_role_dict(bucket: Dictionary, role: String, wt: StringName, u: Dictionary) -> void:
	var r: Dictionary = _role_entry(bucket, role)
	if wt == &"draw":
		r["d"] = int(r["d"]) + 1
	elif bool(u.get("won", false)):
		r["w"] = int(r["w"]) + 1
	else:
		r["l"] = int(r["l"]) + 1
	r["dmg_d"] = float(r["dmg_d"]) + _unit_float(u, "damage_dealt")
	r["dmg_r"] = float(r["dmg_r"]) + _unit_float(u, "damage_received")
	r["dmg_m"] = float(r["dmg_m"]) + _unit_float(u, "damage_mitigated")
	r["heal"] = float(r["heal"]) + _unit_float(u, "healing_done")
	r["heal_auto"] = float(r["heal_auto"]) + _unit_float(u, "healing_done_auto")
	r["heal_ability"] = float(r["heal_ability"]) + _unit_float(u, "healing_done_ability")
	r["heal_ultimate"] = float(r["heal_ultimate"]) + _unit_float(u, "healing_done_ultimate")
	r["heal_passive"] = float(r["heal_passive"]) + _unit_float(u, "healing_done_passive")
	r["shield"] = float(r["shield"]) + _unit_float(u, "shielding_done")
	r["shield_auto"] = float(r["shield_auto"]) + _unit_float(u, "shielding_done_auto")
	r["shield_ability"] = float(r["shield_ability"]) + _unit_float(u, "shielding_done_ability")
	r["shield_ultimate"] = float(r["shield_ultimate"]) + _unit_float(u, "shielding_done_ultimate")
	r["shield_passive"] = float(r["shield_passive"]) + _unit_float(u, "shielding_done_passive")
	r["stuns"] = int(r["stuns"]) + _unit_int(u, "stuns")
	r["d_auto"] = float(r["d_auto"]) + _unit_float(u, "damage_dealt_auto")
	r["d_ab"] = float(r["d_ab"]) + _unit_float(u, "damage_dealt_ability")
	r["d_ult"] = float(r["d_ult"]) + _unit_float(u, "damage_dealt_ultimate")
	r["d_passive"] = float(r["d_passive"]) + _unit_float(u, "damage_dealt_passive")


func _acc_combo(bucket: Dictionary, player_comp: Array, enemy_comp: Array, wt: StringName) -> void:
	# Build role fingerprint for player team
	var player_roles: Array[String] = []
	for c in player_comp:
		var hero: String = String(c)
		player_roles.append(str(_role_by_hero.get(hero, "unknown")))
	player_roles.sort()
	var player_role_fp := ""
	for i in player_roles.size():
		if i > 0:
			player_role_fp += " + "
		player_role_fp += player_roles[i]

	# Build role fingerprint for enemy team
	var enemy_roles: Array[String] = []
	for c in enemy_comp:
		var hero: String = String(c)
		enemy_roles.append(str(_role_by_hero.get(hero, "unknown")))
	enemy_roles.sort()
	var enemy_role_fp := ""
	for i in enemy_roles.size():
		if i > 0:
			enemy_role_fp += " + "
		enemy_role_fp += enemy_roles[i]

	var combos: Dictionary = bucket["combos"]

	# Record player team fingerprint
	if not combos.has(player_role_fp):
		combos[player_role_fp] = {"w": 0, "n": 0}
	combos[player_role_fp]["n"] = int(combos[player_role_fp]["n"]) + 1
	if wt == &"player":
		combos[player_role_fp]["w"] = int(combos[player_role_fp]["w"]) + 1

	# Record enemy team fingerprint
	if not combos.has(enemy_role_fp):
		combos[enemy_role_fp] = {"w": 0, "n": 0}
	combos[enemy_role_fp]["n"] = int(combos[enemy_role_fp]["n"]) + 1
	if wt == &"enemy":
		combos[enemy_role_fp]["w"] = int(combos[enemy_role_fp]["w"]) + 1


func _flip_winner(wt: StringName) -> StringName:
	if wt == &"player":
		return &"enemy"
	elif wt == &"enemy":
		return &"player"
	return wt


func _fmt_f(v: float) -> String:
	return String.num(snappedf(v, 0.000001), 6).trim_suffix("0").trim_suffix(".")


func _csv_cell(raw: String) -> String:
	if raw.contains(",") or raw.contains("\"") or raw.contains("\n") or raw.contains("\r"):
		return "\"" + raw.replace("\"", "\"\"") + "\""
	return raw


func _write_text_atomic(res_path: String, content: String) -> Error:
	var abs_path := ProjectSettings.globalize_path(res_path)
	var abs_tmp := abs_path + ".tmp"
	var mk := _ensure_output_dir(_path_parent(res_path))
	if mk != OK and mk != ERR_ALREADY_EXISTS:
		return mk
	var f := FileAccess.open(abs_tmp, FileAccess.WRITE)
	if f == null:
		var open_err := FileAccess.get_open_error()
		push_error("StatsCsvAggregator._write_text_atomic: temp open failed %s for %s" % [error_string(open_err), abs_tmp])
		return open_err
	f.store_string(content)
	f.flush()
	f.close()
	if FileAccess.file_exists(abs_path):
		var rm := DirAccess.remove_absolute(abs_path)
		if rm != OK:
			return rm
	var ren := DirAccess.rename_absolute(abs_tmp, abs_path)
	if ren == OK:
		return OK
	var direct := FileAccess.open(abs_path, FileAccess.WRITE)
	if direct == null:
		push_error("StatsCsvAggregator._write_text_atomic: rename failed %s and direct open failed %s for %s" % [error_string(ren), error_string(FileAccess.get_open_error()), abs_path])
		return ren
	direct.store_string(content)
	direct.flush()
	direct.close()
	DirAccess.remove_absolute(abs_tmp)
	return OK


func _build_summary_csv() -> String:
	var lines: PackedStringArray = PackedStringArray()
	lines.append("team_size,p1_wins,p2_wins,draws,total_rounds")
	var sizes: Array = _by_size.keys()
	sizes.sort()
	for sz in sizes:
		var b: Dictionary = _by_size[sz]
		lines.append(
			"%d,%d,%d,%d,%d"
			% [int(sz), int(b["p1"]), int(b["p2"]), int(b["draws"]), int(b["total"])]
		)
	return "\n".join(lines) + "\n"


func _build_combat_csv() -> String:
	var lines: PackedStringArray = PackedStringArray()
	lines.append(
		"team_size,hero,role,wins,losses,draws,total_games,win_rate,avg_dmg_dealt,avg_dmg_received,avg_dmg_mitigated,avg_healing,avg_healing_auto,avg_healing_ability,avg_healing_ultimate,avg_healing_passive,avg_shielding,avg_shielding_auto,avg_shielding_ability,avg_shielding_ultimate,avg_shielding_passive,avg_stuns,avg_kills,avg_deaths,avg_assists,kda,avg_dmg_auto,avg_dmg_ability,avg_dmg_ultimate,avg_dmg_passive,avg_minion_dmg_dealt,avg_minion_dmg_received,avg_minion_dmg_mitigated"
	)
	var sizes: Array = _by_size.keys()
	sizes.sort()
	for sz in sizes:
		var heroes: Dictionary = _by_size[sz]["heroes"]
		var hero_names: Array = heroes.keys()
		hero_names.sort()
		for hero in hero_names:
			var h: Dictionary = heroes[hero]
			var w: int = int(h["w"])
			var l: int = int(h["l"])
			var d: int = int(h["d"])
			var tg: int = w + l + d
			if tg <= 0:
				continue
			var wr: float = float(w) / float(tg)
			var kda: float = (float(h["kills"]) + float(h["assists"])) / maxf(1.0, float(h["deaths"]))
			lines.append(
				"%d,%s,%s,%d,%d,%d,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s"
				% [
					int(sz),
					_csv_cell(String(hero)),
					_csv_cell(str(_role_by_hero.get(hero, "unknown"))),
					w,
					l,
					d,
					tg,
					_fmt_f(wr),
					_fmt_f(float(h["dmg_d"]) / float(tg)),
					_fmt_f(float(h["dmg_r"]) / float(tg)),
					_fmt_f(float(h["dmg_m"]) / float(tg)),
					_fmt_f(float(h["heal"]) / float(tg)),
					_fmt_f(float(h["heal_auto"]) / float(tg)),
					_fmt_f(float(h["heal_ability"]) / float(tg)),
					_fmt_f(float(h["heal_ultimate"]) / float(tg)),
					_fmt_f(float(h["heal_passive"]) / float(tg)),
					_fmt_f(float(h["shield"]) / float(tg)),
					_fmt_f(float(h["shield_auto"]) / float(tg)),
					_fmt_f(float(h["shield_ability"]) / float(tg)),
					_fmt_f(float(h["shield_ultimate"]) / float(tg)),
					_fmt_f(float(h["shield_passive"]) / float(tg)),
					_fmt_f(float(h["stuns"]) / float(tg)),
					_fmt_f(float(h["kills"]) / float(tg)),
					_fmt_f(float(h["deaths"]) / float(tg)),
					_fmt_f(float(h["assists"]) / float(tg)),
					_fmt_f(kda),
					_fmt_f(float(h["d_auto"]) / float(tg)),
					_fmt_f(float(h["d_ab"]) / float(tg)),
					_fmt_f(float(h["d_ult"]) / float(tg)),
					_fmt_f(float(h["d_passive"]) / float(tg)),
					_fmt_f(float(h.get("minion_dmg_d", 0.0)) / float(tg)),
					_fmt_f(float(h.get("minion_dmg_r", 0.0)) / float(tg)),
					_fmt_f(float(h.get("minion_dmg_m", 0.0)) / float(tg)),
				]
			)
	return "\n".join(lines) + "\n"


func _build_role_csv() -> String:
	var lines: PackedStringArray = PackedStringArray()
	lines.append(
		"team_size,role,wins,losses,draws,total_games,win_rate,avg_dmg_dealt,avg_dmg_received,avg_dmg_mitigated,avg_healing,avg_healing_auto,avg_healing_ability,avg_healing_ultimate,avg_healing_passive,avg_shielding,avg_shielding_auto,avg_shielding_ability,avg_shielding_ultimate,avg_shielding_passive,avg_stuns,avg_dmg_auto,avg_dmg_ability,avg_dmg_ultimate,avg_dmg_passive"
	)
	var sizes: Array = _by_size.keys()
	sizes.sort()
	for sz in sizes:
		var roles: Dictionary = _by_size[sz]["roles"]
		var role_names: Array = roles.keys()
		role_names.sort()
		for role in role_names:
			var r: Dictionary = roles[role]
			var w: int = int(r["w"])
			var l: int = int(r["l"])
			var d: int = int(r["d"])
			var tg: int = w + l + d
			if tg <= 0:
				continue
			var wr: float = float(w) / float(tg)
			lines.append(
				"%d,%s,%d,%d,%d,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s"
				% [
					int(sz),
					_csv_cell(String(role)),
					w,
					l,
					d,
					tg,
					_fmt_f(wr),
					_fmt_f(float(r["dmg_d"]) / float(tg)),
					_fmt_f(float(r["dmg_r"]) / float(tg)),
					_fmt_f(float(r["dmg_m"]) / float(tg)),
					_fmt_f(float(r["heal"]) / float(tg)),
					_fmt_f(float(r["heal_auto"]) / float(tg)),
					_fmt_f(float(r["heal_ability"]) / float(tg)),
					_fmt_f(float(r["heal_ultimate"]) / float(tg)),
					_fmt_f(float(r["heal_passive"]) / float(tg)),
					_fmt_f(float(r["shield"]) / float(tg)),
					_fmt_f(float(r["shield_auto"]) / float(tg)),
					_fmt_f(float(r["shield_ability"]) / float(tg)),
					_fmt_f(float(r["shield_ultimate"]) / float(tg)),
					_fmt_f(float(r["shield_passive"]) / float(tg)),
					_fmt_f(float(r["stuns"]) / float(tg)),
					_fmt_f(float(r["d_auto"]) / float(tg)),
					_fmt_f(float(r["d_ab"]) / float(tg)),
					_fmt_f(float(r["d_ult"]) / float(tg)),
					_fmt_f(float(r["d_passive"]) / float(tg)),
				]
			)
	return "\n".join(lines) + "\n"


func _build_combo_csv() -> String:
	var lines: PackedStringArray = PackedStringArray()
	lines.append("team_size,role_fingerprint,win_rate,wins,total_games")
	var sizes: Array = _by_size.keys()
	sizes.sort()
	for sz in sizes:
		if int(sz) <= 1:
			continue
		var combos: Dictionary = _by_size[sz]["combos"]
		var keys: Array = combos.keys()
		keys.sort()
		for ck in keys:
			var c: Dictionary = combos[ck]
			var n: int = int(c["n"])
			var w: int = int(c["w"])
			if n <= 0:
				continue
			var wr: float = float(w) / float(n)
			lines.append(
				"%d,%s,%s,%d,%d" % [int(sz), _csv_cell(String(ck)), _fmt_f(wr), w, n]
			)
	return "\n".join(lines) + "\n"


func _build_match_log_csv() -> String:
	var lines: PackedStringArray = PackedStringArray()
	lines.append("team_size,seed,winner,sudden_death_ticks,duration")
	for log in _match_logs:
		lines.append(
			"%d,%d,%s,%d,%s"
			% [
				int(log.team_size),
				int(log.seed),
				_csv_cell(String(log.winner)),
				int(log.sudden_death_ticks),
				_fmt_f(float(log.duration)),
			]
		)
	return "\n".join(lines) + "\n"

func write_matchup_file(output_dir: String) -> bool:
	if _matchup_aggregator == null:
		return false

	return _matchup_aggregator.write_matchup_csv_files(output_dir)
