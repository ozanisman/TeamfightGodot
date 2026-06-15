#include "sim_draft_ai_stats_database.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

namespace sim {
namespace draft_ai {

bool DraftStatsDatabase::load_from_dir(const String &dir_path) {
	_base_stats.clear();
	_synergy_stats.clear();
	_counter_stats.clear();
	_champion_roles.clear();
	_role_combination_stats.clear();
	_loaded = false;
	_last_error = String();

	String normalized = dir_path.trim_suffix("/");

	if (!_load_combat_stats(normalized + "/combat_stats.csv")) {
		return false;
	}

	if (!_load_matchup_stats(normalized + "/matchup_with.csv", _synergy_stats)) {
		return false;
	}

	if (!_load_matchup_stats(normalized + "/matchup_vs.csv", _counter_stats)) {
		return false;
	}

	if (!_load_role_combinations(normalized + "/role_combinations.csv")) {
		return false;
	}

	_loaded = true;
	return true;
}

bool DraftStatsDatabase::is_loaded() const {
	return _loaded;
}

String DraftStatsDatabase::last_error() const {
	return _last_error;
}

DraftStatValue DraftStatsDatabase::base_winrate_for(const StringName &champion) const {
	auto it = _base_stats.find(champion);
	if (it == _base_stats.end()) {
		return _create_missing_value();
	}

	DraftPredictionConfig config;
	return _apply_smoothing(it->second, config);
}

DraftStatValue DraftStatsDatabase::synergy_winrate_for(const StringName &champion, const StringName &ally) const {
	auto it = _synergy_stats.find(champion);
	if (it != _synergy_stats.end()) {
		auto jt = it->second.find(ally);
		if (jt != it->second.end()) {
			DraftPredictionConfig config;
			return _apply_smoothing(jt->second, config);
		}
	}

	// Try reverse direction (synergy is symmetric)
	auto it2 = _synergy_stats.find(ally);
	if (it2 != _synergy_stats.end()) {
		auto jt2 = it2->second.find(champion);
		if (jt2 != it2->second.end()) {
			DraftPredictionConfig config;
			return _apply_smoothing(jt2->second, config);
		}
	}

	return _create_missing_value();
}

DraftStatValue DraftStatsDatabase::counter_winrate_for(const StringName &champion, const StringName &enemy) const {
	auto it = _counter_stats.find(champion);
	if (it != _counter_stats.end()) {
		auto jt = it->second.find(enemy);
		if (jt != it->second.end()) {
			DraftPredictionConfig config;
			return _apply_smoothing(jt->second, config);
		}
	}

	return _create_missing_value();
}

StringName DraftStatsDatabase::role_for(const StringName &champion) const {
	auto it = _champion_roles.find(champion);
	if (it != _champion_roles.end()) {
		return it->second;
	}
	return StringName();
}

DraftStatValue DraftStatsDatabase::role_combination_value_for(const std::vector<StringName> &roles) const {
	String fingerprint = build_role_fingerprint(roles);
	auto it = _role_combination_stats.find(fingerprint);
	if (it != _role_combination_stats.end()) {
		DraftPredictionConfig config;
		return _apply_smoothing(it->second, config);
	}
	return _create_missing_value();
}

DraftStatValue DraftStatsDatabase::partial_role_combination_value_for(const std::vector<StringName> &roles, int &out_match_count) const {
	out_match_count = 0;
	
	// Build fingerprint for the partial role set
	String partial_fingerprint = build_role_fingerprint(roles);
	
	// Count role frequencies in partial set (for multiset matching)
	std::map<String, int> partial_role_counts;
	for (const StringName &role : roles) {
		String role_str = String(role);
		if (!role_str.is_empty() && role_str != "unknown") {
			partial_role_counts[role_str]++;
		}
	}
	
	// If no valid roles, return neutral
	if (partial_role_counts.empty()) {
		return _create_missing_value();
	}
	
	// If we have 5+ roles, use exact lookup
	if (partial_role_counts.size() >= 5) {
		return role_combination_value_for(roles);
	}
	
	// Aggregate stats from all full comps containing the partial multiset
	StatRecord aggregated;
	for (const auto &entry : _role_combination_stats) {
		const String &full_fingerprint = entry.first;
		const StatRecord &full_record = entry.second;
		
		// Check if full comp contains the partial role multiset
		std::map<String, int> full_role_counts;
		
		// Manually parse fingerprint "role1 + role2 + role3"
		String remaining = full_fingerprint;
		while (!remaining.is_empty()) {
			int plus_idx = remaining.find(" + ");
			if (plus_idx >= 0) {
				String role = remaining.substr(0, plus_idx);
				if (!role.is_empty() && role != "unknown") {
					full_role_counts[role]++;
				}
				remaining = remaining.substr(plus_idx + 3); // Skip " + "
			} else {
				// Last role
				if (!remaining.is_empty() && remaining != "unknown") {
					full_role_counts[remaining]++;
				}
				break;
			}
		}
		
		// Check if partial multiset is subset of full multiset
		bool contains_all = true;
		for (const auto &partial_entry : partial_role_counts) {
			const String &role = partial_entry.first;
			int required_count = partial_entry.second;
			auto full_it = full_role_counts.find(role);
			if (full_it == full_role_counts.end() || full_it->second < required_count) {
				contains_all = false;
				break;
			}
		}
		
		if (contains_all) {
			aggregated.wins += full_record.wins;
			aggregated.losses += full_record.losses;
			aggregated.samples += full_record.samples;
			out_match_count++;
		}
	}
	
	// If no matching full comps, return neutral
	if (out_match_count == 0) {
		return _create_missing_value();
	}
	
	// Calculate aggregated raw winrate
	aggregated.raw_winrate = aggregated.samples > 0 ? 
		static_cast<double>(aggregated.wins) / aggregated.samples : 0.5;
	
	// Apply smoothing
	DraftPredictionConfig config;
	return _apply_smoothing(aggregated, config);
}

String DraftStatsDatabase::build_role_fingerprint(const std::vector<StringName> &roles) {
	std::vector<String> valid_roles;
	for (const StringName &role : roles) {
		String role_str = String(role);
		if (!role_str.is_empty() && role_str != "unknown") {
			valid_roles.push_back(role_str);
		}
	}
	
	// Sort alphabetically
	std::sort(valid_roles.begin(), valid_roles.end());
	
	// Join with " + "
	String result;
	for (size_t i = 0; i < valid_roles.size(); ++i) {
		if (i > 0) {
			result += " + ";
		}
		result += valid_roles[i];
	}
	return result;
}

bool DraftStatsDatabase::_load_combat_stats(const String &path) {
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::ModeFlags::READ);
	if (file.is_null()) {
		_last_error = vformat("DraftStatsDatabase: cannot open %s", path);
		return false;
	}

	if (file->eof_reached()) {
		_last_error = vformat("DraftStatsDatabase: empty CSV %s", path);
		return false;
	}

	std::vector<String> header = _parse_csv_line(file->get_line());
	int64_t hero_col = _column_index(header, "hero");
	int64_t winrate_col = _column_index(header, "win_rate");
	int64_t wins_col = _column_index(header, "wins");
	int64_t losses_col = _column_index(header, "losses");
	int64_t total_games_col = _column_index(header, "total_games");
	int64_t role_col = _column_index(header, "role");

	if (hero_col < 0 || winrate_col < 0) {
		_last_error = vformat("DraftStatsDatabase: missing hero/win_rate columns in %s", path);
		return false;
	}

	while (!file->eof_reached()) {
		String line = file->get_line();
		if (line.strip_edges().is_empty()) {
			continue;
		}

		std::vector<String> row = _parse_csv_line(line);
		String hero = _cell_or_empty(row, hero_col);
		String winrate_str = _cell_or_empty(row, winrate_col);

		if (hero.is_empty() || winrate_str.is_empty()) {
			continue;
		}

		StatRecord record;
		record.raw_winrate = winrate_str.to_float();

		// Try to get wins/losses from columns if available
		if (wins_col >= 0 && losses_col >= 0) {
			String wins_str = _cell_or_empty(row, wins_col);
			String losses_str = _cell_or_empty(row, losses_col);
			if (!wins_str.is_empty() && !losses_str.is_empty()) {
				record.wins = wins_str.to_int();
				record.losses = losses_str.to_int();
				record.samples = record.wins + record.losses;
			}
		} else if (total_games_col >= 0) {
			String total_str = _cell_or_empty(row, total_games_col);
			if (!total_str.is_empty()) {
				record.samples = total_str.to_int();
				// Estimate wins/losses from winrate
				record.wins = static_cast<int>(record.raw_winrate * record.samples);
				record.losses = record.samples - record.wins;
			}
		}

		_base_stats[hero] = record;
		
		// Extract role if available
		if (role_col >= 0) {
			String role_str = _cell_or_empty(row, role_col);
			if (!role_str.is_empty()) {
				_champion_roles[hero] = role_str;
			}
		}
	}

	return true;
}

bool DraftStatsDatabase::_load_role_combinations(const String &path) {
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::ModeFlags::READ);
	if (file.is_null()) {
		_last_error = vformat("DraftStatsDatabase: cannot open %s", path);
		return false;
	}

	if (file->eof_reached()) {
		_last_error = vformat("DraftStatsDatabase: empty CSV %s", path);
		return false;
	}

	std::vector<String> header = _parse_csv_line(file->get_line());
	int64_t team_size_col = _column_index(header, "team_size");
	int64_t fingerprint_col = _column_index(header, "role_fingerprint");
	int64_t winrate_col = _column_index(header, "win_rate");
	int64_t wins_col = _column_index(header, "wins");
	int64_t total_games_col = _column_index(header, "total_games");

	if (fingerprint_col < 0 || winrate_col < 0) {
		_last_error = vformat("DraftStatsDatabase: missing role_fingerprint/win_rate columns in %s", path);
		return false;
	}

	while (!file->eof_reached()) {
		String line = file->get_line();
		if (line.strip_edges().is_empty()) {
			continue;
		}

		std::vector<String> row = _parse_csv_line(line);
		String fingerprint = _cell_or_empty(row, fingerprint_col);
		String winrate_str = _cell_or_empty(row, winrate_col);

		if (fingerprint.is_empty() || winrate_str.is_empty()) {
			continue;
		}

		StatRecord record;
		record.raw_winrate = winrate_str.to_float();

		if (wins_col >= 0 && total_games_col >= 0) {
			String wins_str = _cell_or_empty(row, wins_col);
			String total_str = _cell_or_empty(row, total_games_col);
			if (!wins_str.is_empty() && !total_str.is_empty()) {
				record.wins = wins_str.to_int();
				record.samples = total_str.to_int();
				record.losses = record.samples - record.wins;
			}
		}

		_role_combination_stats[fingerprint] = record;
	}

	return true;
}

bool DraftStatsDatabase::_load_matchup_stats(const String &path, MatchupMap &target) {
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::ModeFlags::READ);
	if (file.is_null()) {
		_last_error = vformat("DraftStatsDatabase: cannot open %s", path);
		return false;
	}

	if (file->eof_reached()) {
		_last_error = vformat("DraftStatsDatabase: empty CSV %s", path);
		return false;
	}

	std::vector<String> header = _parse_csv_line(file->get_line());
	int64_t champion_col = _column_index(header, "champion");
	int64_t other_col = _column_index(header, "ally");
	if (other_col < 0) {
		other_col = _column_index(header, "opponent");
	}
	int64_t winrate_col = _column_index(header, "winrate");
	int64_t wins_col = _column_index(header, "wins");
	int64_t losses_col = _column_index(header, "losses");

	if (champion_col < 0 || other_col < 0 || winrate_col < 0) {
		_last_error = vformat("DraftStatsDatabase: missing required columns in %s", path);
		return false;
	}

	while (!file->eof_reached()) {
		String line = file->get_line();
		if (line.strip_edges().is_empty()) {
			continue;
		}

		std::vector<String> row = _parse_csv_line(line);
		String champion = _cell_or_empty(row, champion_col);
		String other = _cell_or_empty(row, other_col);
		String winrate_str = _cell_or_empty(row, winrate_col);

		if (champion.is_empty() || other.is_empty() || winrate_str.is_empty()) {
			continue;
		}

		StatRecord record;
		record.raw_winrate = winrate_str.to_float();

		if (wins_col >= 0 && losses_col >= 0) {
			String wins_str = _cell_or_empty(row, wins_col);
			String losses_str = _cell_or_empty(row, losses_col);
			if (!wins_str.is_empty() && !losses_str.is_empty()) {
				record.wins = wins_str.to_int();
				record.losses = losses_str.to_int();
				record.samples = record.wins + record.losses;
			}
		}

		target[champion][other] = record;
	}

	return true;
}

DraftStatValue DraftStatsDatabase::_apply_smoothing(const StatRecord &record, const DraftPredictionConfig &config) const {
	DraftStatValue result;
	result.raw_winrate = record.raw_winrate;
	result.wins = record.wins;
	result.losses = record.losses;
	result.samples = record.samples;
	result.found = true;

	if (record.samples == 0) {
		result.adjusted_winrate = config.prior_winrate;
		result.confidence = 0.0;
		result.centered_value = 0.0;
		return result;
	}

	// Bayesian smoothing
	double prior_winrate = config.prior_winrate;
	double prior_samples = config.prior_samples;
	double confidence_prior = config.confidence_prior_samples;

	result.adjusted_winrate = (record.wins + prior_winrate * prior_samples) / (record.samples + prior_samples);
	result.confidence = record.samples / (record.samples + confidence_prior);
	result.centered_value = (result.adjusted_winrate - 0.50) * result.confidence;

	return result;
}

DraftStatValue DraftStatsDatabase::_create_missing_value() const {
	DraftStatValue result;
	result.raw_winrate = 0.5;
	result.adjusted_winrate = 0.5;
	result.centered_value = 0.0;
	result.wins = 0;
	result.losses = 0;
	result.samples = 0;
	result.confidence = 0.0;
	result.found = false;
	return result;
}

std::vector<String> DraftStatsDatabase::_parse_csv_line(const String &line) {
	std::vector<String> out;
	String cur;
	bool in_quotes = false;

	for (int64_t i = 0; i < line.length(); ++i) {
		String ch = line.substr(i, 1);
		if (in_quotes) {
			if (ch == "\"") {
				if (i + 1 < line.length() && line.substr(i + 1, 1) == "\"") {
					cur += "\"";
					++i;
				} else {
					in_quotes = false;
				}
			} else {
				cur += ch;
			}
		} else {
			if (ch == "\"") {
				in_quotes = true;
			} else if (ch == ",") {
				out.push_back(cur.strip_edges());
				cur = String();
			} else {
				cur += ch;
			}
		}
	}

	out.push_back(cur.strip_edges());
	return out;
}

int64_t DraftStatsDatabase::_column_index(const std::vector<String> &header, const String &name) {
	for (int64_t i = 0; i < static_cast<int64_t>(header.size()); ++i) {
		if (header[static_cast<size_t>(i)] == name) {
			return i;
		}
	}
	return -1;
}

String DraftStatsDatabase::_cell_or_empty(const std::vector<String> &row, int64_t index) {
	if (index < 0 || index >= static_cast<int64_t>(row.size())) {
		return String();
	}
	return row[static_cast<size_t>(index)];
}

std::vector<StringName> DraftStatsDatabase::tags_for(const StringName &champion) const {
	if (_catalog == nullptr) {
		return std::vector<StringName>();
	}
	return catalog::tags_for(*_catalog, champion);
}

bool DraftStatsDatabase::champion_has_tag(const StringName &champion, const StringName &tag) const {
	if (_catalog == nullptr) {
		return false;
	}
	return catalog::champion_has_tag(*_catalog, champion, tag);
}

void DraftStatsDatabase::set_catalog(const catalog::CatalogState *catalog) {
	_catalog = catalog;
}

} // namespace draft_ai
} // namespace sim
