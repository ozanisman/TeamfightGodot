#ifndef SIM_PROFILE_HPP
#define SIM_PROFILE_HPP

#include "sim_profile_counters.hpp"

class TeamfightSimulationCore;

namespace sim::unit_tick {
struct UnitTickProfileCounters;
}

namespace sim::profile {

void reset(TeamfightSimulationCore &core);
void emit_json_stderr(const TeamfightSimulationCore &core);
unit_tick::UnitTickProfileCounters unit_tick_profile(Counters &counters, bool profile_sim);

} // namespace sim::profile

#endif // SIM_PROFILE_HPP
