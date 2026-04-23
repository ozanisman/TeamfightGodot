#include "teamfight_simulation_core.hpp"

#include <godot_cpp/core/class_db.hpp>

namespace godot {

void TeamfightSimulationCore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_ready"), &TeamfightSimulationCore::is_ready);
	ClassDB::bind_method(D_METHOD("clear"), &TeamfightSimulationCore::clear);
	ClassDB::bind_method(D_METHOD("run_match", "match_input"), &TeamfightSimulationCore::run_match);
	ClassDB::bind_method(D_METHOD("run_matches", "match_inputs"), &TeamfightSimulationCore::run_matches);
}

void TeamfightSimulationCore::_reset_runtime_state() {
	// Native implementation will own all match-local state here.
}

bool TeamfightSimulationCore::is_ready() const {
	return false;
}

void TeamfightSimulationCore::clear() {
	_reset_runtime_state();
}

Dictionary TeamfightSimulationCore::run_match(const Variant &match_input) {
	Dictionary summary;
	(void)match_input;
	return summary;
}

Array TeamfightSimulationCore::run_matches(const Array &match_inputs) {
	Array summaries;
	summaries.resize(match_inputs.size());
	for (int64_t index = 0; index < match_inputs.size(); ++index) {
		summaries[index] = run_match(match_inputs[index]);
	}
	return summaries;
}

} // namespace godot
