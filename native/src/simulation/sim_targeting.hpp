#ifndef SIM_TARGETING_HPP
#define SIM_TARGETING_HPP

#include "sim_constants.hpp"
#include "sim_spatial.hpp"
#include "sim_world.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace sim {
namespace targeting {

struct ScoreEnemyProfileCounters {
	bool active = false;
	uint64_t *se_base = nullptr;
	int64_t *se_calls = nullptr;
};

struct TargetingProfileCounters {
	bool active = false;
	int64_t *retarget_keeps = nullptr;
	int64_t *enemy_scans = nullptr;
	int64_t *candidates_scored = nullptr;
	int64_t *ties_adjusted = nullptr;
	int64_t *ties_distance = nullptr;
	int64_t *ties_instance = nullptr;
	int64_t *ally_scans = nullptr;
};

struct TargetingDebugHooks {
	bool enabled = false;
	void *user_data = nullptr;
	StringName (*archetype_for_unit)(void *user_data, const UnitState &unit) = nullptr;
	void (*print_line)(void *user_data, const String &line) = nullptr;
	void (*print_score_breakdown)(
			void *user_data,
			const ScoreBreakdown &breakdown,
			const StringName &attacker_archetype,
			const StringName &enemy_archetype) = nullptr;
};

UnitState *unit_by_id(SimWorld &world, int64_t instance_id);
const UnitState *unit_by_id(const SimWorld &world, int64_t instance_id);
int64_t unit_index_by_id(const SimWorld &world, int64_t instance_id);
int64_t target_index_for_unit(SimWorld &world, UnitState &unit);

double attack_range(const UnitState &unit);
double effective_attack_range(const UnitState &unit);

TargetingFrameEntry make_targeting_frame_entry(const UnitState &unit);
void sync_targeting_frame_index(SimWorld &world, int64_t index, const UnitState &unit);
void sync_targeting_frame_unit(SimWorld &world, const UnitState &unit);

double score_ally_target(
		const UnitState &unit,
		const TargetingFrameEntry &ally,
		const UnitStrategy &strategy,
		double unit_ally_distance = -1.0);

double score_enemy_target(
		SimWorld &world,
		const UnitState &attacker,
		const TargetingFrameEntry &enemy,
		const TargetingFrameEntry *ally_for_peel,
		const UnitStrategy &strategy,
		const TickContext &ctx,
		const TargetScoreContext &score_ctx,
		double attacker_enemy_distance = -1.0,
		const ScoreEnemyProfileCounters *profile = nullptr,
		int64_t enemy_index = -1,
		ScoreBreakdown *breakdown = nullptr);

bool should_switch(
		SimWorld &world,
		const UnitState &unit,
		double current_score,
		double new_score,
		const UnitStrategy &strategy,
		double current_target_distance = -1.0);

void adjust_target_pressure(SimWorld &world, int64_t old_target_id, int64_t new_target_id, const SimHostCallbacks *host = nullptr);
void set_current_target(SimWorld &world, UnitState &unit, const UnitState &target, const SimHostCallbacks *host);

UnitState *select_enemy_target(
		SimWorld &world,
		UnitState &unit,
		const UnitStrategy &strategy,
		const TargetScoreContext &score_ctx,
		const SimHostCallbacks *host,
		const TargetingProfileCounters *profile = nullptr,
		const ScoreEnemyProfileCounters *score_profile = nullptr,
		const TargetingDebugHooks *debug = nullptr);

UnitState *select_ally_target(SimWorld &world, UnitState &unit, const UnitStrategy &strategy, const TargetingProfileCounters *profile = nullptr);

struct CoordinatorTargetingState {
	bool profile_targeting_active = false;
	bool profile_score_enemy = false;
	bool debug_targeting_scoring = false;
	void *debug_user_data = nullptr;
	StringName (*debug_archetype_for_unit)(void *user_data, const UnitState &unit) = nullptr;
	void (*debug_print_line)(void *user_data, const String &line) = nullptr;
	void (*debug_print_score_breakdown)(
			void *user_data,
			const ScoreBreakdown &breakdown,
			const StringName &attacker_archetype,
			const StringName &enemy_archetype) = nullptr;
	int64_t *tgt_retarget_keeps = nullptr;
	int64_t *tgt_enemy_scans = nullptr;
	int64_t *tgt_candidates_scored = nullptr;
	int64_t *tgt_ties_adjusted = nullptr;
	int64_t *tgt_ties_distance = nullptr;
	int64_t *tgt_ties_instance = nullptr;
	int64_t *tgt_ally_scans = nullptr;
	uint64_t *profile_se_base = nullptr;
	int64_t *profile_se_calls = nullptr;
};

UnitState *select_enemy_target_coordinator(
		SimWorld &world,
		UnitState &unit,
		const UnitStrategy &strategy,
		SimHostCallbacks *host,
		const CoordinatorTargetingState &state);

UnitState *select_ally_target_coordinator(
		SimWorld &world,
		UnitState &unit,
		const UnitStrategy &strategy,
		const CoordinatorTargetingState &state);

void prepare_tick_context(SimWorld &world, const SimHostCallbacks &host);

std::vector<UnitState *> select_targets(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		int64_t target_count,
		TargetSelectionStrategy strategy,
		bool include_source,
		ExcessTargetHandling excess_handling,
		const StringName &team_filter);

} // namespace targeting
} // namespace sim

#endif // SIM_TARGETING_HPP
