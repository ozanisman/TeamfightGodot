#ifndef SIM_MATCH_LIFECYCLE_HPP
#define SIM_MATCH_LIFECYCLE_HPP

#include "sim_match_roster.hpp"
#include "sim_viewer.hpp"
#include "sim_world.hpp"

#include <cstdint>
#include <vector>

namespace sim {
namespace match {

struct MatchScoreState {
	int64_t *player_kills = nullptr;
	int64_t *enemy_kills = nullptr;
	TickContext *tick_ctx = nullptr;
	match_roster::MatchRosterState *roster = nullptr;
	double time = 0.0;
};

struct SpawnSlotState {
	std::vector<bool> &player_slots_used;
	std::vector<bool> &enemy_slots_used;
	uint32_t (*rand_uint32)(void *user_data) = nullptr;
	Vector2 (*get_random_spawn_position)(void *user_data, const StringName &team, bool is_respawn) = nullptr;
	void *rand_user_data = nullptr;
};

void clear_spawn_slots(SpawnSlotState &slots);
int64_t assign_spawn_slot(SpawnSlotState &slots, const StringName &team);
void release_spawn_slot(SpawnSlotState &slots, const StringName &team, int64_t slot_index);

void handle_death(
		SimWorld &world,
		SimHostCallbacks &host,
		ViewerHooks *viewer,
		MatchScoreState &score,
		SpawnSlotState &slots,
		UnitState &killer,
		UnitState &target);

void respawn_unit(
		SimWorld &world,
		SimHostCallbacks &host,
		ViewerHooks *viewer,
		MatchScoreState &score,
		SpawnSlotState &slots,
		UnitState &unit);

} // namespace match
} // namespace sim

#endif // SIM_MATCH_LIFECYCLE_HPP
