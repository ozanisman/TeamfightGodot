#ifndef SIM_TARGETING_INTERNAL_HPP
#define SIM_TARGETING_INTERNAL_HPP

#include "sim_constants.hpp"
#include "sim_targeting.hpp"
#include "sim_world.hpp"

#include <godot_cpp/variant/string_name.hpp>

#include <array>
#include <chrono>
#include <cstdint>

namespace sim {
namespace targeting {
namespace internal {

inline const godot::StringName &player_team_name() {
	static const godot::StringName s("player");
	return s;
}

inline const godot::StringName &enemy_team_name() {
	static const godot::StringName s("enemy");
	return s;
}

inline const godot::StringName &ally_filter_name() {
	static const godot::StringName s("ally");
	return s;
}

double random_unit(SimHostCallbacks &host);
double strategy_role_prio(const std::array<double, ROLE_SLOT_COUNT> &slots, int64_t role_slot);

uint64_t profile_elapsed_ns(std::chrono::steady_clock::time_point t0);

struct ProfileAccScope {
	bool enabled = false;
	uint64_t *accum = nullptr;
	std::chrono::steady_clock::time_point t0{};
	explicit ProfileAccScope(bool profile, uint64_t *out_accum);
	~ProfileAccScope();
	ProfileAccScope(const ProfileAccScope &) = delete;
	ProfileAccScope &operator=(const ProfileAccScope &) = delete;
};

void sync_frame_index(SimWorld &world, int64_t index, const UnitState &unit, const SimHostCallbacks *host);
void sync_frame_unit(SimWorld &world, const UnitState &unit, const SimHostCallbacks *host);

} // namespace internal
} // namespace targeting
} // namespace sim

#endif // SIM_TARGETING_INTERNAL_HPP
