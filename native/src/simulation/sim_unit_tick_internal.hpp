#ifndef SIM_UNIT_TICK_INTERNAL_HPP
#define SIM_UNIT_TICK_INTERNAL_HPP

#include "sim_world.hpp"

#include <chrono>
#include <cstdint>

namespace sim {
namespace unit_tick {
namespace internal {

uint64_t profile_elapsed_ns(std::chrono::steady_clock::time_point t0);

struct SimProfileAccScope {
	bool enabled = false;
	uint64_t *accum = nullptr;
	std::chrono::steady_clock::time_point t0{};
	explicit SimProfileAccScope(bool profile_sim, uint64_t *out_accum);
	~SimProfileAccScope();
	SimProfileAccScope(const SimProfileAccScope &) = delete;
	SimProfileAccScope &operator=(const SimProfileAccScope &) = delete;
};

const StringName &sn_player();
const StringName &sn_enemy();
const StringName &sn_on_tick();
const StringName &sn_passive();

void sync_targeting_frame_unit(SimHostCallbacks &host, const UnitState &unit);
void execute_effect(SimHostCallbacks &host, const EffectRecord &effect, EffectContext &context);

} // namespace internal
} // namespace unit_tick
} // namespace sim

#endif // SIM_UNIT_TICK_INTERNAL_HPP
