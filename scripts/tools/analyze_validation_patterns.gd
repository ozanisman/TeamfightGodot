extends SceneTree

## Analyzes validation patterns to identify when validation is reliable.
##
## Reads existing validation CSVs and analyzes patterns in:
## - When baseline and rollout agree vs disagree
## - Regret magnitude distribution
## - Depth-specific patterns

const DEFAULT_OUTPUT := "res://stats_output/validation_patterns_analysis.csv"


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	var depth := int(_extract_argument("--depth=", "4"))
	var input_path := _extract_argument("--input=", "res://stats_output/pick_recommendation_validation_depth%d.csv" % depth)
	var output_path := _extract_argument("--output=", DEFAULT_OUTPUT)

	print("analyze_validation_patterns: depth=%d input=%s" % [depth, input_path])

	var f := FileAccess.open(ProjectSettings.globalize_path(input_path), FileAccess.READ)
	if f == null:
		push_error("analyze_validation_patterns: could not open %s" % input_path)
		quit(1)
		return

	var lines := f.get_as_text().split("\n")
	f.close()

	if lines.size() < 2:
		push_error("analyze_validation_patterns: CSV has no data")
		quit(1)
		return

	var header := lines[0].split(",")
	var analysis_lines: Array[String] = []
	
	var agree_count := 0
	var disagree_count := 0
	var total_regret := 0.0
	var max_regret := 0.0
	var regret_values: Array[float] = []
	var score_values: Array[float] = []

	for i in range(1, lines.size()):
		var line := lines[i].strip_edges()
		if line.is_empty():
			continue
		
		var parts := line.split(",")
		if parts.size() < 11:
			continue
		
		var regret := float(parts[9])
		var top1_overlap := int(parts[11])
		var rollout_score := float(parts[8])
		
		if top1_overlap == 1:
			agree_count += 1
		else:
			disagree_count += 1
			total_regret += regret
			regret_values.append(regret)
		
		max_regret = maxf(max_regret, regret)
		score_values.append(rollout_score)

	analysis_lines.append("depth,%d" % depth)
	analysis_lines.append("total_states,%d" % (agree_count + disagree_count))
	analysis_lines.append("agree_count,%d" % agree_count)
	analysis_lines.append("disagree_count,%d" % disagree_count)
	analysis_lines.append("agree_rate,%.4f" % (float(agree_count) / float(agree_count + disagree_count)))
	analysis_lines.append("avg_regret_when_disagree,%.6f" % (total_regret / float(disagree_count) if disagree_count > 0 else 0.0))
	analysis_lines.append("max_regret,%.6f" % max_regret)
	
	if regret_values.size() > 0:
		regret_values.sort()
		var median_idx := regret_values.size() / 2
		var median_regret := regret_values[median_idx]
		analysis_lines.append("median_regret,%.6f" % median_regret)
		
		var regret_std := _std_dev(regret_values)
		analysis_lines.append("regret_std,%.6f" % regret_std)
	
	if score_values.size() > 0:
		var score_std := _std_dev(score_values)
		var score_mean := _mean(score_values)
		var score_cv := score_std / score_mean if score_mean > 0 else 0.0
		analysis_lines.append("score_mean,%.6f" % score_mean)
		analysis_lines.append("score_std,%.6f" % score_std)
		analysis_lines.append("score_cv,%.6f" % score_cv)

	var out_f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if out_f == null:
		push_error("analyze_validation_patterns: could not open %s" % output_path)
		quit(1)
		return
	out_f.store_string("\n".join(analysis_lines) + "\n")
	out_f.close()

	print("")
	print("================ VALIDATION PATTERNS ANALYSIS ================")
	print("Depth: %d" % depth)
	print("Total states: %d" % (agree_count + disagree_count))
	print("Agree: %d (%.1f%%)" % [agree_count, 100.0 * float(agree_count) / float(agree_count + disagree_count)])
	print("Disagree: %d (%.1f%%)" % [disagree_count, 100.0 * float(disagree_count) / float(agree_count + disagree_count)])
	if disagree_count > 0:
		print("Avg regret when disagree: %.4f pp" % (total_regret / float(disagree_count) * 100.0))
	print("Max regret: %.4f pp" % (max_regret * 100.0))
	print("CSV written to: %s" % output_path)
	print("============================================================")
	quit(0)


func _mean(values: Array[float]) -> float:
	if values.is_empty():
		return 0.0
	var sum := 0.0
	for v in values:
		sum += v
	return sum / float(values.size())


func _std_dev(values: Array[float]) -> float:
	if values.size() < 2:
		return 0.0
	var m := _mean(values)
	var sum_sq := 0.0
	for v in values:
		var diff := v - m
		sum_sq += diff * diff
	return sqrt(sum_sq / float(values.size() - 1))
