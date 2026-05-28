#ifndef SIM_EFFECTS_COMPILE_HPP
#define SIM_EFFECTS_COMPILE_HPP

#include "simulation_types.hpp"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <vector>

namespace sim::effects::compile {

struct ParamTracker {
	Dictionary params;
	mutable std::vector<String> accessed;
	String reason;

	explicit ParamTracker(const Dictionary &p) : params(p) {}

	Variant get(const String &key, const Variant &default_value) {
		accessed.push_back(key);
		return params.get(key, default_value);
	}

	void mark_accessed(const String &key) {
		accessed.push_back(key);
	}

	void report_unused(const String &effect_kind) const;
};

int64_t opcode_for_kind(const StringName &kind);
const StringName &kind_for_opcode(int64_t opcode);

EffectRecord compile_effect(const Dictionary &effect);
std::vector<EffectRecord> compile_effect_array(const Array &effects);

} // namespace sim::effects::compile

#endif // SIM_EFFECTS_COMPILE_HPP
