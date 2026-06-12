extends SceneTree

## Analyzes A/B test draft pick data to understand strategy behavior.
## Generates frequency, contextual, and comparative analysis outputs.

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	print("analyze_draft_picks: STARTED")
	var input_path := _extract_argument("--input=", "res://model_stats/draft_ab_test_200.csv")
	var output_dir := _extract_argument("--output-dir=", "res://model_stats")
	var mode := _extract_argument("--mode=", "all")

	print("analyze_draft_picks: input=%s output_dir=%s mode=%s" % [input_path, output_dir, mode])

	var global_input_path := ProjectSettings.globalize_path(input_path)
	print("analyze_draft_picks: global_input_path=%s" % global_input_path)

	var f := FileAccess.open(global_input_path, FileAccess.READ)
	if f == null:
		push_error("analyze_draft_picks: could not open %s" % input_path)
		quit(1)
		return

	var lines := f.get_as_text().split("\n")
	f.close()

	if lines.is_empty():
		push_error("analyze_draft_pategies: empty file")
		quit(1)
		return

	var header := lines[0].split(",")
	print("analyze_draft_picks: header has %d columns: %s" % [header.size(), header])

	var rows := []
	for i in range(1, lines.size()):
		var line := lines[i].strip_edges()
		if line.is_empty():
			continue
		var parts := line.split(",")
		if parts.size() >= 12:
			rows.append(parts)

	print("analyze_draft_picks: loaded %d rows" % rows.size())
	if rows.is_empty():
		push_error("analyze_draft_picks: no rows loaded")
		quit(1)
		return

	if mode == "all" or mode == "frequency":
		print("analyze_draft_picks: running frequency analysis...")
		_analyze_frequency(rows, output_dir)
	if mode == "all" or mode == "contextual":
		print("analyze_draft_picks: running contextual analysis...")
		_analyze_contextual(rows, output_dir)
	if mode == "all" or mode == "comparative":
		print("analyze_draft_picks: running comparative analysis...")
		_analyze_comparative(rows, output_dir)

	print("")
	print("================ ANALYSIS COMPLETE ================")
	print("output_dir: %s" % output_dir)
	print("====================================================")
	quit(0)


func _analyze_frequency(rows: Array, output_dir: String) -> void:
	var pick_counts := {}
	var strategies := ["certified", "random"]

	for strat in strategies:
		pick_counts[strat] = {}

	for row in rows:
		var strategy_a := row[5]
		var pick_a := row[6]
		var strategy_b := row[8]
		var pick_b := row[9]

		if strategy_a in pick_counts and not pick_a.is_empty():
			if not pick_a in pick_counts[strategy_a]:
				pick_counts[strategy_a][pick_a] = 0
			pick_counts[strategy_a][pick_a] += 1

		if strategy_b in pick_counts and not pick_b.is_empty():
			if not pick_b in pick_counts[strategy_b]:
				pick_counts[strategy_b][pick_b] = 0
			pick_counts[strategy_b][pick_b] += 1

	print("  frequency: %d strategies, %d rows processed" % [strategies.size(), rows.size()])

	var output_path := "%s/pick_frequency.csv" % output_dir
	var global_path := ProjectSettings.globalize_path(output_path)
	print("  frequency: writing to %s" % global_path)
	var out_f := FileAccess.open(global_path, FileAccess.WRITE)
	if out_f == null:
		push_error("analyze_draft_picks: could not open %s" % output_path)
		return

	out_f.store_string("champion,%s\n" % ",".join(strategies))

	var all_champions := {}
	for strat in strategies:
		for champ in pick_counts[strat]:
			all_champions[champ] = true

	for champ in all_champions:
		var line_parts = [champ]
		for strat in strategies:
			var count := pick_counts[strat].get(champ, 0)
			line_parts.append(str(count))
		out_f.store_string(",".join(line_parts) + "\n")

	out_f.close()
	print("  frequency analysis: %s (%d champions)" % [output_path, all_champions.size()])


func _analyze_contextual(rows: Array, output_dir: String) -> void:
	var enemy_cooccurrence := {}
	var strategies := ["certified", "random"]

	for strat in strategies:
		enemy_cooccurrence[strat] = {}

	for row in rows:
		var strategy_a := row[5]
		var pick_a := row[6]
		var enemies_a := row[3].split("|") if row.size() > 3 else []
		var strategy_b := row[8]
		var pick_b := row[9]
		var enemies_b := row[3].split("|") if row.size() > 3 else []

		_process_contextual_row(strategy_a, pick_a, enemies_a, enemy_cooccurrence)
		_process_contextual_row(strategy_b, pick_b, enemies_b, enemy_cooccurrence)

	var output_path := "%s/pick_contextual.csv" % output_dir
	var global_path := ProjectSettings.globalize_path(output_path)
	print("  contextual: writing to %s" % global_path)
	var out_f := FileAccess.open(global_path, FileAccess.WRITE)
	if out_f == null:
		push_error("analyze_draft_picks: could not open %s" % output_path)
		return

	out_f.store_string("picked_champion,strategy,enemy_champion,co_occurrence_count\n")

	for strat in strategies:
		for picked_champ in enemy_cooccurrence[strat]:
			for enemy_champ in enemy_cooccurrence[strat][picked_champ]:
				var count := enemy_cooccurrence[strat][picked_champ][enemy_champ]
				out_f.store_string("%s,%s,%s,%d\n" % [picked_champ, strat, enemy_champ, count])

	out_f.close()
	print("  contextual analysis: %s" % output_path)


func _process_contextual_row(strategy: String, pick: String, enemies: Array, enemy_cooccurrence: Dictionary) -> void:
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


func _analyze_comparative(rows: Array, output_dir: String) -> void:
	var disagreements := {}

	for row in rows:
		var trial_id := row[0]
		var depth := row[1]
		var allies := row[2]
		var enemies := row[3]
		var strategy_a := row[5]
		var pick_a := row[6]
		var winrate_a := float(row[7])
		var strategy_b := row[8]
		var pick_b := row[9]
		var winrate_b := float(row[10])

		var key := "%s_%s_%s_%s" % [depth, allies, enemies, trial_id]
		if key not in disagreements:
			disagreements[key] = {
				"depth": depth,
				"allies": allies,
				"enemies": enemies,
				"picks": {},
				"winrates": {}
			}

		disagreements[key]["picks"][strategy_a] = pick_a
		disagreements[key]["picks"][strategy_b] = pick_b
		disagreements[key]["winrates"][strategy_a] = winrate_a
		disagreements[key]["winrates"][strategy_b] = winrate_b

	var output_path := "%s/pick_comparative.csv" % output_dir
	var global_path := ProjectSettings.globalize_path(output_path)
	print("  comparative: writing to %s" % global_path)
	var out_f := FileAccess.open(global_path, FileAccess.WRITE)
	if out_f == null:
		push_error("analyze_draft_picks: could not open %s" % output_path)
		return

	var strategies := ["certified", "random"]
	var headers := ["allies", "enemies", "depth"]
	for s in strategies:
		headers.append("%s_pick" % s)
	for s in strategies:
		headers.append("%s_winrate" % s)
	out_f.store_string(",".join(headers) + "\n")

	for key in disagreements:
		var data := disagreements[key]
		var line_parts = [data["allies"], data["enemies"], data["depth"]]
		for strat in strategies:
			line_parts.append(data["picks"].get(strat, ""))
		for strat in strategies:
			line_parts.append(str(data["winrates"].get(strat, 0.0)))
		out_f.store_string(",".join(line_parts) + "\n")

	out_f.close()
	print("  comparative analysis: %s" % output_path)
