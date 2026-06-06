#ifndef SIM_DRAFT_RECOMMENDER_HPP
#define SIM_DRAFT_RECOMMENDER_HPP

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <map>
#include <vector>

using namespace godot;

namespace sim {
namespace draft {

struct DraftScoreWeights {
	double base = 0.50;
	double synergy = 0.25;
	double matchup = 0.25;
};

struct DraftEvaluation {
	StringName champion;
	double score = 0.0;
	double base_winrate = 0.5;
	double avg_synergy = 0.5;
	double avg_matchup = 0.5;
	int64_t synergy_samples = 0;
	int64_t matchup_samples = 0;
};

struct EvalDebug {
	double base = 0.0;
	double synergy = 0.0;
	double matchup = 0.0;
	double final = 0.0;
};

class DraftStatsDatabase {
public:
	using MatchupMap = std::map<String, std::map<String, double>>;

	bool load_from_dir(const String &dir_path);
	bool is_loaded() const;
	String last_error() const;

	double base_winrate_for(const StringName &champion, double fallback = 0.5) const;
	bool synergy_winrate_for(const StringName &champion, const StringName &ally, double &out_winrate) const;
	bool matchup_winrate_for(const StringName &champion, const StringName &enemy, double &out_winrate) const;

private:
	std::map<String, double> _base_winrates;
	MatchupMap _synergy_winrates;
	MatchupMap _matchup_winrates;
	bool _loaded = false;
	String _last_error;

	bool _load_combat_stats(const String &path);
	bool _load_matchup_stats(const String &path, MatchupMap &target);
};

class DraftEvaluator {
public:
	explicit DraftEvaluator(const DraftStatsDatabase &database, DraftScoreWeights weights = DraftScoreWeights());
	DraftEvaluation evaluate(const StringName &candidate, const std::vector<StringName> &allies, const std::vector<StringName> &enemies) const;
	double evaluate_candidate(const StringName &candidate, const std::vector<StringName> &allies, const std::vector<StringName> &enemies, EvalDebug *out_debug = nullptr) const;

private:
	const DraftStatsDatabase &_database;
	DraftScoreWeights _weights;
};

class DraftRecommender {
public:
	explicit DraftRecommender(const DraftEvaluator &evaluator, bool debug_mode = false);
	void set_debug_mode(bool enabled);
	std::vector<DraftEvaluation> recommend(const std::vector<StringName> &allies, const std::vector<StringName> &enemies, const std::vector<StringName> &available) const;
	void print_top(const std::vector<DraftEvaluation> &ranked, int64_t top_n) const;
	void run_debug_evaluation_batch(const std::vector<StringName> &allies, const std::vector<StringName> &enemies, const std::vector<StringName> &available, int64_t num_runs = 50) const;

private:
	const DraftEvaluator &_evaluator;
	bool _debug_mode = false;
};

} // namespace draft
} // namespace sim

#endif
