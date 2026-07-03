#ifndef SIM_DRAFT_AI_CONFIG_HPP
#define SIM_DRAFT_AI_CONFIG_HPP

#include <godot_cpp/variant/string.hpp>

using namespace godot;

namespace sim {
namespace draft_ai {

// Phase-specific pick scoring weights. Defaults reproduce the literals that were
// previously hardcoded in DraftRecommender::recommend_picks.
struct PickPhaseWeights {
	double base_power = 0.35;
	double ally_synergy = 0.25;
	double enemy_counter_value = 0.25;
	double counter_risk = -0.15;
	double role_fit = 0.10;
	double comp_fit = 0.08;
};

// Phase-specific ban scoring weights. Defaults reproduce the literals that were
// previously hardcoded in DraftRecommender::recommend_bans.
struct BanPhaseWeights {
	double denial_value = 0.60;
	double enemy_synergy = 0.25;
	double counters_my_team = 0.25;
	double fills_enemy_role_need = 0.10;
	double enemy_comp_fit = 0.08;
	double early_fallback_multiplier = 0.30;
};

// Draft-step ranges that select which PickPhaseWeights/BanPhaseWeights apply.
struct PickPhaseRanges {
	int early_start = 6;
	int early_end = 8;
	int mid_start = 9;
	int mid_end = 11;
	int late_start = 16;
	int late_end = 19;
};

struct BanPhaseRanges {
	int phase1_start = 0;
	int phase1_end = 5;
	int phase2_start = 12;
	int phase2_end = 15;
};

// Role-fit step function for picks (DraftRecommender::recommend_picks).
struct RoleFitSteps {
	double not_present = 0.050;
	double once = 0.020;
	double twice = -0.030;
	double three_plus = -0.060;
};

// Fills-enemy-role-need step function for bans (DraftRecommender::recommend_bans).
struct FillsRoleSteps {
	double not_present = 0.050;
	double once = 0.020;
	double two_plus = -0.020;
};

// NOTE: These weights are used to compute the intermediate total_score in
// DraftEvaluator::evaluate_candidate_pick/ban. That intermediate pick score is used directly
// by DraftEvaluator::evaluate_candidate_ban to derive denial_value, so changing these weights
// DOES affect ban recommendations. The total_score computed at the end of evaluate_candidate_ban
// is overwritten by DraftRecommender with phase-aware weights, but the pick total_score values
// feeding into denial_value are not.
struct EvaluatorWeights {
	double base_power_weight = 0.35;
	double ally_synergy_weight = 0.25;
	double enemy_counter_value_weight = 0.25;
	double counter_risk_weight = 0.15;

	double enemy_pick_value_weight = 0.40;
	double enemy_synergy_weight = 0.25;
	double counters_my_team_weight = 0.25;
	double own_pick_value_penalty_weight = 0.60;
};

// 1-ply lookahead constants (quarantined NATIVE_LOOKAHEAD* strategies).
struct LookaheadParams {
	double pick_response_weight = 0.35;
	double ban_denied_enemy_pick_weight = 0.50;
	int lookahead_candidates = 8;
	double denial_negligible_epsilon = 0.001;
};

// Centralized tunables for the sim::draft_ai recommender/evaluator. Default member
// initializers reproduce today's hardcoded behavior exactly; an optional JSON override
// file can selectively replace values for tuning experiments.
struct Config {
	PickPhaseWeights pick_default; // Also used for the "mid_pick" phase (identical values).
	PickPhaseWeights pick_early;
	PickPhaseWeights pick_late;
	PickPhaseRanges pick_ranges;

	BanPhaseWeights ban_default;
	BanPhaseWeights ban_phase1;
	BanPhaseWeights ban_phase2;
	BanPhaseRanges ban_ranges;

	RoleFitSteps role_fit_steps;
	FillsRoleSteps fills_role_steps;
	EvaluatorWeights evaluator_weights;
	LookaheadParams lookahead;

	// Returns defaults if override_path is empty, the file does not exist, or parsing fails
	// (a warning is emitted for the latter case). Missing keys within a present file fall
	// back to their default value individually.
	static Config load_with_optional_override(const String &override_path);
};

} // namespace draft_ai
} // namespace sim

#endif // SIM_DRAFT_AI_CONFIG_HPP
