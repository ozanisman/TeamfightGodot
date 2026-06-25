#ifndef TEAMFIGHT_SIMULATION_TEST_RUNNER_HPP
#define TEAMFIGHT_SIMULATION_TEST_RUNNER_HPP

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

class TeamfightSimulationTestRunner : public RefCounted {
	GDCLASS(TeamfightSimulationTestRunner, RefCounted)

protected:
	static void _bind_methods();

public:
	Dictionary run_all();
};

} // namespace godot

#endif // TEAMFIGHT_SIMULATION_TEST_RUNNER_HPP
