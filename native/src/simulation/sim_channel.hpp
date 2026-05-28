#ifndef SIM_CHANNEL_HPP
#define SIM_CHANNEL_HPP

#include "sim_world.hpp"

namespace sim {
namespace channel {

struct ChannelHostHooks {
	void *user_data = nullptr;
	bool (*debug_combat_trace)(void *user_data) = nullptr;
};

void process_channel_tick(SimWorld &world, SimHostCallbacks &host, const ChannelHostHooks &hooks, UnitState &unit, double delta);

} // namespace channel
} // namespace sim

#endif // SIM_CHANNEL_HPP
