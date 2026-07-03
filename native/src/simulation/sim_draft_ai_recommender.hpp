#ifndef SIM_DRAFT_AI_RECOMMENDER_HPP
#define SIM_DRAFT_AI_RECOMMENDER_HPP

#include "sim_draft_ai_types.hpp"
#include "sim_draft_ai_evaluator.hpp"
#include "sim_draft_ai_stats_database.hpp"
#include "sim_draft_ai_config.hpp"

#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <vector>

using namespace godot;

namespace sim {
namespace draft_ai {

class DraftRecommender {
public:
	explicit DraftRecommender(const DraftEvaluator *evaluator, const DraftStatsDatabase *database, const Config *config = nullptr);
	~DraftRecommender() = default;

	std::vector<DraftPickScoreBreakdown> recommend_picks(
		const std::vector<StringName> &available,
		const std::vector<StringName> &allies,
		const std::vector<StringName> &enemies,
		int max_results,
		int draft_step = -1,
		DraftStrategy strategy = DraftStrategy::NATIVE_FULL
	) const;

	std::vector<DraftBanScoreBreakdown> recommend_bans(
		const std::vector<StringName> &available,
		const std::vector<StringName> &allies,
		const std::vector<StringName> &enemies,
		int max_results,
		int draft_step = -1,
		const String &acting_side = "blue",
		const Dictionary &weight_overrides = Dictionary(),
		DraftStrategy strategy = DraftStrategy::NATIVE_FULL
	) const;

	// Lookahead turn diagnostic
	static void generate_lookahead_turn_diagnostic();

private:
	const DraftEvaluator *_evaluator;
	const DraftStatsDatabase *_database;
	Config _default_config;
	const Config *_config;

	// Lookahead turn logic helpers
	static bool is_pick_step(int step);
	static bool is_ban_step(int step);
	static String side_for_step(int step);
	static int next_pick_step_after(int step);
	static int next_pick_step_for_side_after(int step, const String &side);
};

} // namespace draft_ai
} // namespace sim

#endif // SIM_DRAFT_AI_RECOMMENDER_HPP
