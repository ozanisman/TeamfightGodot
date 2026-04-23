#ifndef TEAMFIGHT_SIMULATION_CORE_HPP
#define TEAMFIGHT_SIMULATION_CORE_HPP

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot {

class TeamfightSimulationCore : public RefCounted {
	GDCLASS(TeamfightSimulationCore, RefCounted)

protected:
	static void _bind_methods();

private:
	void _reset_runtime_state();

public:
	TeamfightSimulationCore() = default;
	~TeamfightSimulationCore() override = default;

	bool is_ready() const;
	void clear();
	Dictionary run_match(const Variant &match_input);
	Array run_matches(const Array &match_inputs);
};

} // namespace godot

#endif
