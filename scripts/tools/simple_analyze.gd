extends SceneTree

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	print("simple_analyze: STARTED")
	
	var input_path := "res://stats_output_partial/draft_ab_test_200.csv"
	var global_path := ProjectSettings.globalize_path(input_path)
	print("simple_analyze: reading from %s" % global_path)
	
	var f := FileAccess.open(global_path, FileAccess.READ)
	if f == null:
		push_error("simple_analyze: could not open file")
		quit(1)
		return
	
	var lines := f.get_as_text().split("\n")
	f.close()
	print("simple_analyze: loaded %d lines" % lines.size())
	
	var header := lines[0].split(",")
	print("simple_analyze: header: %s" % header)
	
	var pick_counts := {"hybrid": {}, "logit": {}, "random": {}}
	
	for i in range(1, lines.size()):
		var line := lines[i].strip_edges()
		if line.is_empty():
			continue
		var parts := line.split(",")
		if parts.size() < 10:
			continue
		
		var strategy_a := parts[5]
		var pick_a := parts[6]
		var strategy_b := parts[8]
		var pick_b := parts[9]
		
		if strategy_a in pick_counts and not pick_a.is_empty():
			if not pick_a in pick_counts[strategy_a]:
				pick_counts[strategy_a][pick_a] = 0
			pick_counts[strategy_a][pick_a] += 1
		
		if strategy_b in pick_counts and not pick_b.is_empty():
			if not pick_b in pick_counts[strategy_b]:
				pick_counts[strategy_b][pick_b] = 0
			pick_counts[strategy_b][pick_b] += 1
	
	print("simple_analyze: pick counts:")
	for strat in pick_counts:
		print("  %s: %d unique picks" % [strat, pick_counts[strat].size()])
	
	var output_path := "res://stats_output_partial/pick_frequency_simple.csv"
	var global_output := ProjectSettings.globalize_path(output_path)
	print("simple_analyze: writing to %s" % global_output)
	
	var out_f := FileAccess.open(global_output, FileAccess.WRITE)
	if out_f == null:
		push_error("simple_analyze: could not write output")
		quit(1)
		return
	
	out_f.store_string("champion,hybrid,logit,random\n")
	
	var all_champs := {}
	for strat in pick_counts:
		for champ in pick_counts[strat]:
			all_champs[champ] = true
	
	for champ in all_champs:
		var line = "%s,%d,%d,%d\n" % [champ, pick_counts["hybrid"].get(champ, 0), pick_counts["logit"].get(champ, 0), pick_counts["random"].get(champ, 0)]
		out_f.store_string(line)
	
	out_f.close()
	print("simple_analyze: wrote %d champions" % all_champs.size())
	
	# Contextual analysis: enemy co-occurrence
	print("simple_analyze: contextual analysis...")
	var enemy_cooccurrence := {"hybrid": {}, "logit": {}, "random": {}}
	
	for i in range(1, lines.size()):
		var line := lines[i].strip_edges()
		if line.is_empty():
			continue
		var parts := line.split(",")
		if parts.size() < 10:
			continue
		
		var strategy_a := parts[5]
		var pick_a := parts[6]
		var enemies := parts[3].split("|")
		var strategy_b := parts[8]
		var pick_b := parts[9]
		
		_process_context(strategy_a, pick_a, enemies, enemy_cooccurrence)
		_process_context(strategy_b, pick_b, enemies, enemy_cooccurrence)
	
	var context_path := "res://stats_output_partial/pick_contextual_simple.csv"
	var global_context := ProjectSettings.globalize_path(context_path)
	var context_f := FileAccess.open(global_context, FileAccess.WRITE)
	if context_f == null:
		push_error("simple_analyze: could not write contextual")
		quit(1)
		return
	
	context_f.store_string("picked_champion,strategy,enemy_champion,count\n")
	
	for strat in enemy_cooccurrence:
		for picked in enemy_cooccurrence[strat]:
			for enemy in enemy_cooccurrence[strat][picked]:
				var count = enemy_cooccurrence[strat][picked][enemy]
				context_f.store_string("%s,%s,%s,%d\n" % [picked, strat, enemy, count])
	
	context_f.close()
	print("simple_analyze: wrote contextual analysis")
	
	# Comparative analysis: strategy disagreements
	print("simple_analyze: comparative analysis...")
	var disagreements := {}
	
	for i in range(1, lines.size()):
		var line := lines[i].strip_edges()
		if line.is_empty():
			continue
		var parts := line.split(",")
		if parts.size() < 10:
			continue
		
		var depth := parts[1]
		var allies := parts[2]
		var enemies := parts[3]
		var trial_id := parts[0]
		var key = "%s_%s_%s_%s" % [depth, allies, enemies, trial_id]
		
		if key not in disagreements:
			disagreements[key] = {"depth": depth, "allies": allies, "enemies": enemies, "picks": {}, "winrates": {}}
		
		var strategy_a := parts[5]
		var pick_a := parts[6]
		var winrate_a := float(parts[7])
		var strategy_b := parts[8]
		var pick_b := parts[9]
		var winrate_b := float(parts[10])
		
		disagreements[key]["picks"][strategy_a] = pick_a
		disagreements[key]["picks"][strategy_b] = pick_b
		disagreements[key]["winrates"][strategy_a] = winrate_a
		disagreements[key]["winrates"][strategy_b] = winrate_b
	
	var comp_path := "res://stats_output_partial/pick_comparative_simple.csv"
	var global_comp := ProjectSettings.globalize_path(comp_path)
	var comp_f := FileAccess.open(global_comp, FileAccess.WRITE)
	if comp_f == null:
		push_error("simple_analyze: could not write comparative")
		quit(1)
		return
	
	var strategies = ["hybrid", "logit", "random"]
	var headers = ["allies", "enemies", "depth"]
	for s in strategies:
		headers.append("%s_pick" % s)
	for s in strategies:
		headers.append("%s_winrate" % s)
	comp_f.store_string(",".join(headers) + "\n")
	
	for key in disagreements:
		var data = disagreements[key]
		var line_parts = [data["allies"], data["enemies"], data["depth"]]
		for s in strategies:
			line_parts.append(data["picks"].get(s, ""))
		for s in strategies:
			line_parts.append(str(data["winrates"].get(s, 0.0)))
		comp_f.store_string(",".join(line_parts) + "\n")
	
	comp_f.close()
	print("simple_analyze: wrote comparative analysis (%d entries)" % disagreements.size())
	print("simple_analyze: DONE")
	quit(0)


func _process_context(strategy: String, pick: String, enemies: Array, enemy_cooccurrence: Dictionary) -> void:
	if strategy not in enemy_cooccurrence or pick.is_empty():
		return
	if not pick in enemy_cooccurrence[strategy]:
		enemy_cooccurrence[strategy][pick] = {}
	for enemy in enemies:
		if enemy.is_empty():
			continue
		if not enemy in enemy_cooccurrence[strategy][pick]:
			enemy_cooccurrence[strategy][pick][enemy] = 0
		enemy_cooccurrence[strategy][pick][enemy] += 1
