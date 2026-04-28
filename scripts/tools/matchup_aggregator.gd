class_name MatchupAggregator
extends RefCounted

# Aggregates matchup data from multiple simulation chunks
# Merges individual matchup trackers into comprehensive statistics

const MatchupTracker := preload("res://scripts/simulation/matchup_tracker.gd")

var global_matchup_tracker: MatchupTracker
var total_matches: int = 0

func _init():
	global_matchup_tracker = MatchupTracker.new()

func reset() -> void:
	global_matchup_tracker.reset()
	total_matches = 0

func consume_chunk_result(chunk_result: Variant) -> void:
	# Handle both old format (array of results) and new format (dictionary with matchup_data)
	if chunk_result is Array:
		# Old format - just count matches
		total_matches += chunk_result.size()
	elif chunk_result is Dictionary:
		# New format - extract match results and matchup data
		if chunk_result.has("match_results"):
			var match_results = chunk_result["match_results"]
			if match_results is Array:
				total_matches += match_results.size()
		
		if chunk_result.has("matchup_data"):
			var matchup_data = chunk_result["matchup_data"]
			if matchup_data is Dictionary:
				_merge_matchup_data(matchup_data)

func _merge_matchup_data(chunk_matchup_data: Dictionary) -> void:
	# Merge chunk matchup data into global tracker
	for champion_id in chunk_matchup_data:
		if not global_matchup_tracker.matchup_data.has(champion_id):
			continue
		
		for matchup_key in chunk_matchup_data[champion_id]:
			if not global_matchup_tracker.matchup_data[champion_id].has(matchup_key):
				continue
			
			var chunk_data = chunk_matchup_data[champion_id][matchup_key]
			var global_data = global_matchup_tracker.matchup_data[champion_id][matchup_key]
			
			# Merge wins and losses
			global_data.wins += chunk_data.wins
			global_data.losses += chunk_data.losses
			
			# Update winrate
			var total = global_data.wins + global_data.losses
			if total > 0:
				global_data.winrate = float(global_data.wins) / float(total)
			else:
				global_data.winrate = 0.0

func get_matchup_data() -> Dictionary:
	return global_matchup_tracker.get_matchup_data()

func get_total_matches() -> int:
	return total_matches

func export_matchup_json() -> Dictionary:
	var result = {
		"metadata": {
			"total_matches": total_matches,
			"champion_count": global_matchup_tracker.champion_ids.size(),
			"generated_at": Time.get_datetime_string_from_system()
		},
		"matchups": _serialize_matchup_data_ordered()
	}
	return result

func _serialize_matchup_data_ordered() -> Dictionary:
	var ordered_data: Dictionary = {}
	var matchup_data = global_matchup_tracker.get_matchup_data()
	
	for champion_id in matchup_data.keys():
		var champion_data: Dictionary = {}
		var matchups = matchup_data[champion_id]
		
		# Sort keys to ensure consistent ordering
		var sorted_keys = matchups.keys()
		sorted_keys.sort()
		
		for key in sorted_keys:
			var data = matchups[key]
			# Create ordered dictionary with wins -> losses -> winrate
			champion_data[key] = {
				"wins": data.wins,
				"losses": data.losses,
				"winrate": data.winrate
			}
		
		ordered_data[champion_id] = champion_data
	
	return ordered_data

func write_matchup_csv_files(output_dir: String) -> bool:
	var matchup_data = global_matchup_tracker.get_matchup_data()
	
	# Write matchup_vs.csv
	var vs_lines: PackedStringArray = []
	vs_lines.append("champion,opponent,wins,losses,winrate")
	
	# Write matchup_with.csv
	var with_lines: PackedStringArray = []
	with_lines.append("champion,ally,wins,losses,winrate")
	
	for champion_id in matchup_data.keys():
		var matchups = matchup_data[champion_id]
		var sorted_keys = matchups.keys()
		sorted_keys.sort()
		
		for key in sorted_keys:
			var data = matchups[key]
			if key.begins_with("vs_"):
				var opponent = key.substr(3)
				vs_lines.append("%s,%s,%d,%d,%s" % [champion_id, opponent, data.wins, data.losses, _fmt_f(data.winrate)])
			elif key.begins_with("with_"):
				var ally = key.substr(5)
				with_lines.append("%s,%s,%d,%d,%s" % [champion_id, ally, data.wins, data.losses, _fmt_f(data.winrate)])
	
	# Write vs file
	var vs_path = output_dir + "/matchup_vs.csv"
	var vs_file = FileAccess.open(vs_path, FileAccess.WRITE)
	if vs_file == null:
		push_error("Failed to open matchup_vs.csv for writing: %s" % vs_path)
		return false
	vs_file.store_string("\n".join(vs_lines))
	vs_file.close()
	print("Matchup vs data exported successfully to: %s" % vs_path)
	
	# Write with file
	var with_path = output_dir + "/matchup_with.csv"
	var with_file = FileAccess.open(with_path, FileAccess.WRITE)
	if with_file == null:
		push_error("Failed to open matchup_with.csv for writing: %s" % with_path)
		return false
	with_file.store_string("\n".join(with_lines))
	with_file.close()
	print("Matchup with data exported successfully to: %s" % with_path)
	
	return true

func _fmt_f(value: float) -> String:
	return str(snapped(value, 0.000001))
