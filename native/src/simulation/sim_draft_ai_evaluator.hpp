#ifndef SIM_DRAFT_AI_EVALUATOR_HPP
#define SIM_DRAFT_AI_EVALUATOR_HPP

#include "sim_draft_ai_types.hpp"
#include "sim_draft_ai_stats_database.hpp"
#include "sim_draft_ai_config.hpp"

#include <godot_cpp/variant/string_name.hpp>
#include <vector>

using namespace godot;

namespace sim {
namespace draft_ai {

class DraftEvaluator {
public:
	explicit DraftEvaluator(const DraftStatsDatabase *stats_database, const Config *config = nullptr);
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
	Config _default_config;
	const Config *_config;
};

} // namespace draft_ai
} // namespace sim

#endif // SIM_DRAFT_AI_EVALUATOR_HPP
