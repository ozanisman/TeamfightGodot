extends RefCounted

## Signal Variance Analysis
##
## Analyzes base/synergy/matchup/composition signal variance across champions
## to test the hypothesis that limited signal variance is the bottleneck preventing
## draft prediction differentiation.
##
## Usage:
##   var analyzer_script := load("res://scripts/tools/analyze_signal_variance.gd")
##   var analyzer := analyzer_script.new()
##   analyzer.analyze("res://stats_output", [5], 20)

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")

## Bayesian smoothing parameters (same as draft recommender)
const SMOOTHING_K: float = 10.0
const PRIOR_WINRATE: float = 0.5

## Flat-profile threshold: signals within ±0.05 of 0.5 are considered flat
const FLAT_PROFILE_THRESHOLD: float = 0.05


## Main analysis function
## stats_dir: Path to stats directory containing CSV files
## team_sizes: Array of team sizes to analyze (default [5] for 5v5)
## min_samples: Minimum sample count for a champion to be included (default 20)
## csv_output_path: Optional path to write CSV output
## log_path: Optional path to write terminal output to file
## Returns: Dictionary with analysis results
func analyze(stats_dir: String, team_sizes: Array = [5], min_samples: int = 20, csv_output_path: String = "", log_path: String = "") -> Dictionary:
	var log_lines: Array = []
	log_lines.append("Signal Variance Analysis")
	log_lines.append("========================")
	log_lines.append("Stats dir: %s" % stats_dir)
	log_lines.append("Team sizes: %s" % str(team_sizes))
	log_lines.append("Min samples: %d" % min_samples)
	log_lines.append("")

	log_lines.append("Loading combat_stats.csv...")
	var combat_stats: Dictionary = _load_combat_stats(stats_dir + "/combat_stats.csv", team_sizes, min_samples)
	log_lines.append("Loaded combat_stats")

	log_lines.append("Loading matchup_with.csv...")
	var synergy_stats: Dictionary = _load_matchup_stats(stats_dir + "/matchup_with.csv", team_sizes, min_samples)
	log_lines.append("Loaded synergy_stats")

	log_lines.append("Loading matchup_vs.csv...")
	var counter_stats: Dictionary = _load_matchup_stats(stats_dir + "/matchup_vs.csv", team_sizes, min_samples)
	log_lines.append("Loaded counter_stats")

	log_lines.append("Loading role_combinations.csv...")
	var composition_stats: Dictionary = _load_composition_stats(stats_dir + "/role_combinations.csv", team_sizes, min_samples)
	log_lines.append("Loaded composition_stats")

	log_lines.append("Loaded %d champions from combat_stats.csv" % combat_stats.size())
	log_lines.append("Loaded %d synergy pairs from matchup_with.csv" % synergy_stats.size())
	log_lines.append("Loaded %d counter pairs from matchup_vs.csv" % counter_stats.size())
	log_lines.append("Loaded %d composition fingerprints from role_combinations.csv" % composition_stats.size())
	log_lines.append("")

	log_lines.append("Computing champion profiles...")
	# Compute champion profiles
	var profiles: Array = _compute_champion_profiles(combat_stats, synergy_stats, counter_stats, composition_stats)
	log_lines.append("Computed profiles for %d champions" % profiles.size())
	log_lines.append("")

	# Compute global statistics
	var global_stats: Dictionary = _compute_global_statistics(profiles)

	# Compute correlations
	var correlations: Dictionary = _compute_correlations(profiles)

	# Compute role-level statistics
	var role_stats: Dictionary = _compute_role_level_stats(profiles)

	# Identify flat profiles
	var flat_profiles: Array = _identify_flat_profiles(profiles, FLAT_PROFILE_THRESHOLD)
	log_lines.append("Identified %d flat-profile champions" % flat_profiles.size())
	log_lines.append("")

	# Build terminal report
	var report_lines: Array = _build_terminal_report(global_stats, correlations, role_stats, flat_profiles)
	log_lines.append_array(report_lines)

	# Write CSV if requested
	if not csv_output_path.is_empty():
		_write_csv(profiles, csv_output_path)
		log_lines.append("Wrote CSV to %s" % csv_output_path)

	# Write log file if requested
	if not log_path.is_empty():
		_write_log(log_lines, log_path)

	# Also print to console
	for line in log_lines:
		print(line)

	return {
		"profiles": profiles,
		"global_stats": global_stats,
		"correlations": correlations,
		"role_stats": role_stats,
		"flat_profiles": flat_profiles,
	}


## Load combat_stats.csv
## Returns: Dictionary {champion: {winrate, samples, role}}
func _load_combat_stats(path: String, team_sizes: Array, min_samples: int) -> Dictionary:
	var result: Dictionary = {}
	var abs_path: String = ProjectSettings.globalize_path(path)
	var f := FileAccess.open(abs_path, FileAccess.READ)
	if f == null:
		push_error("Failed to open %s" % path)
		return result

	var header: PackedStringArray = f.get_csv_line()
	var team_size_col: int = header.find("team_size")
	var hero_col: int = header.find("hero")
	var winrate_col: int = header.find("win_rate")
	var total_games_col: int = header.find("total_games")
	var role_col: int = header.find("role")

	if team_size_col < 0 or hero_col < 0 or winrate_col < 0 or total_games_col < 0:
		push_error("Missing required columns in combat_stats.csv")
		f.close()
		return result

	while not f.eof_reached():
		var line: PackedStringArray = f.get_csv_line()
		if line.size() <= 1:
			continue

		var team_size: int = int(line[team_size_col])
		if team_size not in team_sizes:
			continue

		var hero: String = line[hero_col]
		var winrate: float = float(line[winrate_col])
		var samples: int = int(line[total_games_col])

		if samples < min_samples:
			continue

		var role: String = "unknown"
		if role_col >= 0 and role_col < line.size():
			role = line[role_col]

		result[hero] = {
			"winrate": winrate,
			"samples": samples,
			"role": role
		}

	f.close()
	return result


## Load matchup stats (synergy or counter)
## Returns: Dictionary {champion: {other: {winrate, samples}}}
func _load_matchup_stats(path: String, team_sizes: Array, min_samples: int) -> Dictionary:
	var result: Dictionary = {}
	var abs_path: String = ProjectSettings.globalize_path(path)
	var f := FileAccess.open(abs_path, FileAccess.READ)
	if f == null:
		push_error("Failed to open %s" % path)
		return result

	var header: PackedStringArray = f.get_csv_line()
	var champion_col: int = header.find("champion")
	var other_col: int = header.find("ally")
	if other_col < 0:
		other_col = header.find("opponent")
	var winrate_col: int = header.find("winrate")
	var wins_col: int = header.find("wins")
	var losses_col: int = header.find("losses")

	if champion_col < 0 or other_col < 0 or winrate_col < 0:
		push_error("Missing required columns in matchup CSV")
		f.close()
		return result

	while not f.eof_reached():
		var line: PackedStringArray = f.get_csv_line()
		if line.size() <= 1:
			continue

		var champion: String = line[champion_col]
		var other: String = line[other_col]
		var winrate: float = float(line[winrate_col])
		var samples: int = 0

		if wins_col >= 0 and losses_col >= 0:
			samples = int(line[wins_col]) + int(line[losses_col])

		# Don't filter by min_samples for matchup data - we need all pairs
		# to compute averages, even if individual pairs have low samples

		if not result.has(champion):
			result[champion] = {}
		result[champion][other] = {
			"winrate": winrate,
			"samples": samples
		}

	f.close()
	return result


## Load composition stats
## Returns: Dictionary {fingerprint: {winrate, samples}}
func _load_composition_stats(path: String, team_sizes: Array, min_samples: int) -> Dictionary:
	var result: Dictionary = {}
	var abs_path: String = ProjectSettings.globalize_path(path)
	var f := FileAccess.open(abs_path, FileAccess.READ)
	if f == null:
		push_error("Failed to open %s" % path)
		return result

	var header: PackedStringArray = f.get_csv_line()
	var team_size_col: int = header.find("team_size")
	var fingerprint_col: int = header.find("role_fingerprint")
	var winrate_col: int = header.find("win_rate")
	var total_games_col: int = header.find("total_games")

	if team_size_col < 0 or fingerprint_col < 0 or winrate_col < 0:
		push_error("Missing required columns in role_combinations.csv")
		f.close()
		return result

	while not f.eof_reached():
		var line: PackedStringArray = f.get_csv_line()
		if line.size() <= 1:
			continue

		var team_size: int = int(line[team_size_col])
		if team_size not in team_sizes:
			continue

		var fingerprint: String = line[fingerprint_col]
		var winrate: float = float(line[winrate_col])
		var samples: int = 0

		if total_games_col >= 0:
			samples = int(line[total_games_col])

		if samples < min_samples:
			continue

		result[fingerprint] = {
			"winrate": winrate,
			"samples": samples
		}

	f.close()
	return result


## Compute per-champion signal profiles
func _compute_champion_profiles(combat_stats: Dictionary, synergy_stats: Dictionary, counter_stats: Dictionary, composition_stats: Dictionary) -> Array:
	var profiles: Array = []

	for champion in combat_stats:
		var combat: Dictionary = combat_stats[champion]
		var role: String = combat["role"]
		var samples: int = combat["samples"]

		# Base winrate
		var base_raw: float = combat["winrate"]
		var base_smoothed: float = _apply_bayesian_smoothing(base_raw, samples)

		# Average synergy
		var synergy_data: Dictionary = synergy_stats.get(champion, {})
		var synergy_values: Array = []
		var synergy_raw_sum: float = 0.0
		var synergy_count: int = 0
		for other in synergy_data:
			var data: Dictionary = synergy_data[other]
			synergy_values.append(data["winrate"])
			synergy_raw_sum += data["winrate"]
			synergy_count += 1
		var avg_synergy_raw: float = synergy_raw_sum / float(synergy_count) if synergy_count > 0 else 0.5
		var avg_synergy_smoothed: float = _apply_bayesian_smoothing(avg_synergy_raw, synergy_count)
		var synergy_variance: float = _compute_variance(synergy_values)

		# Average counter
		var counter_data: Dictionary = counter_stats.get(champion, {})
		var counter_values: Array = []
		var counter_raw_sum: float = 0.0
		var counter_count: int = 0
		for other in counter_data:
			var data: Dictionary = counter_data[other]
			counter_values.append(data["winrate"])
			counter_raw_sum += data["winrate"]
			counter_count += 1
		var avg_counter_raw: float = counter_raw_sum / float(counter_count) if counter_count > 0 else 0.5
		var avg_counter_smoothed: float = _apply_bayesian_smoothing(avg_counter_raw, counter_count)
		var counter_variance: float = _compute_variance(counter_values)

		# Composition (placeholder - needs team context)
		var composition_raw: float = 0.5
		var composition_smoothed: float = 0.5

		profiles.append({
			"champion": champion,
			"role": role,
			"samples": samples,
			"base_raw": base_raw,
			"base_smoothed": base_smoothed,
			"avg_synergy_raw": avg_synergy_raw,
			"avg_synergy_smoothed": avg_synergy_smoothed,
			"synergy_variance": synergy_variance,
			"avg_counter_raw": avg_counter_raw,
			"avg_counter_smoothed": avg_counter_smoothed,
			"counter_variance": counter_variance,
			"composition_raw": composition_raw,
			"composition_smoothed": composition_smoothed,
		})

	return profiles


## Compute global statistics
func _compute_global_statistics(profiles: Array) -> Dictionary:
	var signal_names: Array = ["base_raw", "base_smoothed", "avg_synergy_raw", "avg_synergy_smoothed", "avg_counter_raw", "avg_counter_smoothed"]
	var result: Dictionary = {}

	for sig in signal_names:
		var values: Array = []
		for profile in profiles:
			values.append(profile[sig])

		var mean: float = _compute_mean(values)
		var variance: float = _compute_variance(values)
		var stddev: float = sqrt(variance)
		var min_val: float = values.min()
		var max_val: float = values.max()
		var cv: float = stddev / mean if mean > 0 else 0.0

		result[sig] = {
			"mean": mean,
			"variance": variance,
			"stddev": stddev,
			"min": min_val,
			"max": max_val,
			"cv": cv
		}

	return result


## Compute correlation matrix
func _compute_correlations(profiles: Array) -> Dictionary:
	var signal_pairs: Array = [
		["base_smoothed", "avg_synergy_smoothed"],
		["base_smoothed", "avg_counter_smoothed"],
		["avg_synergy_smoothed", "avg_counter_smoothed"],
		["base_smoothed", "synergy_variance"],
		["base_smoothed", "counter_variance"],
		["synergy_variance", "counter_variance"]
	]

	var result: Dictionary = {}

	for pair in signal_pairs:
		var signal1: String = pair[0]
		var signal2: String = pair[1]
		var values1: Array = []
		var values2: Array = []

		for profile in profiles:
			values1.append(profile[signal1])
			values2.append(profile[signal2])

		var correlation: float = _pearson_correlation(values1, values2)
		result["%s_vs_%s" % [signal1, signal2]] = correlation

	return result


## Compute role-level statistics
func _compute_role_level_stats(profiles: Array) -> Dictionary:
	var result: Dictionary = {}
	var role_map: Dictionary = {}

	for profile in profiles:
		var role: String = profile["role"]
		if not role_map.has(role):
			role_map[role] = []
		role_map[role].append(profile)

	for role in role_map:
		var role_profiles: Array = role_map[role]
		var signal_names: Array = ["base_smoothed", "avg_synergy_smoothed", "avg_counter_smoothed"]
		var role_result: Dictionary = {"count": role_profiles.size()}

		for sig in signal_names:
			var values: Array = []
			for profile in role_profiles:
				values.append(profile[sig])

			var mean: float = _compute_mean(values)
			var variance: float = _compute_variance(values)
			var stddev: float = sqrt(variance)

			role_result[sig] = {
				"mean": mean,
				"variance": variance,
				"stddev": stddev
			}

		result[role] = role_result

	return result


## Identify flat-profile champions
func _identify_flat_profiles(profiles: Array, threshold: float) -> Array:
	var flat_profiles: Array = []

	for profile in profiles:
		var base_flat: bool = absf(profile["base_smoothed"] - 0.5) <= threshold
		var synergy_flat: bool = absf(profile["avg_synergy_smoothed"] - 0.5) <= threshold
		var counter_flat: bool = absf(profile["avg_counter_smoothed"] - 0.5) <= threshold

		if base_flat and synergy_flat and counter_flat:
			var flat_profile: Dictionary = profile.duplicate()
			flat_profile["is_flat_profile"] = true
			flat_profiles.append(flat_profile)

	return flat_profiles


## Apply Bayesian smoothing
func _apply_bayesian_smoothing(raw_winrate: float, samples: int) -> float:
	if samples <= 0:
		return PRIOR_WINRATE
	var weight: float = float(samples) / (float(samples) + SMOOTHING_K)
	return weight * raw_winrate + (1.0 - weight) * PRIOR_WINRATE


## Compute mean
func _compute_mean(values: Array) -> float:
	if values.is_empty():
		return 0.0
	var sum: float = 0.0
	for v in values:
		sum += v
	return sum / float(values.size())


## Compute variance
func _compute_variance(values: Array) -> float:
	if values.size() < 2:
		return 0.0
	var mean: float = _compute_mean(values)
	var sum_sq_diff: float = 0.0
	for v in values:
		var diff: float = v - mean
		sum_sq_diff += diff * diff
	return sum_sq_diff / float(values.size())


## Pearson correlation
func _pearson_correlation(x: Array, y: Array) -> float:
	if x.size() < 2 or x.size() != y.size():
		return 0.0

	var n: float = float(x.size())
	var mean_x: float = _compute_mean(x)
	var mean_y: float = _compute_mean(y)

	var sum_xy: float = 0.0
	var sum_x2: float = 0.0
	var sum_y2: float = 0.0

	for i in range(x.size()):
		var dx: float = x[i] - mean_x
		var dy: float = y[i] - mean_y
		sum_xy += dx * dy
		sum_x2 += dx * dx
		sum_y2 += dy * dy

	var denominator: float = sqrt(sum_x2 * sum_y2)
	if denominator <= 0.0:
		return 0.0

	return sum_xy / denominator


## Build terminal report (returns lines instead of printing)
func _build_terminal_report(global_stats: Dictionary, correlations: Dictionary, role_stats: Dictionary, flat_profiles: Array) -> Array:
	var lines: Array = []
	lines.append("=== Global Signal Statistics ===")
	lines.append("")
	lines.append("Signal                      Mean      StdDev        Min        Max         CV")
	lines.append("-------------------------------------------------------------------------------------")

	var signal_names: Array = ["base_smoothed", "avg_synergy_smoothed", "avg_counter_smoothed"]
	for sig in signal_names:
		var stats: Dictionary = global_stats[sig]
		lines.append(str(sig).lpad(25) + " " + str(snapped(stats["mean"], 0.0001)).rpad(10) + " " + str(snapped(stats["stddev"], 0.0001)).rpad(10) + " " + str(snapped(stats["min"], 0.0001)).rpad(10) + " " + str(snapped(stats["max"], 0.0001)).rpad(10) + " " + str(snapped(stats["cv"], 0.0001)).rpad(10))

	lines.append("")
	lines.append("=== Signal Correlations ===")
	lines.append("")
	lines.append("Pair                                     Correlation")
	lines.append("-------------------------------------------------------")

	for pair in correlations:
		var corr: float = correlations[pair]
		lines.append(str(pair).lpad(40) + " " + str(snapped(corr, 0.0001)).rpad(10))

	lines.append("")
	lines.append("=== Role-Level Statistics ===")
	lines.append("")

	for role in role_stats:
		var stats: Dictionary = role_stats[role]
		lines.append(str(role.capitalize()) + " (n=" + str(stats["count"]) + ")")
		lines.append("  Base:       mean=" + str(snapped(stats["base_smoothed"]["mean"], 0.0001)) + ", stddev=" + str(snapped(stats["base_smoothed"]["stddev"], 0.0001)))
		lines.append("  Synergy:    mean=" + str(snapped(stats["avg_synergy_smoothed"]["mean"], 0.0001)) + ", stddev=" + str(snapped(stats["avg_synergy_smoothed"]["stddev"], 0.0001)))
		lines.append("  Counter:    mean=" + str(snapped(stats["avg_counter_smoothed"]["mean"], 0.0001)) + ", stddev=" + str(snapped(stats["avg_counter_smoothed"]["stddev"], 0.0001)))
		lines.append("")

	lines.append("=== Flat-Profile Champions ===")
	lines.append("")
	lines.append("Champion            Base    Synergy   Counter    Samples")
	lines.append("----------------------------------------------------------------------")

	for profile in flat_profiles:
		lines.append(str(profile["champion"]).lpad(20) + " " + str(snapped(profile["base_smoothed"], 0.0001)).rpad(10) + " " + str(snapped(profile["avg_synergy_smoothed"], 0.0001)).rpad(10) + " " + str(snapped(profile["avg_counter_smoothed"], 0.0001)).rpad(10) + " " + str(profile["samples"]).rpad(10))

	lines.append("")
	lines.append("=== Key Insights ===")
	lines.append("")

	for sig in signal_names:
		var stats: Dictionary = global_stats[sig]
		var variance_level: String = "low" if stats["stddev"] < 0.02 else "medium" if stats["stddev"] < 0.05 else "high"
		lines.append(str(sig) + " variance: " + str(snapped(stats["stddev"], 0.0001)) + " (" + variance_level + ")")

	for pair in correlations:
		var corr: float = correlations[pair]
		var correlation_level: String = "low" if absf(corr) < 0.3 else "medium" if absf(corr) < 0.7 else "high"
		if absf(corr) >= 0.7:
			lines.append(str(pair) + " correlation: " + str(snapped(corr, 0.0001)) + " (" + correlation_level + " - potentially redundant)")

	lines.append("")
	return lines


## Write CSV output
func _write_csv(profiles: Array, output_path: String) -> void:
	var lines: Array = []
	lines.append("champion,role,samples,base_raw,base_smoothed,avg_synergy_raw,avg_synergy_smoothed,synergy_variance,avg_counter_raw,avg_counter_smoothed,counter_variance,composition_raw,composition_smoothed,is_flat_profile")

	for profile in profiles:
		var is_flat: int = 1 if profile.get("is_flat_profile", false) else 0
		lines.append("%s,%s,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d" % [
			profile["champion"],
			profile["role"],
			profile["samples"],
			profile["base_raw"],
			profile["base_smoothed"],
			profile["avg_synergy_raw"],
			profile["avg_synergy_smoothed"],
			profile["synergy_variance"],
			profile["avg_counter_raw"],
			profile["avg_counter_smoothed"],
			profile["counter_variance"],
			profile["composition_raw"],
			profile["composition_smoothed"],
			is_flat
		])

	var content: String = "\n".join(lines) + "\n"
	var abs_path: String = ProjectSettings.globalize_path(output_path)
	var f := FileAccess.open(abs_path, FileAccess.WRITE)
	if f == null:
		push_error("Failed to open %s for writing" % output_path)
		return

	f.store_string(content)
	f.flush()
	f.close()


## Write log file
func _write_log(lines: Array[String], output_path: String) -> void:
	var content: String = "\n".join(lines) + "\n"
	var abs_path: String = ProjectSettings.globalize_path(output_path)
	var f := FileAccess.open(abs_path, FileAccess.WRITE)
	if f == null:
		push_error("Failed to open %s for writing" % output_path)
		return

	f.store_string(content)
	f.flush()
	f.close()
