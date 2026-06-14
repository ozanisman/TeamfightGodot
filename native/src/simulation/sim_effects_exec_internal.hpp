#ifndef SIM_EFFECTS_EXEC_INTERNAL_HPP
#define SIM_EFFECTS_EXEC_INTERNAL_HPP

#include "sim_effects_host.hpp"
#include "sim_effect_kinds.inl.hpp"
#include "sim_status.hpp"
#include "sim_world.hpp"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::execution {
namespace internal {

using namespace sim::effect_kinds;

inline UnitState *unit_by_id(SimWorld &world, int64_t instance_id) {
	auto it = world.unit_index_map.find(instance_id);
	if (it == world.unit_index_map.end()) {
		return nullptr;
	}
	const int64_t index = it->second;
	if (index < 0 || index >= int64_t(world.units.size())) {
		return nullptr;
	}
	return &world.units[static_cast<size_t>(index)];
}



inline void merge_accumulated_results(Dictionary &target, const Dictionary &source) {
	if (source.is_empty()) {
		return;
	}
	Array keys = source.keys();
	for (int64_t i = 0; i < keys.size(); ++i) {
		Variant key = keys[i];
		Variant source_value = source[key];
		target[key] = source_value;
	}
}

inline bool check_condition(const EffectRecord &effect, const Dictionary &results) {
	if (effect.requires_result_from.is_empty()) {
		return true;  // No condition
	}

	if (!results.has(effect.requires_result_from)) {
		UtilityFunctions::push_error(vformat(
			"Missing requires_result_from '%s' for conditional effect '%s'",
			String(effect.requires_result_from),
			String(effect.reason)
		));
		return false;  // Required effect didn't run
	}

	Dictionary required_result = results[effect.requires_result_from];
	if (!required_result.has(effect.requires_field)) {
		UtilityFunctions::push_error(vformat(
			"Missing requires_field '%s' in result '%s' for conditional effect '%s'",
			String(effect.requires_field),
			String(effect.requires_result_from),
			String(effect.reason)
		));
		return false;  // Field doesn't exist
	}

	Variant actual_value = required_result[effect.requires_field];
	return actual_value == effect.requires_value;
}

inline bool check_target_status_condition(const EffectRecord &effect, const EffectContext &context, SimWorld &world) {
	if (effect.requires_target_status.is_empty()) {
		return true;  // No condition
	}
	
	// Validate status string
	StringName status_to_check = effect.requires_target_status;
	bool is_valid_status = (status_to_check == sn_slow() || status_to_check == sn_root() || 
	                       status_to_check == sn_silence() || status_to_check == sn_disarm() || 
	                       status_to_check == sn_stealth() || status_to_check == sn_stun() || 
	                       status_to_check == sn_reflect());
	if (!is_valid_status) {
		UtilityFunctions::push_error(vformat("Invalid requires_target_status '%s' for effect '%s'. Valid statuses: slow, root, silence, disarm, stealth, stun, reflect", 
			String(status_to_check), String(effect.reason)));
		return false;
	}
	
	UnitState *unit_to_check = nullptr;
	if (effect.status_target == sn_source()) {
		unit_to_check = context.source;
	} else if (effect.status_target == sn_target()) {
		unit_to_check = context.target;
	} else {
		UtilityFunctions::push_error(vformat("Invalid status_target '%s' for effect '%s'", String(effect.status_target), String(effect.reason)));
		return false;
	}
	
	if (unit_to_check == nullptr) {
		return false;
	}
	
	return ::sim::status::target_has_status(world, *unit_to_check, status_to_check);
}

inline bool check_all_conditions(const EffectRecord &effect, const Dictionary &results, const EffectContext &context, SimWorld &world) {
	// Check requires_result_from condition
	if (!check_condition(effect, results)) {
		return false;
	}
	
	// Check requires_target_status condition
	if (!check_target_status_condition(effect, context, world)) {
		return false;
	}
	
	return true;
}

enum class ExecRoute : uint8_t {
	Damage = 0,
	Status = 1,
	Spawn = 2,
	Aoe = 3,
	MultiEffect = 4,
	MultiTarget = 5,
	DefaultOk = 6,
};

ExecRoute exec_route_for_opcode(int64_t opcode);

inline void merge_result(Dictionary &target_result, const Dictionary &source_result) {
	if (source_result.is_empty()) {
		return;
	}
	Array keys = source_result.keys();
	for (int64_t i = 0; i < keys.size(); ++i) {
		Variant key = keys[i];
		Variant source_value = source_result[key];
		if (target_result.has(key) && source_value.get_type() == Variant::FLOAT && target_result[key].get_type() == Variant::FLOAT) {
			target_result[key] = double(target_result[key]) + double(source_value);
		} else if (target_result.has(key) && source_value.get_type() == Variant::INT && target_result[key].get_type() == Variant::INT) {
			target_result[key] = int64_t(target_result[key]) + int64_t(source_value);
		} else {
			target_result[key] = source_value;
		}
	}
}




Dictionary execute_impl(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		const ::sim::effects::SimMatchHost &match_host);

Dictionary execute_recursive(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		const ::sim::effects::SimMatchHost &match_host);

Dictionary exec_damage(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, UnitState &source, UnitState *target, UnitState *target_ally);
Dictionary exec_status(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, UnitState &source, UnitState *target, UnitState *target_ally);
Dictionary exec_status_heal_shield(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, UnitState &source, UnitState *target, UnitState *target_ally);
Dictionary exec_status_mana(const EffectRecord &effect, EffectContext &context, SimWorld &world, UnitState &source, UnitState *target);
Dictionary exec_status_cc(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, UnitState &source, UnitState *target);
Dictionary exec_status_channel(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, UnitState &source, UnitState *target);
Dictionary exec_spawn(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, UnitState &source, UnitState *target, UnitState *target_ally);
Dictionary exec_aoe(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, UnitState &source, UnitState *target, UnitState *target_ally);

} // namespace internal
} // namespace sim::effects::execution

#endif // SIM_EFFECTS_EXEC_INTERNAL_HPP
