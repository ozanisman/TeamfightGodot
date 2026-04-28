class_name MatchupDataLoader
extends RefCounted

## Loads and parses matchup data from CSV files
## Provides structured access to vs and with matchup statistics

var vs_data: Dictionary = {}
var with_data: Dictionary = {}
var champions: Array[String] = []
var is_loaded: bool = false
var last_error: String = ""

const VS_FILE_PATH := "res://stats_output/matchup_vs.csv"
const WITH_FILE_PATH := "res://stats_output/matchup_with.csv"

func load_data() -> bool:
	vs_data.clear()
	with_data.clear()
	champions.clear()
	is_loaded = false
	last_error = ""
	
	print("Loading matchup data from:")
	print("VS file: ", VS_FILE_PATH)
	print("WITH file: ", WITH_FILE_PATH)
	
	# Load vs matchups
	print("Loading VS matchups...")
	if not _load_csv_file(VS_FILE_PATH, vs_data, "vs"):
		return false
	
	print("VS matchups loaded: ", vs_data.size(), " champions")
	
	# Load with matchups  
	print("Loading WITH matchups...")
	if not _load_csv_file(WITH_FILE_PATH, with_data, "with"):
		return false
	
	print("WITH matchups loaded: ", with_data.size(), " champions")
	
	# Extract champion list
	print("Extracting champion list...")
	_extract_champion_list()
	print("Final champion count: ", champions.size())
	is_loaded = true
	return true

func _load_csv_file(file_path: String, target_dict: Dictionary, matchup_type: String) -> bool:
	print("Opening file: ", file_path)
	var file := FileAccess.open(file_path, FileAccess.READ)
	if file == null:
		last_error = "Failed to open %s file: %s" % [matchup_type, file_path]
		print("ERROR: ", last_error)
		return false
	
	print("File opened successfully")
	
	# Skip header line
	if file.get_position() < file.get_length():
		var header = file.get_line()
		print("Header: ", header)
	
	var line_count = 0
	while file.get_position() < file.get_length():
		var line := file.get_line().strip_edges()
		if line.is_empty():
			continue
		
		line_count += 1
		var parts := line.split(",", false)
		if parts.size() != 5:
			print("Skipping invalid line (", line_count, "): ", line)
			continue
		
		var champion := parts[0]
		var opponent := parts[1]
		var wins := int(parts[2])
		var losses := int(parts[3])
		var winrate := float(parts[4])
		
		if not target_dict.has(champion):
			target_dict[champion] = {}
		
		target_dict[champion][opponent] = {
			"wins": wins,
			"losses": losses,
			"winrate": winrate
		}
	
	print("Processed ", line_count, " lines for ", matchup_type)
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
		if data.winrate > 0.5:  # Only include favorable matchups
			counters.append({
				"name": opponent,
				"winrate": data.winrate,
				"wins": data.wins,
				"losses": data.losses
			})
	
	# Sort by winrate descending
	counters.sort_custom(func(a, b): return a.winrate > b.winrate)
	
	# Return top results
	if counters.size() > limit:
		counters = counters.slice(0, limit)
	
	return counters

func get_weak_against(champion_name: String, limit: int = 5) -> Array:
	var matchups := get_champion_matchups(champion_name)
	var weak := []
	
	for opponent in matchups["vs"]:
		var data = matchups["vs"][opponent]
		if data.winrate < 0.5:  # Only include unfavorable matchups
			weak.append({
				"name": opponent,
				"winrate": data.winrate,
				"wins": data.wins,
				"losses": data.losses
			})
	
	# Sort by winrate ascending (worst first)
	weak.sort_custom(func(a, b): return a.winrate < b.winrate)
	
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

func get_poor_synergies(champion_name: String, limit: int = 5) -> Array:
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
