#include "sim_draft_recommender.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

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

enum class RelationshipKind {
	SYNERGY,
	COUNTER
};

struct RelationshipAggregate {
	double adjusted_average = 0.5; // flat average of smoothed winrates (neutral fallback)
	double raw_average = 0.5; // flat average of raw winrates — kept for diagnostics/debug output
	double variance = 0.0; // population variance of smoothed winrates across the relationship set
	int64_t relationships = 0;
	int64_t stat_samples = 0;
};

// Aggregates a champion's pairwise winrates against a set of others (allies for synergy, enemies
// for counter/matchup) into a flat average of their (Bayesian-smoothed) values, falling back to
// the neutral 0.5 default when nothing resolves. Shared by DraftEvaluator::evaluate (per-candidate
// recommendation scoring) and DraftStatsDatabase::calculate_team_score (whole-team win-probability
// scoring) so the two scorers cannot silently disagree about how relationship signals aggregate.
RelationshipAggregate aggregate_relationship_signal(const DraftStatsDatabase &database, const StringName &champion, const std::vector<StringName> &others, RelationshipKind kind, const PredictionConfig &config) {
	RelationshipAggregate result;

	double adjusted_sum = 0.0;
	double adjusted_sum_sq = 0.0;
	double raw_sum = 0.0;

	for (const StringName &other : others) {
		if (other == champion) {
			continue;
		}
		DraftStatsDatabase::StatValue stat;
		bool found = (kind == RelationshipKind::SYNERGY)
				? database.synergy_winrate_for(champion, other, stat)
				: database.counter_winrate_for(champion, other, stat);
		if (!found) {
			continue;
		}
		double smoothed = database.apply_bayesian_smoothing(stat.winrate, stat.samples, config);
		adjusted_sum += smoothed;
		adjusted_sum_sq += smoothed * smoothed;
		raw_sum += stat.winrate;
		result.stat_samples += stat.samples;
		++result.relationships;
	}

	if (result.relationships == 0) {
		return result; // neutral 0.5 defaults
	}

	result.raw_average = raw_sum / double(result.relationships);
	result.adjusted_average = adjusted_sum / double(result.relationships);
	// Population variance: E[x²] - E[x]²
	result.variance = (adjusted_sum_sq / double(result.relationships)) - (result.adjusted_average * result.adjusted_average);

	return result;
}

// Combines a set of base/synergy/matchup signals (plus an optional composition bonus) into a
// single score according to config.scoring_mode and its associated sharpening/interaction knobs.
// Shared by DraftEvaluator::evaluate (per-candidate recommendation scoring) and
// DraftStatsDatabase::calculate_team_score (whole-team win-probability scoring) so the two
// scorers cannot silently disagree about what "scoring_mode" means.
double score_from_signals(double base_winrate, double avg_synergy, double avg_counter, double composition_bonus, const PredictionConfig &config) {
	double effective_synergy = avg_synergy * config.synergy_amplification;
	double effective_matchup = avg_counter * config.matchup_amplification;

	double score = 0.0;
	switch (config.scoring_mode) {
		case ScoringMode::MULTIPLICATIVE: {
			// Multiplicative model: product of components
			score = base_winrate * effective_synergy * effective_matchup;
			// Normalize by expected max (1.0 * 1.0 * 1.0 = 1.0)
			score = std::clamp(score, 0.0, 1.0);
			// No interaction term in multiplicative mode
			break;
		}
		case ScoringMode::LOGIT: {
			// Logit-space model
			auto logit_transform = [](double p) -> double {
				p = std::clamp(p, 1e-6, 1.0 - 1e-6);
				return std::log(p / (1.0 - p));
			};
			auto sigmoid = [](double x) -> double {
				return 1.0 / (1.0 + std::exp(-x));
			};

			double logit_base = logit_transform(base_winrate);
			double logit_synergy = logit_transform(avg_synergy) + std::log(config.synergy_amplification);
			double logit_matchup = logit_transform(avg_counter) + std::log(config.matchup_amplification);

			double combined = config.logit_sharpness * (
				config.base_weight * logit_base +
				config.synergy_weight * logit_synergy +
				config.matchup_weight * logit_matchup
			);

			score = sigmoid(combined);
			// Add interaction term (only in logit mode)
			score += config.interaction_weight * (effective_synergy * effective_matchup);
			score = std::clamp(score, 0.0, 1.0);
			break;
		}
		case ScoringMode::ADDITIVE:
		default: {
			// Additive model (default)
			score = (config.base_weight * base_winrate) +
					(config.synergy_weight * effective_synergy) +
					(config.matchup_weight * effective_matchup) +
					composition_bonus;
			// Add interaction term (only in additive mode)
			score += config.interaction_weight * (effective_synergy * effective_matchup);
			// Apply score sharpening
			score = std::pow(score, config.score_sharpness);
			break;
		}
	}
	return score;
}

} // namespace

bool DraftStatsDatabase::load_from_dir(const String &dir_path) {
	_base_winrates.clear();
	_synergy_winrates.clear();
	_counter_winrates.clear();
	_composition_winrates.clear();
	_champion_roles.clear();
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
	_load_composition_stats(normalized + "/role_combinations.csv"); // Optional, don't fail if missing
	_load_champion_roles();
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

		// Synergy component: flat average vs. teammates (see aggregate_relationship_signal).
		RelationshipAggregate synergy_agg = aggregate_relationship_signal(*this, champion, team, RelationshipKind::SYNERGY, config);
		double avg_synergy = synergy_agg.adjusted_average;
		total_synergy += avg_synergy;

		// Matchup component: flat average vs. enemies (see aggregate_relationship_signal).
		double avg_matchup = aggregate_relationship_signal(*this, champion, enemies, RelationshipKind::COUNTER, config).adjusted_average;
		total_matchup += avg_matchup;

		// Combine components according to scoring_mode (no per-champion composition heuristic
		// here — composition is handled below as a whole-team blend via composition_winrate_for).
		double champion_score = score_from_signals(base_adjusted, avg_synergy, avg_matchup, 0.0, config);
		if (config.variance_weight != 0.0) {
			champion_score += config.variance_weight * synergy_agg.variance;
			champion_score = std::clamp(champion_score, 0.0, 1.0);
		}
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
	int64_t role_col = column_index(header, "role");
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
		String role = role_col >= 0 ? cell_or_empty(row, role_col) : String();
		if (hero.is_empty() || winrate.is_empty()) {
			continue;
		}
		StatValue value;
		value.winrate = winrate.to_float();
		value.samples = total_games.is_empty() ? 0 : total_games.to_int();
		_base_winrates[StringName(hero)] = value;
		
		// Store role if available
		if (!role.is_empty()) {
			_champion_roles[StringName(hero)] = StringName(role);
		}
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
	int64_t role_fingerprint_col = column_index(header, "role_fingerprint");
	int64_t winrate_col = column_index(header, "win_rate");
	int64_t total_games_col = column_index(header, "total_games");
	if (team_size_col < 0 || role_fingerprint_col < 0 || winrate_col < 0) {
		return true;
	}
	while (!file->eof_reached()) {
		String line = file->get_line();
		if (line.strip_edges().is_empty()) {
			continue;
		}
		std::vector<String> row = parse_csv_line(line);
		String team_size = cell_or_empty(row, team_size_col);
		String role_fingerprint = cell_or_empty(row, role_fingerprint_col);
		String winrate = cell_or_empty(row, winrate_col);
		String total_games = total_games_col >= 0 ? cell_or_empty(row, total_games_col) : String();
		if (team_size.is_empty() || role_fingerprint.is_empty() || winrate.is_empty()) {
			continue;
		}
		// Only load 5v5 compositions for now
		if (team_size.to_int() != 5) {
			continue;
		}
		// Use role_fingerprint directly as key
		StatValue value;
		value.winrate = winrate.to_float();
		value.samples = total_games.is_empty() ? 0 : total_games.to_int();
		_composition_winrates[role_fingerprint] = value;
	}
	return true;
}

void DraftStatsDatabase::_load_champion_roles() {
	// Roles are already loaded from combat_stats.csv
	// This is a placeholder for future role-specific loading if needed
}

StringName DraftStatsDatabase::get_champion_role(const StringName &champion) const {
	auto it = _champion_roles.find(champion);
	if (it != _champion_roles.end()) {
		return it->second;
	}
	return StringName();  // Empty if not found
}

double DraftStatsDatabase::apply_bayesian_smoothing(double raw_winrate, int64_t samples, const PredictionConfig &config) const {
	if (samples <= 0) {
		return config.prior_winrate;
	}
	
	double adjusted;
	
	if (config.smoothing_mode == SmoothingMode::CONFIDENCE_WEIGHTED) {
		// Confidence-weighted smoothing
		double w = std::clamp(double(samples) / (double(samples) + config.smoothing_k), 0.0, 1.0);
		adjusted = w * raw_winrate + (1.0 - w) * config.prior_winrate;
	} else {
		// Legacy Bayesian smoothing
		double prior = config.prior_winrate;
		int64_t N = config.confidence_prior_samples;
		adjusted = (raw_winrate * samples + prior * N) / (samples + N);
		
		// Apply threshold-based smoothing strength adjustment
		if (samples < config.smoothing_threshold_samples) {
			adjusted = config.smoothing_strength * adjusted +
			           (1.0f - config.smoothing_strength) * 0.5f;
		}
	}
	
	return adjusted;
}

DraftStatsDatabase::StatValue DraftStatsDatabase::composition_winrate_for(const std::vector<StringName> &team) const {
	if (team.size() != 5) {
		UtilityFunctions::print(vformat("DraftStatsDatabase: composition_winrate_for fallback - team size: %d", team.size()));
		StatValue fallback;
		fallback.winrate = 0.5;
		fallback.samples = 0;
		return fallback;
	}

	// Build role fingerprint for team
	std::vector<String> team_roles;
	for (const StringName &champion : team) {
		StringName role = get_champion_role(champion);
		team_roles.push_back(role != StringName() ? String(role) : String("unknown"));
	}
	std::sort(team_roles.begin(), team_roles.end());
	String team_role_fp = team_roles[0];
	for (size_t i = 1; i < team_roles.size(); ++i) {
		team_role_fp += " + " + team_roles[i];
	}

	auto it = _composition_winrates.find(team_role_fp);
	if (it == _composition_winrates.end()) {
		UtilityFunctions::print(vformat("DraftStatsDatabase: composition_winrate_for key not found - key: %s", team_role_fp));
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

	// Synergy component: flat average vs. allies (see aggregate_relationship_signal).
	RelationshipAggregate synergy_agg = aggregate_relationship_signal(_database, candidate, allies, RelationshipKind::SYNERGY, _config);
	result.avg_synergy = synergy_agg.adjusted_average;
	result.synergy_raw = synergy_agg.raw_average;
	result.synergy_variance = synergy_agg.variance;
	result.synergy_stat_samples = synergy_agg.stat_samples;
	result.synergy_relationships = synergy_agg.relationships;

	// Counter component: flat average vs. enemies (see aggregate_relationship_signal).
	RelationshipAggregate counter_agg = aggregate_relationship_signal(_database, candidate, enemies, RelationshipKind::COUNTER, _config);
	result.avg_counter = counter_agg.adjusted_average;
	result.counter_raw = counter_agg.raw_average;
	result.counter_variance = counter_agg.variance;
	result.counter_stat_samples = counter_agg.stat_samples;
	result.counter_relationships = counter_agg.relationships;

	// Calculate composition bonus
	double composition_bonus = calculate_composition_bonus(allies, candidate);

	result.score = score_from_signals(result.base_winrate, result.avg_synergy, result.avg_counter, composition_bonus, _config);
	if (_config.variance_weight != 0.0) {
		result.score += _config.variance_weight * result.synergy_variance;
		result.score = std::clamp(result.score, 0.0, 1.0);
	}

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

double DraftEvaluator::calculate_composition_bonus(const std::vector<StringName> &team, const StringName &candidate) const {
	// Build full team including candidate
	std::vector<StringName> full_team = team;
	full_team.push_back(candidate);
	
	// Count roles
	std::map<StringName, int64_t> role_counts;
	for (const StringName &champ : full_team) {
		StringName role = _database.get_champion_role(champ);
		if (role != StringName()) {
			role_counts[role]++;
		}
	}
	
	// Check for balance
	int64_t max_count = 0;
	for (const auto &pair : role_counts) {
		max_count = std::max(max_count, pair.second);
	}
	
	// Apply bonus/penalty based on balance
	if (max_count <= 2) {
		// Balanced: bonus
		return _config.composition_balance_weight;
	} else if (max_count == 3) {
		// Slightly unbalanced: neutral
		return 0.0;
	} else {
		// Severely unbalanced: penalty
		return -_config.composition_unbalance_penalty;
	}
}

StressTestReport DraftEvaluator::run_stress_test(const std::vector<StringName> &allies, const std::vector<StringName> &enemies, const std::vector<StringName> &available, int64_t num_iterations) const {
	StressTestReport report;
	
	// Compute baseline ranking (deterministic)
	std::vector<DraftEvaluation> baseline_ranked;
	baseline_ranked.reserve(available.size());
	for (const StringName &candidate : available) {
		baseline_ranked.push_back(evaluate(candidate, allies, enemies));
	}
	std::sort(baseline_ranked.begin(), baseline_ranked.end(), [](const DraftEvaluation &a, const DraftEvaluation &b) {
		if (a.score == b.score) {
			return String(a.champion) < String(b.champion);
		}
		return a.score > b.score;
	});
	
	// Store baseline top-3 and top-1
	for (int64_t i = 0; i < std::min(int64_t(3), static_cast<int64_t>(baseline_ranked.size())); ++i) {
		report.baseline_top3.push_back(baseline_ranked[i].champion);
	}
	if (!baseline_ranked.empty()) {
		report.baseline_top1 = baseline_ranked[0].champion;
	}
	
	// Create champion -> baseline rank map
	std::map<StringName, int64_t> baseline_ranks;
	for (int64_t i = 0; i < static_cast<int64_t>(baseline_ranked.size()); ++i) {
		baseline_ranks[baseline_ranked[i].champion] = i;
	}
	
	// Accumulate scores and ranks across iterations
	std::map<StringName, std::vector<double>> score_history;
	std::map<StringName, std::vector<int64_t>> rank_history;
	std::vector<double> all_scores;
	all_scores.reserve(available.size() * num_iterations);
	
	int64_t top1_stable_count = 0;
	int64_t top3_stable_count = 0;
	
	for (int64_t iter = 0; iter < num_iterations; ++iter) {
		// For stress testing, we evaluate the same deterministic scenario multiple times
		// The "randomness" comes from the test harness varying allies/enemies/available
		// Here we just accumulate statistics for the given fixed scenario
		std::vector<DraftEvaluation> ranked;
		ranked.reserve(available.size());
		for (const StringName &candidate : available) {
			ranked.push_back(evaluate(candidate, allies, enemies));
		}
		std::sort(ranked.begin(), ranked.end(), [](const DraftEvaluation &a, const DraftEvaluation &b) {
			if (a.score == b.score) {
				return String(a.champion) < String(b.champion);
			}
			return a.score > b.score;
		});
		
		// Check winner stability
		if (!ranked.empty() && ranked[0].champion == report.baseline_top1) {
			++top1_stable_count;
		}
		
		std::set<StringName> current_top3;
		for (int64_t i = 0; i < std::min(int64_t(3), static_cast<int64_t>(ranked.size())); ++i) {
			current_top3.insert(ranked[i].champion);
		}
		std::set<StringName> baseline_top3_set(report.baseline_top3.begin(), report.baseline_top3.end());
		if (current_top3 == baseline_top3_set) {
			++top3_stable_count;
		}
		
		// Accumulate scores and ranks
		for (int64_t i = 0; i < static_cast<int64_t>(ranked.size()); ++i) {
			score_history[ranked[i].champion].push_back(ranked[i].score);
			rank_history[ranked[i].champion].push_back(i);
			all_scores.push_back(ranked[i].score);
		}
	}
	
	// Compute per-candidate statistics
	report.candidate_stats.reserve(available.size());
	for (const StringName &candidate : available) {
		CandidateStressStats stats;
		stats.champion = candidate;
		
		auto score_it = score_history.find(candidate);
		auto rank_it = rank_history.find(candidate);
		
		if (score_it != score_history.end() && !score_it->second.empty()) {
			// Mean score
			double sum = 0.0;
			for (double s : score_it->second) {
				sum += s;
			}
			stats.mean_score = sum / score_it->second.size();
			
			// Score stddev
			double variance = 0.0;
			for (double s : score_it->second) {
				variance += (s - stats.mean_score) * (s - stats.mean_score);
			}
			stats.score_stddev = std::sqrt(variance / score_it->second.size());
		}
		
		if (rank_it != rank_history.end() && !rank_it->second.empty()) {
			// Mean rank
			double sum = 0.0;
			for (int64_t r : rank_it->second) {
				sum += r;
			}
			stats.mean_rank = sum / rank_it->second.size();
			
			// Rank stddev
			double variance = 0.0;
			for (int64_t r : rank_it->second) {
				variance += (r - stats.mean_rank) * (r - stats.mean_rank);
			}
			stats.rank_stddev = std::sqrt(variance / rank_it->second.size());
			
			// Max rank swing
			int64_t min_rank = *std::min_element(rank_it->second.begin(), rank_it->second.end());
			int64_t max_rank = *std::max_element(rank_it->second.begin(), rank_it->second.end());
			stats.max_rank_swing = max_rank - min_rank;
		}
		
		// Baseline reference
		auto baseline_rank_it = baseline_ranks.find(candidate);
		if (baseline_rank_it != baseline_ranks.end()) {
			stats.baseline_rank = baseline_rank_it->second;
			stats.baseline_score = baseline_ranked[stats.baseline_rank].score;
		}
		
		report.candidate_stats.push_back(stats);
	}
	
	// Compute global distribution metrics
	if (!all_scores.empty()) {
		std::sort(all_scores.begin(), all_scores.end());
		report.score_min = all_scores.front();
		report.score_max = all_scores.back();
		
		double sum = 0.0;
		for (double s : all_scores) {
			sum += s;
		}
		report.score_mean = sum / all_scores.size();
		
		// Percentiles
		size_t p50_idx = all_scores.size() * 50 / 100;
		size_t p90_idx = all_scores.size() * 90 / 100;
		report.score_p50 = all_scores[p50_idx];
		report.score_p90 = all_scores[p90_idx];
	}
	
	// Compute rank volatility (average rank change per candidate)
	double total_rank_change = 0.0;
	int64_t total_max_swing = 0;
	for (const auto &stats : report.candidate_stats) {
		total_rank_change += std::abs(stats.mean_rank - stats.baseline_rank);
		total_max_swing = std::max(total_max_swing, stats.max_rank_swing);
	}
	report.avg_rank_change = total_rank_change / report.candidate_stats.size();
	report.max_rank_swing = total_max_swing;
	
	// Compute context sensitivity (avg absolute delta from baseline)
	double total_score_delta = 0.0;
	for (const auto &stats : report.candidate_stats) {
		total_score_delta += std::abs(stats.mean_score - stats.baseline_score);
	}
	report.context_sensitivity = total_score_delta / report.candidate_stats.size();
	
	// Compute winner stability rates
	report.top1_stability_rate = num_iterations > 0 ? (100.0 * top1_stable_count / num_iterations) : 0.0;
	report.top3_stability_rate = num_iterations > 0 ? (100.0 * top3_stable_count / num_iterations) : 0.0;
	
	return report;
}

// ScenarioPerturbationGenerator implementation

ScenarioPerturbationGenerator::ScenarioPerturbationGenerator(const DraftStatsDatabase &database) :
		_database(database),
		_rng_state(0) {
}

void ScenarioPerturbationGenerator::_seed_rng(uint64_t seed) {
	_rng_state = seed;
}

uint64_t ScenarioPerturbationGenerator::_rng_next() {
	// Simple LCG (Linear Congruential Generator)
	// Using constants from Numerical Recipes
	_rng_state = _rng_state * 1664525 + 1013904223;
	return _rng_state;
}

double ScenarioPerturbationGenerator::_rng_next_double() {
	return static_cast<double>(_rng_next()) / static_cast<double>(UINT64_MAX);
}

int64_t ScenarioPerturbationGenerator::_rng_next_range(int64_t min, int64_t max) {
	if (min >= max) {
		return min;
	}
	uint64_t range = static_cast<uint64_t>(max - min);
	return min + static_cast<int64_t>(_rng_next() % (range + 1));
}

PerturbedScenario ScenarioPerturbationGenerator::_swap_ally(
		const std::vector<StringName> &base_allies,
		const std::vector<StringName> &base_enemies,
		const std::vector<StringName> &base_available) {
	
	PerturbedScenario scenario;
	scenario.type = PerturbationType::SWAP_ALLY;
	scenario.enemies = base_enemies;
	scenario.available = base_available;
	
	if (base_allies.empty()) {
		// If no allies, add one random champion
		if (!base_available.empty()) {
			int64_t idx = _rng_next_range(0, base_available.size() - 1);
			scenario.allies.push_back(base_available[idx]);
		} else {
			scenario.allies = base_allies;
		}
	} else {
		// Swap 1-2 random allies
		scenario.allies = base_allies;
		int64_t num_swaps = _rng_next_range(1, std::min(int64_t(2), static_cast<int64_t>(base_allies.size())));
		
		// Build pool of candidates to swap in (exclude current allies and enemies)
		std::vector<StringName> swap_pool;
		std::set<StringName> excluded(base_allies.begin(), base_allies.end());
		excluded.insert(base_enemies.begin(), base_enemies.end());
		
		for (const StringName &champ : base_available) {
			if (excluded.find(champ) == excluded.end()) {
				swap_pool.push_back(champ);
			}
		}
		
		for (int64_t i = 0; i < num_swaps && !swap_pool.empty(); ++i) {
			int64_t ally_idx = _rng_next_range(0, scenario.allies.size() - 1);
			int64_t swap_idx = _rng_next_range(0, swap_pool.size() - 1);
			scenario.allies[ally_idx] = swap_pool[swap_idx];
			swap_pool.erase(swap_pool.begin() + swap_idx);
		}
	}
	
	return scenario;
}

PerturbedScenario ScenarioPerturbationGenerator::_swap_enemy(
		const std::vector<StringName> &base_allies,
		const std::vector<StringName> &base_enemies,
		const std::vector<StringName> &base_available) {
	
	PerturbedScenario scenario;
	scenario.type = PerturbationType::SWAP_ENEMY;
	scenario.allies = base_allies;
	scenario.available = base_available;
	
	if (base_enemies.empty()) {
		// If no enemies, add one random champion
		if (!base_available.empty()) {
			int64_t idx = _rng_next_range(0, base_available.size() - 1);
			scenario.enemies.push_back(base_available[idx]);
		} else {
			scenario.enemies = base_enemies;
		}
	} else {
		// Swap 1-2 random enemies
		scenario.enemies = base_enemies;
		int64_t num_swaps = _rng_next_range(1, std::min(int64_t(2), static_cast<int64_t>(base_enemies.size())));
		
		// Build pool of candidates to swap in (exclude current allies and enemies)
		std::vector<StringName> swap_pool;
		std::set<StringName> excluded(base_allies.begin(), base_allies.end());
		excluded.insert(base_enemies.begin(), base_enemies.end());
		
		for (const StringName &champ : base_available) {
			if (excluded.find(champ) == excluded.end()) {
				swap_pool.push_back(champ);
			}
		}
		
		for (int64_t i = 0; i < num_swaps && !swap_pool.empty(); ++i) {
			int64_t enemy_idx = _rng_next_range(0, scenario.enemies.size() - 1);
			int64_t swap_idx = _rng_next_range(0, swap_pool.size() - 1);
			scenario.enemies[enemy_idx] = swap_pool[swap_idx];
			swap_pool.erase(swap_pool.begin() + swap_idx);
		}
	}
	
	return scenario;
}

PerturbedScenario ScenarioPerturbationGenerator::_remove_available(
		const std::vector<StringName> &base_allies,
		const std::vector<StringName> &base_enemies,
		const std::vector<StringName> &base_available) {
	
	PerturbedScenario scenario;
	scenario.type = PerturbationType::REMOVE_AVAILABLE;
	scenario.allies = base_allies;
	scenario.enemies = base_enemies;
	
	if (base_available.size() <= 1) {
		scenario.available = base_available;
	} else {
		// Remove 1-2 random champions from available pool
		scenario.available = base_available;
		int64_t num_remove = _rng_next_range(1, std::min(int64_t(2), static_cast<int64_t>(base_available.size()) - 1));
		
		for (int64_t i = 0; i < num_remove && !scenario.available.empty(); ++i) {
			int64_t remove_idx = _rng_next_range(0, scenario.available.size() - 1);
			scenario.available.erase(scenario.available.begin() + remove_idx);
		}
	}
	
	return scenario;
}

PerturbedScenario ScenarioPerturbationGenerator::_role_skew(
		const std::vector<StringName> &base_allies,
		const std::vector<StringName> &base_enemies,
		const std::vector<StringName> &base_available) {
	
	PerturbedScenario scenario;
	scenario.type = PerturbationType::ROLE_SKEW;
	scenario.allies = base_allies;
	scenario.enemies = base_enemies;
	
	// Collect all unique roles
	std::set<StringName> all_roles;
	for (const StringName &champ : base_available) {
		StringName role = _database.get_champion_role(champ);
		if (role != StringName()) {
			all_roles.insert(role);
		}
	}
	
	if (all_roles.empty()) {
		// No role data available, return unchanged
		scenario.available = base_available;
		return scenario;
	}
	
	// Pick a random role to skew toward
	std::vector<StringName> role_vector(all_roles.begin(), all_roles.end());
	int64_t role_idx = _rng_next_range(0, role_vector.size() - 1);
	StringName target_role = role_vector[role_idx];
	
	// Filter available pool with bias toward target role (70-100%)
	scenario.available = base_available;
	std::vector<StringName> filtered;
	
	for (const StringName &champ : base_available) {
		StringName role = _database.get_champion_role(champ);
		double keep_prob = (role == target_role) ? 0.9 : 0.3;  // 90% keep if target role, 30% otherwise
		
		if (_rng_next_double() < keep_prob) {
			filtered.push_back(champ);
		}
	}
	
	// Ensure at least some champions remain
	if (!filtered.empty()) {
		scenario.available = filtered;
	}
	
	return scenario;
}

std::vector<PerturbedScenario> ScenarioPerturbationGenerator::generate_scenarios(
		const std::vector<StringName> &base_allies,
		const std::vector<StringName> &base_enemies,
		const std::vector<StringName> &base_available,
		uint64_t seed,
		int64_t count) {
	
	std::vector<PerturbedScenario> scenarios;
	scenarios.reserve(count);
	
	_seed_rng(seed);
	
	for (int64_t i = 0; i < count; ++i) {
		// Use seed + scenario_index for determinism
		_seed_rng(seed + static_cast<uint64_t>(i));
		
		// Cycle through perturbation types
		int64_t perturbation_type = i % 4;
		
		switch (perturbation_type) {
			case 0:
				scenarios.push_back(_swap_ally(base_allies, base_enemies, base_available));
				break;
			case 1:
				scenarios.push_back(_swap_enemy(base_allies, base_enemies, base_available));
				break;
			case 2:
				scenarios.push_back(_remove_available(base_allies, base_enemies, base_available));
				break;
			case 3:
				scenarios.push_back(_role_skew(base_allies, base_enemies, base_available));
				break;
		}
	}
	
	return scenarios;
}

StressTestReport DraftEvaluator::run_stress_test_with_perturbations(
		const std::vector<StringName> &allies,
		const std::vector<StringName> &enemies,
		const std::vector<StringName> &available,
		uint64_t seed,
		int64_t scenario_count) const {
	
	StressTestReport report;
	
	// Compute baseline ranking (original scenario)
	std::vector<DraftEvaluation> baseline_ranked;
	baseline_ranked.reserve(available.size());
	for (const StringName &candidate : available) {
		baseline_ranked.push_back(evaluate(candidate, allies, enemies));
	}
	std::sort(baseline_ranked.begin(), baseline_ranked.end(), [](const DraftEvaluation &a, const DraftEvaluation &b) {
		if (a.score == b.score) {
			return String(a.champion) < String(b.champion);
		}
		return a.score > b.score;
	});
	
	// Store baseline top-3 and top-1
	for (int64_t i = 0; i < std::min(int64_t(3), static_cast<int64_t>(baseline_ranked.size())); ++i) {
		report.baseline_top3.push_back(baseline_ranked[i].champion);
	}
	if (!baseline_ranked.empty()) {
		report.baseline_top1 = baseline_ranked[0].champion;
	}
	
	// Create champion -> baseline rank map
	std::map<StringName, int64_t> baseline_ranks;
	for (int64_t i = 0; i < static_cast<int64_t>(baseline_ranked.size()); ++i) {
		baseline_ranks[baseline_ranked[i].champion] = i;
	}
	
	// Generate perturbed scenarios
	ScenarioPerturbationGenerator generator(_database);
	std::vector<PerturbedScenario> scenarios = generator.generate_scenarios(
		allies, enemies, available, seed, scenario_count);
	
	// Accumulate scores and ranks across scenarios
	std::map<StringName, std::vector<double>> score_history;
	std::map<StringName, std::vector<int64_t>> rank_history;
	std::vector<double> all_scores;
	
	int64_t top1_stable_count = 0;
	int64_t top3_stable_count = 0;
	
	for (const auto &scenario : scenarios) {
		// Evaluate this scenario
		std::vector<DraftEvaluation> ranked;
		ranked.reserve(scenario.available.size());
		for (const StringName &candidate : scenario.available) {
			ranked.push_back(evaluate(candidate, scenario.allies, scenario.enemies));
		}
		std::sort(ranked.begin(), ranked.end(), [](const DraftEvaluation &a, const DraftEvaluation &b) {
			if (a.score == b.score) {
				return String(a.champion) < String(b.champion);
			}
			return a.score > b.score;
		});
		
		// Check winner stability (only if champion is in current scenario)
		bool top1_in_scenario = false;
		for (const auto &eval : ranked) {
			if (eval.champion == report.baseline_top1) {
				top1_in_scenario = true;
				break;
			}
		}
		if (top1_in_scenario && !ranked.empty() && ranked[0].champion == report.baseline_top1) {
			++top1_stable_count;
		}
		
		// Check top-3 stability
		std::set<StringName> current_top3;
		int64_t baseline_in_current = 0;
		for (int64_t i = 0; i < std::min(int64_t(3), static_cast<int64_t>(ranked.size())); ++i) {
			current_top3.insert(ranked[i].champion);
		}
		std::set<StringName> baseline_top3_set(report.baseline_top3.begin(), report.baseline_top3.end());
		
		// Count how many baseline top-3 are in current scenario
		for (const StringName &champ : report.baseline_top3) {
			for (const auto &eval : ranked) {
				if (eval.champion == champ) {
					++baseline_in_current;
					break;
				}
			}
		}
		
		// Only count as stable if all baseline top-3 are present and match
		if (baseline_in_current == 3 && current_top3 == baseline_top3_set) {
			++top3_stable_count;
		}
		
		// Accumulate scores and ranks
		for (int64_t i = 0; i < static_cast<int64_t>(ranked.size()); ++i) {
			score_history[ranked[i].champion].push_back(ranked[i].score);
			rank_history[ranked[i].champion].push_back(i);
			all_scores.push_back(ranked[i].score);
		}
	}
	
	// Compute per-candidate statistics
	report.candidate_stats.reserve(available.size());
	for (const StringName &candidate : available) {
		CandidateStressStats stats;
		stats.champion = candidate;
		
		auto score_it = score_history.find(candidate);
		auto rank_it = rank_history.find(candidate);
		
		if (score_it != score_history.end() && !score_it->second.empty()) {
			// Mean score
			double sum = 0.0;
			for (double s : score_it->second) {
				sum += s;
			}
			stats.mean_score = sum / score_it->second.size();
			
			// Score stddev
			double variance = 0.0;
			for (double s : score_it->second) {
				variance += (s - stats.mean_score) * (s - stats.mean_score);
			}
			stats.score_stddev = std::sqrt(variance / score_it->second.size());
		}
		
		if (rank_it != rank_history.end() && !rank_it->second.empty()) {
			// Mean rank
			double sum = 0.0;
			for (int64_t r : rank_it->second) {
				sum += r;
			}
			stats.mean_rank = sum / rank_it->second.size();
			
			// Rank stddev
			double variance = 0.0;
			for (int64_t r : rank_it->second) {
				variance += (r - stats.mean_rank) * (r - stats.mean_rank);
			}
			stats.rank_stddev = std::sqrt(variance / rank_it->second.size());
			
			// Max rank swing
			int64_t min_rank = *std::min_element(rank_it->second.begin(), rank_it->second.end());
			int64_t max_rank = *std::max_element(rank_it->second.begin(), rank_it->second.end());
			stats.max_rank_swing = max_rank - min_rank;
		}
		
		// Baseline reference
		auto baseline_rank_it = baseline_ranks.find(candidate);
		if (baseline_rank_it != baseline_ranks.end()) {
			stats.baseline_rank = baseline_rank_it->second;
			stats.baseline_score = baseline_ranked[stats.baseline_rank].score;
		}
		
		report.candidate_stats.push_back(stats);
	}
	
	// Compute global distribution metrics
	if (!all_scores.empty()) {
		std::sort(all_scores.begin(), all_scores.end());
		report.score_min = all_scores.front();
		report.score_max = all_scores.back();
		
		double sum = 0.0;
		for (double s : all_scores) {
			sum += s;
		}
		report.score_mean = sum / all_scores.size();
		
		// Percentiles
		size_t p50_idx = all_scores.size() * 50 / 100;
		size_t p90_idx = all_scores.size() * 90 / 100;
		report.score_p50 = all_scores[p50_idx];
		report.score_p90 = all_scores[p90_idx];
	}
	
	// Compute rank volatility (average rank change per candidate)
	double total_rank_change = 0.0;
	int64_t total_max_swing = 0;
	for (const auto &stats : report.candidate_stats) {
		total_rank_change += std::abs(stats.mean_rank - stats.baseline_rank);
		total_max_swing = std::max(total_max_swing, stats.max_rank_swing);
	}
	report.avg_rank_change = total_rank_change / report.candidate_stats.size();
	report.max_rank_swing = total_max_swing;
	
	// Compute context sensitivity (avg absolute delta from baseline)
	double total_score_delta = 0.0;
	for (const auto &stats : report.candidate_stats) {
		total_score_delta += std::abs(stats.mean_score - stats.baseline_score);
	}
	report.context_sensitivity = total_score_delta / report.candidate_stats.size();
	
	// Compute inter-rank margins (from baseline ranking)
	std::vector<double> margins;
	for (size_t i = 0; i < baseline_ranked.size() - 1; ++i) {
		margins.push_back(baseline_ranked[i].score - baseline_ranked[i + 1].score);
	}
	if (!margins.empty()) {
		// Mean margin
		double margin_sum = 0.0;
		for (double m : margins) {
			margin_sum += m;
		}
		report.mean_inter_rank_margin = margin_sum / margins.size();
		
		// Median margin
		std::sort(margins.begin(), margins.end());
		size_t median_idx = margins.size() / 2;
		report.median_inter_rank_margin = margins[median_idx];
	}
	
	// Compute winner stability rates
	int64_t valid_scenarios = 0;
	for (const auto &scenario : scenarios) {
		// Count scenarios where baseline top-1 is in available pool
		for (const StringName &champ : scenario.available) {
			if (champ == report.baseline_top1) {
				++valid_scenarios;
				break;
			}
		}
	}
	
	report.top1_stability_rate = valid_scenarios > 0 ? (100.0 * top1_stable_count / valid_scenarios) : 0.0;
	report.top3_stability_rate = valid_scenarios > 0 ? (100.0 * top3_stable_count / valid_scenarios) : 0.0;
	
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
		constexpr double kTieEpsilon = 0.005;
		if (std::abs(a.score - b.score) < kTieEpsilon) {
			int64_t a_rels = a.synergy_relationships + a.counter_relationships;
			int64_t b_rels = b.synergy_relationships + b.counter_relationships;
			if (a_rels != b_rels) {
				return a_rels > b_rels;  // More data = more reliable pick
			}
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
