class_name MatchupTracker
extends RefCounted

# Tracks individual champion vs champion and champion with champion winrates
# Data structure: champion_id -> { vs_opponent_id: {wins, losses, winrate}, with_ally_id: {wins, losses, winrate} }

var matchup_data: Dictionary = {}
var champion_ids: Array[StringName] = []

func _init():
	_initialize_champion_ids()

func _initialize_champion_ids() -> void:
	# Get all champion IDs dynamically using static methods
	champion_ids = ChampionCatalog.get_champion_ids()
	
	# Initialize data structure for all champions
	for champion_id in champion_ids:
		matchup_data[String(champion_id)] = {}
		
		# Initialize vs and with data for all other champions
		for other_id in champion_ids:
			if champion_id != other_id:
				matchup_data[String(champion_id)]["vs_" + String(other_id)] = {
					"wins": 0,
					"losses": 0,
					"winrate": 0.0
				}
				matchup_data[String(champion_id)]["with_" + String(other_id)] = {
					"wins": 0,
					"losses": 0,
					"winrate": 0.0
				}

func process_match_result(winners: Array[StringName], losers: Array[StringName]) -> void:
	# Process all matchups from this match result
	
	# Winners get wins vs all losers and wins with all other winners
	for winner in winners:
		for loser in losers:
			_record_vs_result(winner, loser, true)  # winner vs loser = win
		
		for ally in winners:
			if winner != ally:
				_record_with_result(winner, ally, true)  # winner with ally = win
	
	# Losers get losses vs all winners and losses with all other losers
	for loser in losers:
		for winner in winners:
			_record_vs_result(loser, winner, false)  # loser vs winner = loss
		
		for ally in losers:
			if loser != ally:
				_record_with_result(loser, ally, false)  # loser with ally = loss

func _record_vs_result(champion: StringName, opponent: StringName, won: bool) -> void:
	var key = "vs_" + String(opponent)
	if not matchup_data.has(String(champion)) or not matchup_data[String(champion)].has(key):
		return
	
	var data = matchup_data[String(champion)][key]
	if won:
		data.wins += 1
	else:
		data.losses += 1
	
	_update_winrate(data)

func _record_with_result(champion: StringName, ally: StringName, won: bool) -> void:
	var key = "with_" + String(ally)
	if not matchup_data.has(String(champion)) or not matchup_data[String(champion)].has(key):
		return
	
	var data = matchup_data[String(champion)][key]
	if won:
		data.wins += 1
	else:
		data.losses += 1
	
	_update_winrate(data)

func _update_winrate(data: Dictionary) -> void:
	var total = data.wins + data.losses
	if total > 0:
		data.winrate = float(data.wins) / float(total)
	else:
		data.winrate = 0.0

func get_matchup_data() -> Dictionary:
	return matchup_data.duplicate(true)

func get_champion_matchups(champion_id: StringName) -> Dictionary:
	return matchup_data.get(String(champion_id), {})

func get_vs_winrate(champion: StringName, opponent: StringName) -> float:
	var key = "vs_" + String(opponent)
	if matchup_data.has(String(champion)) and matchup_data[String(champion)].has(key):
		return matchup_data[String(champion)][key].winrate
	return 0.0

func get_with_winrate(champion: StringName, ally: StringName) -> float:
	var key = "with_" + String(ally)
	if matchup_data.has(String(champion)) and matchup_data[String(champion)].has(key):
		return matchup_data[String(champion)][key].winrate
	return 0.0

func reset() -> void:
	matchup_data.clear()
	_initialize_champion_ids()

func merge_with(other_tracker: MatchupTracker) -> void:
	var other_data = other_tracker.get_matchup_data()
	
	for champion_id in other_data:
		if not matchup_data.has(champion_id):
			continue
		
		for matchup_key in other_data[champion_id]:
			if not matchup_data[champion_id].has(matchup_key):
				continue
			
			# Merge wins and losses
			matchup_data[champion_id][matchup_key].wins += other_data[champion_id][matchup_key].wins
			matchup_data[champion_id][matchup_key].losses += other_data[champion_id][matchup_key].losses
			_update_winrate(matchup_data[champion_id][matchup_key])
