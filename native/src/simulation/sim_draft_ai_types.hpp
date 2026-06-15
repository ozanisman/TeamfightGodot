#ifndef SIM_DRAFT_AI_TYPES_HPP
#define SIM_DRAFT_AI_TYPES_HPP

#include <godot_cpp/variant/string_name.hpp>
#include <vector>

using namespace godot;

namespace sim {
namespace draft_ai {

enum class DraftStrategy {
	NATIVE_FULL,                    // Current baseline: no lookahead
	NATIVE_LOOKAHEAD,              // Experimental: 1-ply lookahead (both picks and bans)
	NATIVE_LOOKAHEAD_PICK,         // Experimental: lookahead only for picks
	NATIVE_LOOKAHEAD_BAN           // Experimental: lookahead only for bans
};

struct DraftStatValue {
	double raw_winrate = 0.5;
	double adjusted_winrate = 0.5;
	double centered_value = 0.0;

	int wins = 0;
	int losses = 0;
	int samples = 0;

	double confidence = 0.0;
	bool found = false;
};

struct DraftPredictionConfig {
	double prior_winrate = 0.50;
	double prior_samples = 100.0;
	double confidence_prior_samples = 100.0;
	int min_samples = 0;
};

struct DraftState {
	std::vector<StringName> allies;
	std::vector<StringName> enemies;
	std::vector<StringName> available;
	int phase = 0;
	int turn = 0;
};

struct DraftPickScoreBreakdown {
	StringName candidate;
	double total_score = 0.0;

	double base_power = 0.0;
	double ally_synergy = 0.0;
	double enemy_counter_value = 0.0;
	double counter_risk = 0.0;

	double base_power_weight = 0.0;
	double ally_synergy_weight = 0.0;
	double enemy_counter_value_weight = 0.0;
	double counter_risk_weight = 0.0;

	double role_fit = 0.0;
	double role_fit_weight = 0.0;
	StringName candidate_role;

	double comp_fit = 0.0;
	double comp_fit_weight = 0.0;
	String comp_fingerprint;
	int comp_samples = 0;
	int comp_match_count = 0;

	int synergy_pairs = 0;
	int counter_pairs = 0;

	int draft_step = -1;
	String phase_label;

	// Lookahead debug fields
	double immediate_score = 0.0;
	double lookahead_adjustment = 0.0;
	StringName opponent_response_candidate;
	double opponent_response_score = 0.0;
	String current_side;
	int next_pick_step = -1;
	String next_pick_side;
	bool next_pick_is_enemy = false;

	// Archetype tags (debug-only, not used in scoring)
	std::vector<StringName> candidate_tags;
};

struct DraftBanScoreBreakdown {
	StringName candidate;
	double total_score = 0.0;

	double enemy_pick_value = 0.0;
	double own_pick_value = 0.0;
	double denial_value = 0.0;
	double enemy_synergy = 0.0;
	double counters_my_team = 0.0;

	double denial_value_weight = 0.0;
	double enemy_synergy_weight = 0.0;
	double counters_my_team_weight = 0.0;
	double own_pick_value_penalty = 0.0;  // Legacy/debug only

	double fills_enemy_role_need = 0.0;
	double fills_enemy_role_need_weight = 0.0;
	StringName candidate_role;

	double enemy_comp_fit = 0.0;
	double enemy_comp_fit_weight = 0.0;
	String enemy_comp_fingerprint;
	int enemy_comp_samples = 0;
	int enemy_comp_match_count = 0;

	int enemy_synergy_pairs = 0;
	int counter_pairs = 0;

	String acting_side;
	bool enemy_gets_first_pick_after_ban_phase = false;
	double early_ban_fallback_component = 0.0;

	int draft_step = -1;
	String phase_label;

	// Lookahead debug fields
	double immediate_score = 0.0;
	double lookahead_adjustment = 0.0;
	StringName enemy_best_pick_before;
	double enemy_best_pick_score_before = 0.0;
	StringName enemy_best_pick_after;
	double enemy_best_pick_score_after = 0.0;
	double denied_enemy_pick_value = 0.0;
	String current_side;
	int enemy_next_pick_step = -1;

	// Archetype tags (debug-only, not used in scoring)
	std::vector<StringName> candidate_tags;
};

} // namespace draft_ai
} // namespace sim

#endif // SIM_DRAFT_AI_TYPES_HPP
