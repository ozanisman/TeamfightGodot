#ifndef SIM_DRAFT_AI_EVALUATOR_HPP
#define SIM_DRAFT_AI_EVALUATOR_HPP

#include "sim_draft_ai_types.hpp"
#include "sim_draft_ai_stats_database.hpp"

#include <godot_cpp/variant/string_name.hpp>
#include <vector>

using namespace godot;

namespace sim {
namespace draft_ai {

class DraftEvaluator {
public:
	explicit DraftEvaluator(const DraftStatsDatabase *stats_database);
	~DraftEvaluator() = default;

	DraftPickScoreBreakdown evaluate_candidate_pick(
		const StringName &candidate,
		const std::vector<StringName> &allies,
		const std::vector<StringName> &enemies
	) const;

	DraftBanScoreBreakdown evaluate_candidate_ban(
		const StringName &candidate,
		const std::vector<StringName> &allies,
		const std::vector<StringName> &enemies
	) const;

private:
	const DraftStatsDatabase *_stats_database;

	static constexpr double BASE_POWER_WEIGHT = 0.35;
	static constexpr double ALLY_SYNERGY_WEIGHT = 0.25;
	static constexpr double ENEMY_COUNTER_VALUE_WEIGHT = 0.25;
	static constexpr double COUNTER_RISK_WEIGHT = 0.15;

	static constexpr double ENEMY_PICK_VALUE_WEIGHT = 0.40;
	static constexpr double ENEMY_SYNERGY_WEIGHT = 0.25;
	static constexpr double COUNTERS_MY_TEAM_WEIGHT = 0.25;
	static constexpr double OWN_PICK_VALUE_PENALTY_WEIGHT = 0.60;
};

} // namespace draft_ai
} // namespace sim

#endif // SIM_DRAFT_AI_EVALUATOR_HPP
