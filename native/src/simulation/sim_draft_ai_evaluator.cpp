#include "sim_draft_ai_evaluator.hpp"

namespace sim {
namespace draft_ai {

namespace {

double pick_value_confidence_for(const DraftPickScoreBreakdown &pick) {
	double confidence_sum = pick.base_power_confidence;
	int active_components = 1;
	if (pick.synergy_pairs > 0) {
		confidence_sum += pick.ally_synergy_confidence;
		active_components++;
	}
	if (pick.counter_pairs > 0) {
		confidence_sum += pick.enemy_counter_value_confidence;
		confidence_sum += pick.counter_risk_confidence;
		active_components += 2;
	}
	return confidence_sum / static_cast<double>(active_components);
}

} // namespace

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
	result.base_power_samples = base_stat.samples;
	result.base_power_confidence = base_stat.confidence;

	// Ally synergy
	double synergy_sum = 0.0;
	double synergy_confidence_sum = 0.0;
	for (const StringName &ally : allies) {
		DraftStatValue synergy_stat = _stats_database->synergy_winrate_for(candidate, ally);
		synergy_sum += synergy_stat.centered_value;
		result.ally_synergy_samples += synergy_stat.samples;
		synergy_confidence_sum += synergy_stat.confidence;
		result.synergy_pairs++;
	}
	result.ally_synergy = allies.empty() ? 0.0 : synergy_sum / static_cast<double>(allies.size());
	result.ally_synergy_confidence = allies.empty() ? 0.0 : synergy_confidence_sum / static_cast<double>(allies.size());

	// Enemy counter value (how well candidate counters enemies)
	double counter_value_sum = 0.0;
	double counter_value_confidence_sum = 0.0;
	for (const StringName &enemy : enemies) {
		DraftStatValue counter_stat = _stats_database->counter_winrate_for(candidate, enemy);
		counter_value_sum += counter_stat.centered_value;
		result.enemy_counter_value_samples += counter_stat.samples;
		counter_value_confidence_sum += counter_stat.confidence;
		result.counter_pairs++;
	}
	result.enemy_counter_value = enemies.empty() ? 0.0 : counter_value_sum / static_cast<double>(enemies.size());
	result.enemy_counter_value_confidence = enemies.empty() ? 0.0 : counter_value_confidence_sum / static_cast<double>(enemies.size());

	// Counter risk (how well enemies counter candidate)
	double counter_risk_sum = 0.0;
	double counter_risk_confidence_sum = 0.0;
	for (const StringName &enemy : enemies) {
		DraftStatValue enemy_counter_stat = _stats_database->counter_winrate_for(enemy, candidate);
		counter_risk_sum += enemy_counter_stat.centered_value;
		result.counter_risk_samples += enemy_counter_stat.samples;
		counter_risk_confidence_sum += enemy_counter_stat.confidence;
	}
	result.counter_risk = enemies.empty() ? 0.0 : counter_risk_sum / static_cast<double>(enemies.size());
	result.counter_risk_confidence = enemies.empty() ? 0.0 : counter_risk_confidence_sum / static_cast<double>(enemies.size());

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
	result.enemy_pick_value_confidence = base_stat.confidence;

	// Enemy synergy: average synergy with enemy champions
	double enemy_synergy_sum = 0.0;
	double enemy_synergy_confidence_sum = 0.0;
	for (const StringName &enemy : enemies) {
		DraftStatValue synergy_stat = _stats_database->synergy_winrate_for(candidate, enemy);
		enemy_synergy_sum += synergy_stat.centered_value;
		result.enemy_synergy_samples += synergy_stat.samples;
		enemy_synergy_confidence_sum += synergy_stat.confidence;
		result.enemy_synergy_pairs++;
	}
	result.enemy_synergy = enemies.empty() ? 0.0 : enemy_synergy_sum / static_cast<double>(enemies.size());
	result.enemy_synergy_confidence = enemies.empty() ? 0.0 : enemy_synergy_confidence_sum / static_cast<double>(enemies.size());

	// Counters my team: average counter_winrate_for(candidate, ally)
	double counters_my_team_sum = 0.0;
	double counters_my_team_confidence_sum = 0.0;
	for (const StringName &ally : allies) {
		DraftStatValue counter_stat = _stats_database->counter_winrate_for(candidate, ally);
		counters_my_team_sum += counter_stat.centered_value;
		result.counters_my_team_samples += counter_stat.samples;
		counters_my_team_confidence_sum += counter_stat.confidence;
		result.counter_pairs++;
	}
	result.counters_my_team = allies.empty() ? 0.0 : counters_my_team_sum / static_cast<double>(allies.size());
	result.counters_my_team_confidence = allies.empty() ? 0.0 : counters_my_team_confidence_sum / static_cast<double>(allies.size());

	// Gated differential denial: enemy wants it more than we do
	DraftPickScoreBreakdown own_pick = evaluate_candidate_pick(candidate, allies, enemies);
	DraftPickScoreBreakdown enemy_pick = evaluate_candidate_pick(candidate, enemies, allies);
	
	result.own_pick_value = own_pick.total_score;
	result.enemy_pick_value = enemy_pick.total_score;
	result.own_pick_value_confidence = pick_value_confidence_for(own_pick);
	result.enemy_pick_value_confidence = pick_value_confidence_for(enemy_pick);
	result.denial_value_confidence = std::min(result.enemy_pick_value_confidence, result.own_pick_value_confidence);
	
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
