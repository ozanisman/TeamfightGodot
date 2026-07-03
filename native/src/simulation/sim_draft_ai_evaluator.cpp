#include "sim_draft_ai_evaluator.hpp"

namespace sim {
namespace draft_ai {

DraftEvaluator::DraftEvaluator(const DraftStatsDatabase *stats_database, const Config *config)
	: _stats_database(stats_database), _config(config ? config : &_default_config) {
}

DraftPickScoreBreakdown DraftEvaluator::evaluate_candidate_pick(
	const StringName &candidate,
	const std::vector<StringName> &allies,
	const std::vector<StringName> &enemies
) const {
	DraftPickScoreBreakdown result;
	result.candidate = candidate;

	// Base power
	DraftStatValue base_stat = _stats_database->base_winrate_for(candidate);
	result.base_power = base_stat.centered_value;

	// Ally synergy
	double synergy_sum = 0.0;
	for (const StringName &ally : allies) {
		DraftStatValue synergy_stat = _stats_database->synergy_winrate_for(candidate, ally);
		synergy_sum += synergy_stat.centered_value;
		result.synergy_pairs++;
	}
	result.ally_synergy = allies.empty() ? 0.0 : synergy_sum / static_cast<double>(allies.size());

	// Enemy counter value (how well candidate counters enemies)
	double counter_value_sum = 0.0;
	for (const StringName &enemy : enemies) {
		DraftStatValue counter_stat = _stats_database->counter_winrate_for(candidate, enemy);
		counter_value_sum += counter_stat.centered_value;
		result.counter_pairs++;
	}
	result.enemy_counter_value = enemies.empty() ? 0.0 : counter_value_sum / static_cast<double>(enemies.size());

	// Counter risk (how well enemies counter candidate)
	double counter_risk_sum = 0.0;
	for (const StringName &enemy : enemies) {
		DraftStatValue enemy_counter_stat = _stats_database->counter_winrate_for(enemy, candidate);
		counter_risk_sum += enemy_counter_stat.centered_value;
	}
	result.counter_risk = enemies.empty() ? 0.0 : counter_risk_sum / static_cast<double>(enemies.size());

	// Total score (NOTE: overwritten by DraftRecommender with phase-aware weights before use).
	result.total_score =
		_config->evaluator_weights.base_power_weight * result.base_power +
		_config->evaluator_weights.ally_synergy_weight * result.ally_synergy +
		_config->evaluator_weights.enemy_counter_value_weight * result.enemy_counter_value -
		_config->evaluator_weights.counter_risk_weight * result.counter_risk;

	return result;
}

DraftBanScoreBreakdown DraftEvaluator::evaluate_candidate_ban(
	const StringName &candidate,
	const std::vector<StringName> &allies,
	const std::vector<StringName> &enemies
) const {
	DraftBanScoreBreakdown result;
	result.candidate = candidate;

	// Enemy pick value: base value of candidate as a pick for enemy
	DraftStatValue base_stat = _stats_database->base_winrate_for(candidate);
	result.enemy_pick_value = base_stat.centered_value;

	// Enemy synergy: average synergy with enemy champions
	double enemy_synergy_sum = 0.0;
	for (const StringName &enemy : enemies) {
		DraftStatValue synergy_stat = _stats_database->synergy_winrate_for(candidate, enemy);
		enemy_synergy_sum += synergy_stat.centered_value;
		result.enemy_synergy_pairs++;
	}
	result.enemy_synergy = enemies.empty() ? 0.0 : enemy_synergy_sum / static_cast<double>(enemies.size());

	// Counters my team: average counter_winrate_for(candidate, ally)
	double counters_my_team_sum = 0.0;
	for (const StringName &ally : allies) {
		DraftStatValue counter_stat = _stats_database->counter_winrate_for(candidate, ally);
		counters_my_team_sum += counter_stat.centered_value;
		result.counter_pairs++;
	}
	result.counters_my_team = allies.empty() ? 0.0 : counters_my_team_sum / static_cast<double>(allies.size());

	// Gated differential denial: enemy wants it more than we do
	DraftPickScoreBreakdown own_pick = evaluate_candidate_pick(candidate, allies, enemies);
	DraftPickScoreBreakdown enemy_pick = evaluate_candidate_pick(candidate, enemies, allies);
	
	result.own_pick_value = own_pick.total_score;
	result.enemy_pick_value = enemy_pick.total_score;
	
	// Gate to positive values only to avoid rewarding bans of weak champions
	double positive_own_value = std::max(0.0, own_pick.total_score);
	double positive_enemy_value = std::max(0.0, enemy_pick.total_score);
	
	result.denial_value = positive_enemy_value - positive_own_value;
	
	// Legacy field for backward compatibility/debug
	result.own_pick_value_penalty = -positive_own_value * _config->evaluator_weights.own_pick_value_penalty_weight;

	// Total ban score with differential denial (NOTE: overwritten by DraftRecommender with
	// phase-aware weights before use).
	result.total_score =
		_config->evaluator_weights.own_pick_value_penalty_weight * result.denial_value +
		_config->evaluator_weights.enemy_synergy_weight * result.enemy_synergy +
		_config->evaluator_weights.counters_my_team_weight * result.counters_my_team;

	return result;
}

} // namespace draft_ai
} // namespace sim
