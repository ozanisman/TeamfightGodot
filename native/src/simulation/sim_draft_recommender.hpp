#ifndef SIM_DRAFT_RECOMMENDER_HPP
#define SIM_DRAFT_RECOMMENDER_HPP

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <map>
#include <vector>

using namespace godot;

namespace sim {
namespace draft {

struct PredictionConfig {
	double base_weight = 0.50;
	double synergy_weight = 0.25;
	double matchup_weight = 0.25;
	double composition_weight = 0.0;

	// Signal amplification factors
	double synergy_amplification = 1.2;
	double matchup_amplification = 1.2;

	// Composition balance parameters
	double composition_balance_weight = 0.05;
	double composition_unbalance_penalty = 0.1;

	double prior_winrate = 0.5;
	int confidence_prior_samples = 100;

	double logistic_k = 10.0;
};

struct DraftScoreWeights {
	double base = 0.50;
	double synergy = 0.25;
	double counter = 0.25;
};

struct DraftEvaluation {
	StringName champion;
	double score = 0.0;
	
	// Adjusted values (after Bayesian smoothing)
	double base_winrate = 0.5;
	double avg_synergy = 0.5;
	double avg_counter = 0.5;
	
	// Raw values (before Bayesian smoothing)
	double base_raw = 0.5;
	double synergy_raw = 0.5;
	double counter_raw = 0.5;
	
	// Statistical sample counts (from CSV, used in Bayesian smoothing)
	int64_t base_samples = 0;
	int64_t synergy_stat_samples = 0;
	int64_t counter_stat_samples = 0;
	
	// Relationship counts (number of allies/enemies evaluated)
	int64_t synergy_relationships = 0;
	int64_t counter_relationships = 0;
};

struct EvalDebug {
	double base = 0.0;
	double synergy = 0.0;
	double counter = 0.0;
	double final = 0.0;
};

struct SignalInfluenceReport {
	double base_only_score = 0.0;
	double synergy_removed_score = 0.0;
	double matchup_removed_score = 0.0;
	double full_score = 0.0;
	
	double base_only_delta = 0.0;
	double synergy_removed_delta = 0.0;
	double matchup_removed_delta = 0.0;
	
	int64_t ranking_shift_synergy_removed = 0;
	int64_t ranking_shift_matchup_removed = 0;
};

struct ControlledEvaluationReport {
	double avg_synergy_impact = 0.0;
	double avg_matchup_impact = 0.0;
	int64_t top3_overlap_synergy_removed = 0;
	int64_t top3_overlap_matchup_removed = 0;
};

class DraftStatsDatabase {
public:
	struct StatValue {
		double winrate = 0.5;
		int64_t samples = 0;
	};

	using MatchupMap = std::map<String, std::map<String, StatValue>>;
	using CompositionMap = std::map<String, StatValue>;

	bool load_from_dir(const String &dir_path);
	bool is_loaded() const;
	String last_error() const;

	StatValue base_winrate_for(const StringName &champion) const;
	bool synergy_winrate_for(const StringName &champion, const StringName &ally, StatValue &out_value) const;
	bool counter_winrate_for(const StringName &champion, const StringName &enemy, StatValue &out_value) const;
	StatValue composition_winrate_for(const std::vector<StringName> &team) const;
	StringName get_champion_role(const StringName &champion) const;

	struct TeamScoreBreakdown {
		double base = 0.5;
		double synergy = 0.5;
		double matchup = 0.5;
		double composition = 0.5;
		double final = 0.5;
	};

	double calculate_team_score(const std::vector<StringName> &team, const std::vector<StringName> &enemies, const PredictionConfig &config, TeamScoreBreakdown &out_breakdown) const;
	void calculate_win_probability(const std::vector<StringName> &team1, const std::vector<StringName> &team2, const PredictionConfig &config, double &out_team1_prob, double &out_team2_prob, TeamScoreBreakdown &out_team1_breakdown, TeamScoreBreakdown &out_team2_breakdown) const;
	double apply_bayesian_smoothing(double raw_winrate, int64_t samples, const PredictionConfig &config) const;

private:
	std::map<String, StatValue> _base_winrates;
	MatchupMap _synergy_winrates;
	MatchupMap _counter_winrates;
	CompositionMap _composition_winrates;
	std::map<StringName, StringName> _champion_roles;  // champion -> role mapping
	bool _loaded = false;
	String _last_error;

	bool _load_combat_stats(const String &path);
	bool _load_counter_stats(const String &path, MatchupMap &target);
	bool _load_composition_stats(const String &path);
	void _load_champion_roles();
};

class DraftEvaluator {
public:
	explicit DraftEvaluator(const DraftStatsDatabase &database, PredictionConfig config = PredictionConfig());
	DraftEvaluation evaluate(const StringName &candidate, const std::vector<StringName> &allies, const std::vector<StringName> &enemies) const;
	double evaluate_candidate(const StringName &candidate, const std::vector<StringName> &allies, const std::vector<StringName> &enemies, EvalDebug *out_debug = nullptr) const;
	SignalInfluenceReport analyze_signal_influence(const StringName &candidate, const std::vector<StringName> &allies, const std::vector<StringName> &enemies) const;
	ControlledEvaluationReport run_controlled_evaluation(const std::vector<StringName> &allies, const std::vector<StringName> &enemies, const std::vector<StringName> &available) const;

private:
	const DraftStatsDatabase &_database;
	PredictionConfig _config;
	double calculate_composition_bonus(const std::vector<StringName> &team, const StringName &candidate) const;
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
