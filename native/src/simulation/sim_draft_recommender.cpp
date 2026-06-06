#include "sim_draft_recommender.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>

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

bool lookup_pair(const DraftStatsDatabase::MatchupMap &map, const StringName &a, const StringName &b, double &out_winrate) {
	auto it = map.find(a);
	if (it == map.end()) {
		return false;
	}
	auto jt = it->second.find(b);
	if (jt == it->second.end()) {
		return false;
	}
	out_winrate = jt->second;
	return true;
}

} // namespace

bool DraftStatsDatabase::load_from_dir(const String &dir_path) {
	_base_winrates.clear();
	_synergy_winrates.clear();
	_matchup_winrates.clear();
	_loaded = false;
	_last_error = String();

	String normalized = dir_path.trim_suffix("/");
	if (!_load_combat_stats(normalized + "/combat_stats.csv")) {
		return false;
	}
	if (!_load_matchup_stats(normalized + "/matchup_with.csv", _synergy_winrates)) {
		return false;
	}
	if (!_load_matchup_stats(normalized + "/matchup_vs.csv", _matchup_winrates)) {
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

double DraftStatsDatabase::base_winrate_for(const StringName &champion, double fallback) const {
	auto it = _base_winrates.find(champion);
	if (it == _base_winrates.end()) {
		return fallback;
	}
	return it->second;
}

bool DraftStatsDatabase::synergy_winrate_for(const StringName &champion, const StringName &ally, double &out_winrate) const {
	return lookup_pair(_synergy_winrates, champion, ally, out_winrate) || lookup_pair(_synergy_winrates, ally, champion, out_winrate);
}

bool DraftStatsDatabase::matchup_winrate_for(const StringName &champion, const StringName &enemy, double &out_winrate) const {
	return lookup_pair(_matchup_winrates, champion, enemy, out_winrate);
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
		if (hero.is_empty() || winrate.is_empty()) {
			continue;
		}
		_base_winrates[StringName(hero)] = winrate.to_float();
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
	std::vector<String> header = parse_csv_line(file->get_line());
	int64_t champion_col = column_index(header, "champion");
	int64_t other_col = column_index(header, "ally");
	if (other_col < 0) {
		other_col = column_index(header, "opponent");
	}
	int64_t winrate_col = column_index(header, "winrate");
	if (champion_col < 0 || other_col < 0 || winrate_col < 0) {
		_last_error = vformat("DraftStatsDatabase: missing matchup columns in %s", path);
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
		if (champion.is_empty() || other.is_empty() || winrate.is_empty()) {
			continue;
		}
		target[StringName(champion)][StringName(other)] = winrate.to_float();
	}
	return true;
}

DraftEvaluator::DraftEvaluator(const DraftStatsDatabase &database, DraftScoreWeights weights) :
		_database(database),
		_weights(weights) {
}

DraftEvaluation DraftEvaluator::evaluate(const StringName &candidate, const std::vector<StringName> &allies, const std::vector<StringName> &enemies) const {
	DraftEvaluation result;
	result.champion = candidate;
	result.base_winrate = _database.base_winrate_for(candidate, 0.5);

	double synergy_total = 0.0;
	for (const StringName &ally : allies) {
		double winrate = 0.0;
		if (_database.synergy_winrate_for(candidate, ally, winrate)) {
			synergy_total += winrate;
			++result.synergy_samples;
		}
	}
	result.avg_synergy = result.synergy_samples > 0 ? synergy_total / double(result.synergy_samples) : result.base_winrate;

	double matchup_total = 0.0;
	for (const StringName &enemy : enemies) {
		double winrate = 0.0;
		if (_database.matchup_winrate_for(candidate, enemy, winrate)) {
			matchup_total += winrate;
			++result.matchup_samples;
		}
	}
	result.avg_matchup = result.matchup_samples > 0 ? matchup_total / double(result.matchup_samples) : result.base_winrate;

	result.score = (_weights.base * result.base_winrate) + (_weights.synergy * result.avg_synergy) + (_weights.matchup * result.avg_matchup);
	return result;
}

double DraftEvaluator::evaluate_candidate(const StringName &candidate, const std::vector<StringName> &allies, const std::vector<StringName> &enemies, EvalDebug *out_debug) const {
	DraftEvaluation evaluation = evaluate(candidate, allies, enemies);
	if (out_debug != nullptr) {
		out_debug->base = evaluation.base_winrate;
		out_debug->synergy = evaluation.avg_synergy;
		out_debug->matchup = evaluation.avg_matchup;
		out_debug->final = evaluation.score;
	}
	return evaluation.score;
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
			UtilityFunctions::print(vformat("  base:    %.6f", r.base_winrate));
			UtilityFunctions::print(vformat("  synergy: %.6f", r.avg_synergy));
			UtilityFunctions::print(vformat("  matchup: %.6f", r.avg_matchup));
			UtilityFunctions::print(vformat("  final:   %.6f", r.score));
		} else {
			UtilityFunctions::print(vformat(
					"%d. %s score=%.6f base=%.6f synergy=%.6f(%d) matchup=%.6f(%d)",
					i + 1,
					String(r.champion),
					r.score,
					r.base_winrate,
					r.avg_synergy,
					r.synergy_samples,
					r.avg_matchup,
					r.matchup_samples));
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
