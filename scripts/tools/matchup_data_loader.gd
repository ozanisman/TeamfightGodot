class_name MatchupDataLoader
extends RefCounted

## Loads and parses matchup data from CSV files
## Provides structured access to vs and with matchup statistics

var vs_data: Dictionary = {}
var with_data: Dictionary = {}
var champions: Array[String] = []
var is_loaded: bool = false
var last_error: String = ""

const VS_FILE_PATH := "res://model_stats/certified_pairwise_testing_250k/matchup_vs.csv"
const WITH_FILE_PATH := "res://model_stats/certified_pairwise_testing_250k/matchup_with.csv"

func load_data() -> bool:
	vs_data.clear()
	with_data.clear()
	champions.clear()
	is_loaded = false
	last_error = ""
	
	# Load vs matchups
	if not _load_csv_file(VS_FILE_PATH, vs_data, "vs"):
		return false
	
	# Load with matchups  
	if not _load_csv_file(WITH_FILE_PATH, with_data, "with"):
		return false
	
	# Extract champion list
	_extract_champion_list()
	is_loaded = true
	return true

func _load_csv_file(file_path: String, target_dict: Dictionary, matchup_type: String) -> bool:
	var file := FileAccess.open(file_path, FileAccess.READ)
	if file == null:
		last_error = ""
		return false
	
	# Skip header line
	if file.get_position() < file.get_length():
		file.get_line()
	
	while file.get_position() < file.get_length():
		var line := file.get_line().strip_edges()
		if line.is_empty():
			continue
		
		var parts := line.split(",", false)
		if parts.size() != 5:
			continue
		
		var champion_unit_id := parts[0]
		var opponent_unit_id := parts[1]
		var wins := int(parts[2])
		var losses := int(parts[3])
		var winrate := float(parts[4])
		
		if not target_dict.has(champion_unit_id):
			target_dict[champion_unit_id] = {}
		
		target_dict[champion_unit_id][opponent_unit_id] = {
			"wins": wins,
			"losses": losses,
			"winrate": winrate
		}
	
	file.close()
	return true

func _extract_champion_list():
	var all_champs: Dictionary = {}
	
	# Extract from vs data
	for champion in vs_data.keys():
		all_champs[champion] = true
		for opponent in vs_data[champion].keys():
			all_champs[opponent] = true
	
	# Extract from with data
	for champion in with_data.keys():
		all_champs[champion] = true
		for ally in with_data[champion].keys():
			all_champs[ally] = true
	
	champions = []
	for champ in all_champs.keys():
		champions.append(champ)
	champions.sort()

func get_champion_matchups(champion_name: String) -> Dictionary:
	var result := {
		"vs": {},
		"with": {}
	}
	
	if vs_data.has(champion_name):
		result["vs"] = vs_data[champion_name].duplicate(true)
	
	if with_data.has(champion_name):
		result["with"] = with_data[champion_name].duplicate(true)
	
	return result

func get_best_counters(champion_name: String, limit: int = 5) -> Array:
	var matchups := get_champion_matchups(champion_name)
	var counters := []
	
	for opponent in matchups["vs"]:
		var data = matchups["vs"][opponent]
		if data.winrate < 0.5:  # Only include unfavorable matchups (good counters)
			counters.append({
				"name": opponent,
				"winrate": data.winrate,
				"wins": data.wins,
				"losses": data.losses
			})
	
	# Sort by winrate ascending (worst for current champion = best counter)
	counters.sort_custom(func(a, b): return a.winrate < b.winrate)
	
	# Return top results
	if counters.size() > limit:
		counters = counters.slice(0, limit)
	
	return counters

func get_weak_against(champion_name: String, limit: int = 5) -> Array:
	var matchups := get_champion_matchups(champion_name)
	var weak := []
	
	for opponent in matchups["vs"]:
		var data = matchups["vs"][opponent]
		if data.winrate > 0.5:  # Only include favorable matchups (weak against)
			weak.append({
				"name": opponent,
				"winrate": data.winrate,
				"wins": data.wins,
				"losses": data.losses
			})
	
	# Sort by winrate descending (best for current champion = weak against)
	weak.sort_custom(func(a, b): return a.winrate > b.winrate)
	
	# Return top results
	if weak.size() > limit:
		weak = weak.slice(0, limit)
	
	return weak

func get_best_synergies(champion_name: String, limit: int = 5) -> Array:
	var matchups := get_champion_matchups(champion_name)
	var synergies := []
	
	for ally in matchups["with"]:
		var data = matchups["with"][ally]
		if data.winrate > 0.5:  # Only include favorable synergies
			synergies.append({
				"name": ally,
				"winrate": data.winrate,
				"wins": data.wins,
				"losses": data.losses
			})
	
	# Sort by winrate descending
	synergies.sort_custom(func(a, b): return a.winrate > b.winrate)
	
	# Return top results
	if synergies.size() > limit:
		synergies = synergies.slice(0, limit)
	
	return synergies

func get_worst_synergies(champion_name: String, limit: int = 5) -> Array:
	var matchups := get_champion_matchups(champion_name)
	var poor := []
	
	for ally in matchups["with"]:
		var data = matchups["with"][ally]
		if data.winrate < 0.5:  # Only include unfavorable synergies
			poor.append({
				"name": ally,
				"winrate": data.winrate,
				"wins": data.wins,
				"losses": data.losses
			})
	
	# Sort by winrate ascending (worst first)
	poor.sort_custom(func(a, b): return a.winrate < b.winrate)
	
	# Return top results
	if poor.size() > limit:
		poor = poor.slice(0, limit)
	
	return poor

func search_champions(query: String) -> Array[String]:
	var results: Array[String] = []
	var lower_query = query.to_lower()
	
	for champion in champions:
		if champion.to_lower().contains(lower_query):
			results.append(champion)
	
	return results


func get_aggregate_extremes() -> Dictionary:
	var all_vs := []
	var all_with := []
	
	# Aggregate all counter matchups
	for champion in vs_data.keys():
		for opponent in vs_data[champion].keys():
			var data = vs_data[champion][opponent]
			all_vs.append({
				"champion": champion,
				"opponent": opponent,
				"wins": data.wins,
				"losses": data.losses,
				"winrate": data.winrate
			})
	
	# Aggregate all synergy matchups (deduplicate A,B and B,A pairs)
	var synergy_pairs := {}
	for champion in with_data.keys():
		for ally in with_data[champion].keys():
			var data = with_data[champion][ally]
			# Normalize pair order alphabetically to deduplicate
			var pair_key := [champion, ally]
			pair_key.sort()
			var normalized_key = pair_key[0] + "|" + pair_key[1]
			
			# Only keep the first occurrence (data should be identical)
			if not synergy_pairs.has(normalized_key):
				all_with.append({
					"champion": pair_key[0],  # Alphabetically first
					"ally": pair_key[1],     # Alphabetically second
					"wins": data.wins,
					"losses": data.losses,
					"winrate": data.winrate
				})
				synergy_pairs[normalized_key] = true
	
	# Sort counters by winrate (highest = best counter, lowest = worst counter)
	all_vs.sort_custom(func(a, b): return a.winrate > b.winrate)
	
	# Sort synergies by winrate (highest = best synergy, lowest = worst synergy)
	all_with.sort_custom(func(a, b): return a.winrate > b.winrate)
	
	# Get top 100 best for each
	var best_counters = all_vs.slice(0, 100)
	var best_synergies = all_with.slice(0, 100)
	
	return {
		"best_counters": best_counters,
		"best_synergies": best_synergies
	}
