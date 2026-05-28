#ifndef SIM_PROFILE_HPP
#define SIM_PROFILE_HPP

class TeamfightSimulationCore;

namespace sim::profile {

void reset(TeamfightSimulationCore &core);
void emit_json_stderr(const TeamfightSimulationCore &core);

} // namespace sim::profile

#endif // SIM_PROFILE_HPP
