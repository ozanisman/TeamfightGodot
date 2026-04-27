class_name StatsCsvAggregator
extends RefCounted

## Rolls up native match summaries into CSV rows matching [StatsDashboardLoader].

const MatchReplaySummaryScript := preload("res://scripts/simulation/match_replay_summary.gd")
const UnitReplaySummaryScript := preload("res://scripts/simulation/unit_replay_summary.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")

var _by_size: Dictionary = {}
var _role_by_hero: Dictionary = {}
var _match_logs: Array = []


func reset() -> void:
	_by_size.clear()
	_role_by_hero.clear()
	_match_logs.clear()


func consume_summary(team_size: int, summary_value: Variant) -> void:
	var summary: Object
	if summary_value is Dictionary:
		summary = MatchReplaySummaryScript.from_dict(summary_value)
	else:
		summary = summary_value as Object
	if summary == null:
		push_error("StatsCsvAggregator: bad summary")
		return
	
	# Log match details for debugging
	_match_logs.append({
		"team_size": team_size,
		"seed": summary.seed,
		"winner": String(summary.winner_team),
		"sudden_death_ticks": summary.sudden_death_ticks,
		"duration": summary.duration,
	})
	
	_ensure_roles()
	var bucket: Dictionary = _bucket(team_size)
	var wt: StringName = summary.winner_team
	if wt == &"player":
		bucket["p1"] = int(bucket["p1"]) + 1
	elif wt == &"enemy":
		bucket["p2"] = int(bucket["p2"]) + 1
	else:
		bucket["draws"] = int(bucket["draws"]) + 1
	bucket["total"] = int(bucket["total"]) + 1

	for item in Array(summary.unit_stats):
		var u: Object = _coerce_unit(item)
		if u == null:
			continue
		var hero: String = String(u.archetype_id)
		if hero.is_empty():
			continue
		_acc_hero(bucket, hero, wt, u)
		var role: String = str(_role_by_hero.get(hero, "unknown"))
		_acc_role(bucket, role, wt, u)

	if team_size > 1:
		_acc_combo(bucket, summary.player_comp, wt)


func _coerce_unit(item: Variant) -> Object:
	if item is Dictionary:
		var ud := Dictionary(item)
		var u: Object = UnitReplaySummaryScript.new()
		var archetype_value = ud.get("archetype", ud.get("archetype_id", ""))
		u.archetype_id = StringName(String(archetype_value))
		u.team = StringName(String(ud.get("team", "")))
		u.won = bool(ud.get("won", false))
		u.damage_dealt = float(ud.get("damage_dealt", 0.0))
		u.damage_dealt_auto = float(ud.get("damage_dealt_auto", 0.0))
		u.damage_dealt_ability = float(ud.get("damage_dealt_ability", 0.0))
		u.damage_dealt_ultimate = float(ud.get("damage_dealt_ultimate", 0.0))
		u.damage_received = float(ud.get("damage_received", 0.0))
		u.damage_mitigated = float(ud.get("damage_mitigated", 0.0))
		u.healing_done = float(ud.get("healing_done", 0.0))
		u.shielding_done = float(ud.get("shielding_done", 0.0))
		u.stuns = int(ud.get("stuns", 0))
		u.kills = int(ud.get("kills", 0))
		u.deaths = int(ud.get("deaths", 0))
		u.assists = int(ud.get("assists", 0))
		return u
	if item is Object:
		return item as Object
	return null


func write_to_dir(dir_path: String) -> Error:
	if _by_size.is_empty():
		return ERR_INVALID_DATA
	var abs_base := ProjectSettings.globalize_path(dir_path)
	var mk := DirAccess.make_dir_recursive_absolute(abs_base)
	if mk != OK and mk != ERR_ALREADY_EXISTS:
		return mk
	var summary_csv := _build_summary_csv()
	var hero_csv := _build_combat_csv()
	var role_csv := _build_role_csv()
	var combo_csv := _build_combo_csv()
	var match_log_csv := _build_match_log_csv()
	for pair: Array in [
		["summary_stats.csv", summary_csv],
		["combat_stats.csv", hero_csv],
		["role_stats.csv", role_csv],
		["hero_combinations.csv", combo_csv],
		["match_log.csv", match_log_csv],
	]:
		var name: String = String(pair[0])
		var text: String = String(pair[1])
		var err := _write_text_atomic("%s/%s" % [dir_path.rstrip("/"), name], text)
		if err != OK:
			return err
	return OK


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
			"shield": 0.0,
			"stuns": 0,
			"kills": 0,
			"deaths": 0,
			"assists": 0,
			"d_auto": 0.0,
			"d_ab": 0.0,
			"d_ult": 0.0,
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
	h["shield"] = float(h["shield"]) + float(u.shielding_done)
	h["stuns"] = int(h["stuns"]) + int(u.stuns)
	h["kills"] = int(h["kills"]) + int(u.kills)
	h["deaths"] = int(h["deaths"]) + int(u.deaths)
	h["assists"] = int(h["assists"]) + int(u.assists)
	h["d_auto"] = float(h["d_auto"]) + float(u.damage_dealt_auto)
	h["d_ab"] = float(h["d_ab"]) + float(u.damage_dealt_ability)
	h["d_ult"] = float(h["d_ult"]) + float(u.damage_dealt_ultimate)


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
			"shield": 0.0,
			"stuns": 0,
			"kills": 0,
			"deaths": 0,
			"assists": 0,
			"d_auto": 0.0,
			"d_ab": 0.0,
			"d_ult": 0.0,
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
	r["shield"] = float(r["shield"]) + float(u.shielding_done)
	r["stuns"] = int(r["stuns"]) + int(u.stuns)
	r["kills"] = int(r["kills"]) + int(u.kills)
	r["deaths"] = int(r["deaths"]) + int(u.deaths)
	r["assists"] = int(r["assists"]) + int(u.assists)
	r["d_auto"] = float(r["d_auto"]) + float(u.damage_dealt_auto)
	r["d_ab"] = float(r["d_ab"]) + float(u.damage_dealt_ability)
	r["d_ult"] = float(r["d_ult"]) + float(u.damage_dealt_ultimate)


func _acc_combo(bucket: Dictionary, player_comp: Array, wt: StringName) -> void:
	var names: Array[String] = []
	for c in player_comp:
		names.append(String(c))
	names.sort()
	var combo_label := ""
	for i in names.size():
		if i > 0:
			combo_label += " + "
		combo_label += names[i]
	var combos: Dictionary = bucket["combos"]
	if not combos.has(combo_label):
		combos[combo_label] = {"w": 0, "n": 0}
	combos[combo_label]["n"] = int(combos[combo_label]["n"]) + 1
	if wt == &"player":
		combos[combo_label]["w"] = int(combos[combo_label]["w"]) + 1


func _fmt_f(v: float) -> String:
	return String.num(snappedf(v, 0.000001), 6).trim_suffix("0").trim_suffix(".")


func _csv_cell(raw: String) -> String:
	if raw.contains(",") or raw.contains("\"") or raw.contains("\n") or raw.contains("\r"):
		return "\"" + raw.replace("\"", "\"\"") + "\""
	return raw


func _write_text_atomic(res_path: String, content: String) -> Error:
	var abs_path := ProjectSettings.globalize_path(res_path)
	var abs_tmp := abs_path + ".tmp"
	var f := FileAccess.open(abs_tmp, FileAccess.WRITE)
	if f == null:
		return FileAccess.get_open_error()
	f.store_string(content)
	f.flush()
	f.close()
	if FileAccess.file_exists(abs_path):
		var rm := DirAccess.remove_absolute(abs_path)
		if rm != OK:
			return rm
	return DirAccess.rename_absolute(abs_tmp, abs_path)


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
		"team_size,hero,wins,losses,draws,total_games,win_rate,avg_dmg_dealt,avg_dmg_received,avg_dmg_mitigated,avg_healing,avg_shielding,avg_stuns,avg_kills,avg_deaths,avg_assists,kda,avg_dmg_auto,avg_dmg_ability,avg_dmg_ultimate"
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
				"%d,%s,%d,%d,%d,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s"
				% [
					int(sz),
					_csv_cell(String(hero)),
					w,
					l,
					d,
					tg,
					_fmt_f(wr),
					_fmt_f(float(h["dmg_d"]) / float(tg)),
					_fmt_f(float(h["dmg_r"]) / float(tg)),
					_fmt_f(float(h["dmg_m"]) / float(tg)),
					_fmt_f(float(h["heal"]) / float(tg)),
					_fmt_f(float(h["shield"]) / float(tg)),
					_fmt_f(float(h["stuns"]) / float(tg)),
					_fmt_f(float(h["kills"]) / float(tg)),
					_fmt_f(float(h["deaths"]) / float(tg)),
					_fmt_f(float(h["assists"]) / float(tg)),
					_fmt_f(kda),
					_fmt_f(float(h["d_auto"]) / float(tg)),
					_fmt_f(float(h["d_ab"]) / float(tg)),
					_fmt_f(float(h["d_ult"]) / float(tg)),
				]
			)
	return "\n".join(lines) + "\n"


func _build_role_csv() -> String:
	var lines: PackedStringArray = PackedStringArray()
	lines.append(
		"team_size,role,wins,losses,draws,total_games,win_rate,avg_dmg_dealt,avg_dmg_received,avg_dmg_mitigated,avg_healing,avg_shielding,avg_stuns,kda,avg_dmg_auto,avg_dmg_ability,avg_dmg_ultimate"
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
			var kda: float = (float(r["kills"]) + float(r["assists"])) / maxf(1.0, float(r["deaths"]))
			lines.append(
				"%d,%s,%d,%d,%d,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s"
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
					_fmt_f(float(r["shield"]) / float(tg)),
					_fmt_f(float(r["stuns"]) / float(tg)),
					_fmt_f(kda),
					_fmt_f(float(r["d_auto"]) / float(tg)),
					_fmt_f(float(r["d_ab"]) / float(tg)),
					_fmt_f(float(r["d_ult"]) / float(tg)),
				]
			)
	return "\n".join(lines) + "\n"


func _build_combo_csv() -> String:
	var lines: PackedStringArray = PackedStringArray()
	lines.append("team_size,combination,win_rate,wins,total_games")
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
