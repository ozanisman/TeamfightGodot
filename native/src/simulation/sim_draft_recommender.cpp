#include "sim_draft_recommender.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <set>

namespace sim {
namespace draft {

namespace {

std::vector<String> parse_csv_line(const String &line) {
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

int64_t column_index(const std::vector<String> &header, const String &name) {
	for (int64_t i = 0; i < static_cast<int64_t>(header.size()); ++i) {
		if (header[static_cast<size_t>(i)] == name) {
			return i;
		}
	}
	return -1;
}

String cell_or_empty(const std::vector<String> &row, int64_t index) {
	if (index < 0 || index >= static_cast<int64_t>(row.size())) {
		return String();
	}
	return row[static_cast<size_t>(index)];
}

bool lookup_pair(const DraftStatsDatabase::MatchupMap &map, const StringName &a, const StringName &b, DraftStatsDatabase::StatValue &out_value) {
	auto it = map.find(a);
	if (it == map.end()) {
		return false;
	}
	auto jt = it->second.find(b);
	if (jt == it->second.end()) {
		return false;
	}
	out_value = jt->second;
	return true;
}

} // namespace

bool DraftStatsDatabase::load_from_dir(const String &dir_path) {
	_base_winrates.clear();
	_synergy_winrates.clear();
	_counter_winrates.clear();
	_composition_winrates.clear();
	_loaded = false;
	_last_error = String();

	String normalized = dir_path.trim_suffix("/");
	if (!_load_combat_stats(normalized + "/combat_stats.csv")) {
		return false;
	}
	if (!_load_counter_stats(normalized + "/matchup_with.csv", _synergy_winrates)) {
		return false;
	}
	if (!_load_counter_stats(normalized + "/matchup_vs.csv", _counter_winrates)) {
		return false;
	}
	_load_composition_stats(normalized + "/hero_combinations.csv"); // Optional, don't fail if missing
	_loaded = true;
	return true;
}

bool DraftStatsDatabase::is_loaded() const {
	return _loaded;
}

String DraftStatsDatabase::last_error() const {
	return _last_error;
}

DraftStatsDatabase::StatValue DraftStatsDatabase::base_winrate_for(const StringName &champion) const {
	auto it = _base_winrates.find(champion);
	if (it == _base_winrates.end()) {
		StatValue fallback;
		fallback.winrate = 0.5;
		fallback.samples = 0;
		return fallback;
	}
	return it->second;
}

bool DraftStatsDatabase::synergy_winrate_for(const StringName &champion, const StringName &ally, StatValue &out_value) const {
	return lookup_pair(_synergy_winrates, champion, ally, out_value) || lookup_pair(_synergy_winrates, ally, champion, out_value);
}

bool DraftStatsDatabase::counter_winrate_for(const StringName &champion, const StringName &enemy, StatValue &out_value) const {
	return lookup_pair(_counter_winrates, champion, enemy, out_value);
}

double DraftStatsDatabase::calculate_team_score(const std::vector<StringName> &team, const std::vector<StringName> &enemies, const PredictionConfig &config, TeamScoreBreakdown &out_breakdown) const {
	if (team.empty()) {
		out_breakdown.base = 0.5;
		out_breakdown.synergy = 0.5;
		out_breakdown.matchup = 0.5;
		out_breakdown.composition = 0.5;
		out_breakdown.final = 0.5;
		return 0.5;
	}

	double total_base = 0.0;
	double total_synergy = 0.0;
	double total_matchup = 0.0;
	double total_score = 0.0;

	for (const StringName &champion : team) {
		// Base component with confidence weighting
		StatValue base_stat = base_winrate_for(champion);
		double base_adjusted = apply_bayesian_smoothing(base_stat.winrate, base_stat.samples, config);
		total_base += base_adjusted;

		// Synergy component with confidence weighting
		double synergy_total = 0.0;
		int64_t synergy_samples = 0;
		for (const StringName &teammate : team) {
			if (champion == teammate) {
				continue;
			}
			StatValue synergy_stat;
			if (synergy_winrate_for(champion, teammate, synergy_stat)) {
				synergy_total += apply_bayesian_smoothing(synergy_stat.winrate, synergy_stat.samples, config);
				++synergy_samples;
			}
		}
		double avg_synergy = synergy_samples > 0 ? synergy_total / double(synergy_samples) : 0.5; // Neutral fallback
		total_synergy += avg_synergy;

		// Matchup component with confidence weighting
		double matchup_total = 0.0;
		int64_t matchup_samples = 0;
		for (const StringName &enemy : enemies) {
			StatValue matchup_stat;
			if (counter_winrate_for(champion, enemy, matchup_stat)) {
				matchup_total += apply_bayesian_smoothing(matchup_stat.winrate, matchup_stat.samples, config);
				++matchup_samples;
			}
		}
		double avg_matchup = matchup_samples > 0 ? matchup_total / double(matchup_samples) : 0.5; // Neutral fallback
		total_matchup += avg_matchup;

		// Combine components
		double champion_score = (config.base_weight * base_adjusted) +
							   (config.synergy_weight * avg_synergy) +
							   (config.matchup_weight * avg_matchup);
		total_score += champion_score;
	}

	out_breakdown.base = total_base / double(team.size());
	out_breakdown.synergy = total_synergy / double(team.size());
	out_breakdown.matchup = total_matchup / double(team.size());

	double avg_team_score = total_score / double(team.size());

	// Add composition bonus if team is complete
	if (team.size() == 5 && config.composition_weight > 0.0) {
		StatValue comp_stat = composition_winrate_for(team);
		double comp_adjusted = apply_bayesian_smoothing(comp_stat.winrate, comp_stat.samples, config);
		out_breakdown.composition = comp_adjusted;
		// Blend composition bonus with team score
		avg_team_score = (avg_team_score * (1.0 - config.composition_weight)) + (comp_adjusted * config.composition_weight);
	} else {
		out_breakdown.composition = 0.5;
	}

	out_breakdown.final = avg_team_score;
	return avg_team_score;
}

void DraftStatsDatabase::calculate_win_probability(const std::vector<StringName> &team1, const std::vector<StringName> &team2, const PredictionConfig &config, double &out_team1_prob, double &out_team2_prob, TeamScoreBreakdown &out_team1_breakdown, TeamScoreBreakdown &out_team2_breakdown) const {
	double score1 = calculate_team_score(team1, team2, config, out_team1_breakdown);
	double score2 = calculate_team_score(team2, team1, config, out_team2_breakdown);

	// Use logistic function to convert score difference to probability
	double score_diff = score1 - score2;
	double k = config.logistic_k;
	double logistic = 1.0 / (1.0 + std::exp(-k * score_diff));

	out_team1_prob = logistic;
	out_team2_prob = 1.0 - logistic;
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
	std::vector<String> header = parse_csv_line(file->get_line());
	int64_t hero_col = column_index(header, "hero");
	int64_t winrate_col = column_index(header, "win_rate");
	int64_t total_games_col = column_index(header, "total_games");
	if (hero_col < 0 || winrate_col < 0) {
		_last_error = vformat("DraftStatsDatabase: missing hero/win_rate columns in %s", path);
		return false;
	}
	while (!file->eof_reached()) {
		String line = file->get_line();
		if (line.strip_edges().is_empty()) {
			continue;
		}
		std::vector<String> row = parse_csv_line(line);
		String hero = cell_or_empty(row, hero_col);
		String winrate = cell_or_empty(row, winrate_col);
		String total_games = total_games_col >= 0 ? cell_or_empty(row, total_games_col) : String();
		if (hero.is_empty() || winrate.is_empty()) {
			continue;
		}
		StatValue value;
		value.winrate = winrate.to_float();
		value.samples = total_games.is_empty() ? 0 : total_games.to_int();
		_base_winrates[StringName(hero)] = value;
	}
	return true;
}

bool DraftStatsDatabase::_load_counter_stats(const String &path, MatchupMap &target) {
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::ModeFlags::READ);
	if (file.is_null()) {
		_last_error = vformat("DraftStatsDatabase: cannot open %s", path);
		return false;
	}
	if (file->eof_reached()) {
		_last_error = vformat("DraftStatsDatabase: empty CSV %s", path);
		return false;
	}
	std::vector<String> header = parse_csv_line(file->get_line());
	int64_t champion_col = column_index(header, "champion");
	int64_t other_col = column_index(header, "ally");
	if (other_col < 0) {
		other_col = column_index(header, "opponent");
	}
	int64_t winrate_col = column_index(header, "winrate");
	int64_t wins_col = column_index(header, "wins");
	int64_t losses_col = column_index(header, "losses");
	if (champion_col < 0 || other_col < 0 || winrate_col < 0) {
		_last_error = vformat("DraftStatsDatabase: missing counter columns in %s", path);
		return false;
	}
	while (!file->eof_reached()) {
		String line = file->get_line();
		if (line.strip_edges().is_empty()) {
			continue;
		}
		std::vector<String> row = parse_csv_line(line);
		String champion = cell_or_empty(row, champion_col);
		String other = cell_or_empty(row, other_col);
		String winrate = cell_or_empty(row, winrate_col);
		String wins = wins_col >= 0 ? cell_or_empty(row, wins_col) : String();
		String losses = losses_col >= 0 ? cell_or_empty(row, losses_col) : String();
		if (champion.is_empty() || other.is_empty() || winrate.is_empty()) {
			continue;
		}
		StatValue value;
		value.winrate = winrate.to_float();
		value.samples = (wins.is_empty() || losses.is_empty()) ? 0 : (wins.to_int() + losses.to_int());
		target[StringName(champion)][StringName(other)] = value;
	}
	return true;
}

bool DraftStatsDatabase::_load_composition_stats(const String &path) {
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::ModeFlags::READ);
	if (file.is_null()) {
		// Composition stats are optional, don't fail if missing
		return true;
	}
	if (file->eof_reached()) {
		return true;
	}
	std::vector<String> header = parse_csv_line(file->get_line());
	int64_t team_size_col = column_index(header, "team_size");
	int64_t combination_col = column_index(header, "combination");
	int64_t winrate_col = column_index(header, "win_rate");
	int64_t total_games_col = column_index(header, "total_games");
	if (team_size_col < 0 || combination_col < 0 || winrate_col < 0) {
		return true;
	}
	while (!file->eof_reached()) {
		String line = file->get_line();
		if (line.strip_edges().is_empty()) {
			continue;
		}
		std::vector<String> row = parse_csv_line(line);
		String team_size = cell_or_empty(row, team_size_col);
		String combination = cell_or_empty(row, combination_col);
		String winrate = cell_or_empty(row, winrate_col);
		String total_games = total_games_col >= 0 ? cell_or_empty(row, total_games_col) : String();
		if (team_size.is_empty() || combination.is_empty() || winrate.is_empty()) {
			continue;
		}
		// Only load 5v5 compositions for now
		if (team_size.to_int() != 5) {
			continue;
		}
		StatValue value;
		value.winrate = winrate.to_float();
		value.samples = total_games.is_empty() ? 0 : total_games.to_int();
		_composition_winrates[combination] = value;
	}
	return true;
}

double DraftStatsDatabase::apply_bayesian_smoothing(double raw_winrate, int64_t samples, const PredictionConfig &config) const {
	if (samples <= 0) {
		return config.prior_winrate;
	}
	double prior = config.prior_winrate;
	int64_t N = config.confidence_prior_samples;
	return (raw_winrate * samples + prior * N) / (samples + N);
}

DraftStatsDatabase::StatValue DraftStatsDatabase::composition_winrate_for(const std::vector<StringName> &team) const {
	if (team.size() != 5) {
		StatValue fallback;
		fallback.winrate = 0.5;
		fallback.samples = 0;
		return fallback;
	}

	// Sort team to create consistent key
	std::vector<String> sorted_team;
	for (const StringName &champion : team) {
		sorted_team.push_back(String(champion));
	}
	std::sort(sorted_team.begin(), sorted_team.end());

	String combination = sorted_team[0];
	for (size_t i = 1; i < sorted_team.size(); ++i) {
		combination += " + " + sorted_team[i];
	}

	auto it = _composition_winrates.find(combination);
	if (it == _composition_winrates.end()) {
		StatValue fallback;
		fallback.winrate = 0.5;
		fallback.samples = 0;
		return fallback;
	}
	return it->second;
}

DraftEvaluator::DraftEvaluator(const DraftStatsDatabase &database, PredictionConfig config) :
		_database(database),
		_config(config) {
}

DraftEvaluation DraftEvaluator::evaluate(const StringName &candidate, const std::vector<StringName> &allies, const std::vector<StringName> &enemies) const {
	DraftEvaluation result;
	result.champion = candidate;

	// Base component with confidence weighting
	DraftStatsDatabase::StatValue base_stat = _database.base_winrate_for(candidate);
	result.base_raw = base_stat.winrate;
	result.base_samples = base_stat.samples;
	result.base_winrate = _database.apply_bayesian_smoothing(base_stat.winrate, base_stat.samples, _config);

	// Synergy component with confidence weighting
	double synergy_total = 0.0;
	double synergy_raw_total = 0.0;
	int64_t synergy_relationships = 0;
	int64_t synergy_stat_samples = 0;
	for (const StringName &ally : allies) {
		DraftStatsDatabase::StatValue synergy_stat;
		if (_database.synergy_winrate_for(candidate, ally, synergy_stat)) {
			synergy_total += _database.apply_bayesian_smoothing(synergy_stat.winrate, synergy_stat.samples, _config);
			synergy_raw_total += synergy_stat.winrate;
			synergy_stat_samples += synergy_stat.samples;
			++synergy_relationships;
		}
	}
	result.avg_synergy = synergy_relationships > 0 ? synergy_total / double(synergy_relationships) : 0.5; // Neutral fallback
	result.synergy_raw = synergy_relationships > 0 ? synergy_raw_total / double(synergy_relationships) : 0.5;
	result.synergy_stat_samples = synergy_stat_samples;
	result.synergy_relationships = synergy_relationships;

	// Counter component with confidence weighting
	double counter_total = 0.0;
	double counter_raw_total = 0.0;
	int64_t counter_relationships = 0;
	int64_t counter_stat_samples = 0;
	for (const StringName &enemy : enemies) {
		DraftStatsDatabase::StatValue counter_stat;
		if (_database.counter_winrate_for(candidate, enemy, counter_stat)) {
			counter_total += _database.apply_bayesian_smoothing(counter_stat.winrate, counter_stat.samples, _config);
			counter_raw_total += counter_stat.winrate;
			counter_stat_samples += counter_stat.samples;
			++counter_relationships;
		}
	}
	result.avg_counter = counter_relationships > 0 ? counter_total / double(counter_relationships) : 0.5; // Neutral fallback
	result.counter_raw = counter_relationships > 0 ? counter_raw_total / double(counter_relationships) : 0.5;
	result.counter_stat_samples = counter_stat_samples;
	result.counter_relationships = counter_relationships;

	result.score = (_config.base_weight * result.base_winrate) +
				  (_config.synergy_weight * result.avg_synergy) +
				  (_config.matchup_weight * result.avg_counter);
	return result;
}

double DraftEvaluator::evaluate_candidate(const StringName &candidate, const std::vector<StringName> &allies, const std::vector<StringName> &enemies, EvalDebug *out_debug) const {
	DraftEvaluation evaluation = evaluate(candidate, allies, enemies);
	if (out_debug != nullptr) {
		out_debug->base = evaluation.base_winrate;
		out_debug->synergy = evaluation.avg_synergy;
		out_debug->counter = evaluation.avg_counter;
		out_debug->final = evaluation.score;
	}
	return evaluation.score;
}

SignalInfluenceReport DraftEvaluator::analyze_signal_influence(const StringName &candidate, const std::vector<StringName> &allies, const std::vector<StringName> &enemies) const {
	SignalInfluenceReport report;
	
	// Base only (synergy_weight=0, matchup_weight=0)
	PredictionConfig base_only_config = _config;
	base_only_config.synergy_weight = 0.0;
	base_only_config.matchup_weight = 0.0;
	DraftEvaluator base_only_eval(_database, base_only_config);
	DraftEvaluation base_only_eval_result = base_only_eval.evaluate(candidate, allies, enemies);
	report.base_only_score = base_only_eval_result.score;
	
	// Synergy removed (synergy_weight=0)
	PredictionConfig no_synergy_config = _config;
	no_synergy_config.synergy_weight = 0.0;
	DraftEvaluator no_synergy_eval(_database, no_synergy_config);
	DraftEvaluation no_synergy_eval_result = no_synergy_eval.evaluate(candidate, allies, enemies);
	report.synergy_removed_score = no_synergy_eval_result.score;
	
	// Matchup removed (matchup_weight=0)
	PredictionConfig no_matchup_config = _config;
	no_matchup_config.matchup_weight = 0.0;
	DraftEvaluator no_matchup_eval(_database, no_matchup_config);
	DraftEvaluation no_matchup_eval_result = no_matchup_eval.evaluate(candidate, allies, enemies);
	report.matchup_removed_score = no_matchup_eval_result.score;
	
	// Full score with all weights
	DraftEvaluation full_eval_result = evaluate(candidate, allies, enemies);
	report.full_score = full_eval_result.score;
	
	// Compute deltas
	report.base_only_delta = report.full_score - report.base_only_score;
	report.synergy_removed_delta = report.full_score - report.synergy_removed_score;
	report.matchup_removed_delta = report.full_score - report.matchup_removed_score;
	
	return report;
}

ControlledEvaluationReport DraftEvaluator::run_controlled_evaluation(const std::vector<StringName> &allies, const std::vector<StringName> &enemies, const std::vector<StringName> &available) const {
	ControlledEvaluationReport report;
	
	if (available.empty()) {
		return report;
	}
	
	// Full model evaluation
	DraftRecommender full_recommender(*this, false);
	std::vector<DraftEvaluation> full_ranked = full_recommender.recommend(allies, enemies, available);
	
	// Synergy removed evaluation
	PredictionConfig no_synergy_config = _config;
	no_synergy_config.synergy_weight = 0.0;
	DraftEvaluator no_synergy_eval(_database, no_synergy_config);
	DraftRecommender no_synergy_recommender(no_synergy_eval, false);
	std::vector<DraftEvaluation> no_synergy_ranked = no_synergy_recommender.recommend(allies, enemies, available);
	
	// Matchup removed evaluation
	PredictionConfig no_matchup_config = _config;
	no_matchup_config.matchup_weight = 0.0;
	DraftEvaluator no_matchup_eval(_database, no_matchup_config);
	DraftRecommender no_matchup_recommender(no_matchup_eval, false);
	std::vector<DraftEvaluation> no_matchup_ranked = no_matchup_recommender.recommend(allies, enemies, available);
	
	// Compute average impact scores
	double total_synergy_impact = 0.0;
	double total_matchup_impact = 0.0;
	
	for (size_t i = 0; i < available.size(); ++i) {
		const StringName &candidate = available[i];
		
		// Find full model score for this candidate
		auto full_it = std::find_if(full_ranked.begin(), full_ranked.end(), 
			[&candidate](const DraftEvaluation &e) { return e.champion == candidate; });
		double full_score = (full_it != full_ranked.end()) ? full_it->score : 0.5;
		
		// Find synergy-removed score
		auto no_synergy_it = std::find_if(no_synergy_ranked.begin(), no_synergy_ranked.end(),
			[&candidate](const DraftEvaluation &e) { return e.champion == candidate; });
		double no_synergy_score = (no_synergy_it != no_synergy_ranked.end()) ? no_synergy_it->score : 0.5;
		
		// Find matchup-removed score
		auto no_matchup_it = std::find_if(no_matchup_ranked.begin(), no_matchup_ranked.end(),
			[&candidate](const DraftEvaluation &e) { return e.champion == candidate; });
		double no_matchup_score = (no_matchup_it != no_matchup_ranked.end()) ? no_matchup_it->score : 0.5;
		
		total_synergy_impact += (full_score - no_synergy_score);
		total_matchup_impact += (full_score - no_matchup_score);
	}
	
	report.avg_synergy_impact = total_synergy_impact / double(available.size());
	report.avg_matchup_impact = total_matchup_impact / double(available.size());
	
	// Compute top-3 overlap
	int64_t top3_count = std::min<int64_t>(3, static_cast<int64_t>(full_ranked.size()));
	
	// Top-3 overlap vs synergy removed
	std::set<StringName> full_top3;
	for (int64_t i = 0; i < top3_count; ++i) {
		full_top3.insert(full_ranked[static_cast<size_t>(i)].champion);
	}
	
	std::set<StringName> no_synergy_top3;
	for (int64_t i = 0; i < top3_count; ++i) {
		no_synergy_top3.insert(no_synergy_ranked[static_cast<size_t>(i)].champion);
	}
	
	std::set<StringName> no_matchup_top3;
	for (int64_t i = 0; i < top3_count; ++i) {
		no_matchup_top3.insert(no_matchup_ranked[static_cast<size_t>(i)].champion);
	}
	
	// Count intersections
	for (const StringName &champ : full_top3) {
		if (no_synergy_top3.find(champ) != no_synergy_top3.end()) {
			++report.top3_overlap_synergy_removed;
		}
		if (no_matchup_top3.find(champ) != no_matchup_top3.end()) {
			++report.top3_overlap_matchup_removed;
		}
	}
	
	return report;
}

DraftRecommender::DraftRecommender(const DraftEvaluator &evaluator, bool debug_mode) :
		_evaluator(evaluator),
		_debug_mode(debug_mode) {
}

void DraftRecommender::set_debug_mode(bool enabled) {
	_debug_mode = enabled;
}

std::vector<DraftEvaluation> DraftRecommender::recommend(const std::vector<StringName> &allies, const std::vector<StringName> &enemies, const std::vector<StringName> &available) const {
	std::vector<DraftEvaluation> ranked;
	ranked.reserve(available.size());
	for (const StringName &candidate : available) {
		ranked.push_back(_evaluator.evaluate(candidate, allies, enemies));
	}
	std::sort(ranked.begin(), ranked.end(), [](const DraftEvaluation &a, const DraftEvaluation &b) {
		if (a.score == b.score) {
			return String(a.champion) < String(b.champion);
		}
		return a.score > b.score;
	});
	return ranked;
}

void DraftRecommender::print_top(const std::vector<DraftEvaluation> &ranked, int64_t top_n) const {
	int64_t limit = top_n > 0 ? std::min<int64_t>(top_n, static_cast<int64_t>(ranked.size())) : static_cast<int64_t>(ranked.size());
	UtilityFunctions::print(vformat("Draft recommendations (top %d of %d)", limit, ranked.size()));
	for (int64_t i = 0; i < limit; ++i) {
		const DraftEvaluation &r = ranked[static_cast<size_t>(i)];
		if (_debug_mode) {
			UtilityFunctions::print(vformat("%d. %s", i + 1, String(r.champion)));
			UtilityFunctions::print(vformat(""));
			UtilityFunctions::print(vformat("base:"));
			UtilityFunctions::print(vformat("  raw=%.6f", r.base_raw));
			UtilityFunctions::print(vformat("  stat_samples=%d", r.base_samples));
			UtilityFunctions::print(vformat("  adjusted=%.6f", r.base_winrate));
			UtilityFunctions::print(vformat(""));
			UtilityFunctions::print(vformat("synergy:"));
			UtilityFunctions::print(vformat("  raw=%.6f", r.synergy_raw));
			UtilityFunctions::print(vformat("  relationships=%d", r.synergy_relationships));
			UtilityFunctions::print(vformat("  stat_samples=%d", r.synergy_stat_samples));
			UtilityFunctions::print(vformat("  adjusted=%.6f", r.avg_synergy));
			UtilityFunctions::print(vformat(""));
			UtilityFunctions::print(vformat("counter:"));
			UtilityFunctions::print(vformat("  raw=%.6f", r.counter_raw));
			UtilityFunctions::print(vformat("  relationships=%d", r.counter_relationships));
			UtilityFunctions::print(vformat("  stat_samples=%d", r.counter_stat_samples));
			UtilityFunctions::print(vformat("  adjusted=%.6f", r.avg_counter));
			UtilityFunctions::print(vformat(""));
			UtilityFunctions::print(vformat("final=%.6f", r.score));
		} else {
			UtilityFunctions::print(vformat(
					"%d. %s score=%.6f",
					i + 1,
					String(r.champion),
					r.score));
		}
	}
}

void DraftRecommender::run_debug_evaluation_batch(const std::vector<StringName> &allies, const std::vector<StringName> &enemies, const std::vector<StringName> &available, int64_t num_runs) const {
	if (available.empty()) {
		UtilityFunctions::print("Draft debug evaluation batch: no available champions");
		return;
	}

	int64_t runs = num_runs > 0 ? num_runs : 1;
	for (int64_t run = 0; run < runs; ++run) {
		std::vector<StringName> batch_allies = allies;
		std::vector<StringName> batch_enemies = enemies;
		if (!available.empty()) {
			if (batch_allies.empty()) {
				batch_allies.push_back(available[static_cast<size_t>(run % static_cast<int64_t>(available.size()))]);
			}
			if (batch_enemies.empty()) {
				batch_enemies.push_back(available[static_cast<size_t>((run + 1) % static_cast<int64_t>(available.size()))]);
			}
		}

		UtilityFunctions::print(vformat("Draft debug evaluation state %d/%d", run + 1, runs));
		std::vector<DraftEvaluation> ranked = recommend(batch_allies, batch_enemies, available);
		DraftRecommender debug_recommender(_evaluator, true);
		debug_recommender.print_top(ranked, static_cast<int64_t>(ranked.size()));
	}
}

} // namespace draft
} // namespace sim
