class_name StatsDashboardLoader
extends RefCounted

## Loads stats CSV bundle matching Python stats_gui.py contract.

const HEADER_PREFIX := "team_size,"

var all_stats: Dictionary = {}
var summary_stats: Dictionary = {}
var ci_stats: Dictionary = {}


static func parse_csv_line(line: String) -> PackedStringArray:
	var out: PackedStringArray = PackedStringArray()
	var cur := ""
	var in_quotes := false
	var i := 0
	while i < line.length():
		var ch: String = line[i]
		if in_quotes:
			if ch == "\"":
				if i + 1 < line.length() and line[i + 1] == "\"":
					cur += "\""
					i += 1
				else:
					in_quotes = false
			else:
				cur += ch
		else:
			if ch == "\"":
				in_quotes = true
			elif ch == ",":
				out.append(cur.strip_edges())
				cur = ""
			else:
				cur += ch
		i += 1
	out.append(cur.strip_edges())
	return out


static func _rows_from_csv_file(path: String, allow_empty_data: bool = false) -> Array[Dictionary]:
	var f := FileAccess.open(path, FileAccess.READ)
	if f == null:
		push_error("StatsDashboardLoader: cannot open %s" % path)
		return []
	var header_line: String = ""
	while not f.eof_reached():
		var raw: String = f.get_line().strip_edges(false, true)
		var stripped: String = raw.strip_edges()
		if stripped.begins_with(HEADER_PREFIX):
			header_line = raw
			break
	if header_line.is_empty():
		push_error("StatsDashboardLoader: missing CSV header in %s" % path)
		return []
	var header_cells := parse_csv_line(header_line)
	var rows: Array[Dictionary] = []
	while not f.eof_reached():
		var line: String = f.get_line()
		if line.strip_edges().is_empty():
			continue
		var cells := parse_csv_line(line)
		if cells.size() != header_cells.size():
			push_error(
				"StatsDashboardLoader: column count mismatch in %s (expected %d got %d)"
				% [path, header_cells.size(), cells.size()]
			)
			return []
		var row: Dictionary = {}
		for j in header_cells.size():
			row[header_cells[j]] = cells[j]
		rows.append(row)
	if rows.is_empty() and not allow_empty_data:
		push_error("StatsDashboardLoader: no data rows in %s" % path)
		return []
	return rows


static func _int_cell(row: Dictionary, key: String, default_val: int = 0) -> int:
	if not row.has(key) or str(row[key]).is_empty():
		return default_val
	return int(str(row[key]))


static func _float_cell(row: Dictionary, key: String, default_val: float = 0.0) -> float:
	if not row.has(key) or str(row[key]).is_empty():
		return default_val
	return str(row[key]).to_float()


func load_from_dir(dir_path: String) -> Error:
	all_stats.clear()
	summary_stats.clear()
	ci_stats.clear()

	var hero_path: String = "%s/combat_stats.csv" % dir_path
	var combo_path: String = "%s/role_combinations.csv" % dir_path
	var role_path: String = "%s/role_stats.csv" % dir_path
	var summary_path: String = "%s/summary_stats.csv" % dir_path
	var ci_path: String = "%s/combat_stats_ci.csv" % dir_path

	for p in [hero_path, combo_path, role_path, summary_path]:
		if not FileAccess.file_exists(p):
			return ERR_FILE_NOT_FOUND

	# Summary
	var summary_rows := _rows_from_csv_file(summary_path)
	if summary_rows.is_empty():
		return ERR_INVALID_DATA
	for row in summary_rows:
		var size: int = _int_cell(row, "team_size")
		summary_stats[size] = {
			"p1": _int_cell(row, "p1_wins"),
			"p2": _int_cell(row, "p2_wins"),
			"draws": _int_cell(row, "draws", 0),
			"total": _int_cell(row, "total_rounds"),
		}

	# Heroes
	var hero_rows := _rows_from_csv_file(hero_path)
	if hero_rows.is_empty():
		return ERR_INVALID_DATA
	for row in hero_rows:
		var size: int = _int_cell(row, "team_size")
		if not all_stats.has(size):
			all_stats[size] = {"units": {}, "combinations": {}, "roles": {}}
		var hero: String = str(row["hero"])
		var wins: int = _int_cell(row, "wins")
		var losses: int = _int_cell(row, "losses")
		var draws: int = _int_cell(row, "draws", 0)
		var total: int = _int_cell(row, "total_games", wins + losses + draws)
		all_stats[size]["units"][hero] = {
			"winRate": _float_cell(row, "win_rate"),
			"wins": wins,
			"losses": losses,
			"draws": draws,
			"total": total,
			"damage_dealt": _float_cell(row, "avg_dmg_dealt") * float(total),
			"damage_received": _float_cell(row, "avg_dmg_received") * float(total),
			"damage_mitigated": _float_cell(row, "avg_dmg_mitigated") * float(total),
			"healing_done": _float_cell(row, "avg_healing") * float(total),
			"shielding_done": _float_cell(row, "avg_shielding") * float(total),
			"stuns": _float_cell(row, "avg_stuns") * float(total),
			"kills": _float_cell(row, "avg_kills") * float(total),
			"deaths": _float_cell(row, "avg_deaths") * float(total),
			"assists": _float_cell(row, "avg_assists") * float(total),
			"kda": _float_cell(row, "kda"),
			"minion_damage_dealt": _float_cell(row, "avg_minion_dmg_dealt", 0.0) * float(total),
			"minion_damage_received": _float_cell(row, "avg_minion_dmg_received", 0.0) * float(total),
			"minion_damage_mitigated": _float_cell(row, "avg_minion_dmg_mitigated", 0.0) * float(total),
			"breakdown": {
				"auto": _float_cell(row, "avg_dmg_auto") * float(total),
				"ability": _float_cell(row, "avg_dmg_ability") * float(total),
				"ultimate": _float_cell(row, "avg_dmg_ultimate") * float(total),
				"passive": _float_cell(row, "avg_dmg_passive") * float(total),
				"heal_auto": _float_cell(row, "avg_healing_auto") * float(total),
				"heal_ability": _float_cell(row, "avg_healing_ability") * float(total),
				"heal_ultimate": _float_cell(row, "avg_healing_ultimate") * float(total),
				"heal_passive": _float_cell(row, "avg_healing_passive") * float(total),
				"shield_auto": _float_cell(row, "avg_shielding_auto") * float(total),
				"shield_ability": _float_cell(row, "avg_shielding_ability") * float(total),
				"shield_ultimate": _float_cell(row, "avg_shielding_ultimate") * float(total),
				"shield_passive": _float_cell(row, "avg_shielding_passive") * float(total),
			},
		}

	# Roles
	var role_rows := _rows_from_csv_file(role_path)
	if role_rows.is_empty():
		return ERR_INVALID_DATA
	for row in role_rows:
		var size: int = _int_cell(row, "team_size")
		if not all_stats.has(size):
			all_stats[size] = {"units": {}, "combinations": {}, "roles": {}}
		var role: String = str(row["role"])
		var wins_r: int = _int_cell(row, "wins")
		var losses_r: int = _int_cell(row, "losses")
		var draws_r: int = _int_cell(row, "draws", 0)
		var total_r: int = maxi(1, _int_cell(row, "total_games", wins_r + losses_r + draws_r))
		all_stats[size]["roles"][role] = {
			"winRate": _float_cell(row, "win_rate"),
			"wins": wins_r,
			"losses": losses_r,
			"draws": draws_r,
			"total": total_r,
			"damage_dealt": _float_cell(row, "avg_dmg_dealt") * float(total_r),
			"damage_received": _float_cell(row, "avg_dmg_received") * float(total_r),
			"damage_mitigated": _float_cell(row, "avg_dmg_mitigated") * float(total_r),
			"healing_done": _float_cell(row, "avg_healing") * float(total_r),
			"shielding_done": _float_cell(row, "avg_shielding") * float(total_r),
			"stuns": _float_cell(row, "avg_stuns") * float(total_r),
			"breakdown": {
				"auto": _float_cell(row, "avg_dmg_auto") * float(total_r),
				"ability": _float_cell(row, "avg_dmg_ability") * float(total_r),
				"ultimate": _float_cell(row, "avg_dmg_ultimate") * float(total_r),
				"passive": _float_cell(row, "avg_dmg_passive") * float(total_r),
				"heal_auto": _float_cell(row, "avg_healing_auto") * float(total_r),
				"heal_ability": _float_cell(row, "avg_healing_ability") * float(total_r),
				"heal_ultimate": _float_cell(row, "avg_healing_ultimate") * float(total_r),
				"heal_passive": _float_cell(row, "avg_healing_passive") * float(total_r),
				"shield_auto": _float_cell(row, "avg_shielding_auto") * float(total_r),
				"shield_ability": _float_cell(row, "avg_shielding_ability") * float(total_r),
				"shield_ultimate": _float_cell(row, "avg_shielding_ultimate") * float(total_r),
				"shield_passive": _float_cell(row, "avg_shielding_passive") * float(total_r),
			},
		}

	# Combinations (optional empty body when only 1v1 data was generated)
	var combo_rows := _rows_from_csv_file(combo_path, true)
	for row in combo_rows:
		var size: int = _int_cell(row, "team_size")
		if not all_stats.has(size):
			continue
		var role_fingerprint: String = str(row["role_fingerprint"])
		all_stats[size]["combinations"][role_fingerprint] = {
			"winRate": _float_cell(row, "win_rate"),
			"wins": _int_cell(row, "wins"),
			"total": _int_cell(row, "total_games"),
		}

	if FileAccess.file_exists(ci_path):
		var ci_rows := _rows_from_csv_file(ci_path)
		if ci_rows.is_empty():
			return ERR_INVALID_DATA
		for row in ci_rows:
			var size: int = _int_cell(row, "team_size")
			var hero_ci: String = str(row["hero"])
			if not ci_stats.has(size):
				ci_stats[size] = {}
			ci_stats[size][hero_ci] = {
				"samples": _int_cell(row, "samples"),
				"winRate": _float_cell(row, "win_rate"),
				"damage_dealt": _float_cell(row, "avg_damage_dealt"),
				"damage_received": _float_cell(row, "avg_damage_received"),
				"healing_done": _float_cell(row, "avg_healing_done"),
				"damage_mitigated": _float_cell(row, "avg_damage_mitigated"),
				"ci": {
					"winRate": [_float_cell(row, "win_rate_ci_low"), _float_cell(row, "win_rate_ci_high")],
					"damage_dealt": [
						_float_cell(row, "avg_damage_dealt_ci_low"),
						_float_cell(row, "avg_damage_dealt_ci_high"),
					],
					"damage_received": [
						_float_cell(row, "avg_damage_received_ci_low"),
						_float_cell(row, "avg_damage_received_ci_high"),
					],
					"healing_done": [
						_float_cell(row, "avg_healing_done_ci_low"),
						_float_cell(row, "avg_healing_done_ci_high"),
					],
					"damage_mitigated": [
						_float_cell(row, "avg_damage_mitigated_ci_low"),
						_float_cell(row, "avg_damage_mitigated_ci_high"),
					],
				},
			}

	print("StatsDashboardLoader: loaded team sizes %s" % str(all_stats.keys()))
	return OK


static func get_count(u_data: Dictionary) -> int:
	var t: Variant = u_data.get("total", u_data.get("wins", 0) + u_data.get("losses", 0))
	return maxi(1, int(t))


static func get_display_val(u_data: Dictionary, metric_key: StringName) -> float:
	if metric_key == &"winRate" or metric_key == &"role_composition":
		return float(u_data.get("winRate", 0.0))
	if metric_key == &"kda":
		return float(u_data.get("kda", 0.0))
	var count: int = get_count(u_data)
	var val: float = float(u_data.get(String(metric_key), 0.0))
	return val / float(count)


static func get_display_val_ci(u_data: Dictionary, metric_key: StringName) -> float:
	if metric_key == &"winRate":
		return float(u_data.get("winRate", 0.0))
	return float(u_data.get(String(metric_key), 0.0))
