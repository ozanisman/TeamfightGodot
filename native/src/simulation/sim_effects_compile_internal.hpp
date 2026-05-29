#ifndef SIM_EFFECTS_COMPILE_INTERNAL_HPP
#define SIM_EFFECTS_COMPILE_INTERNAL_HPP

#include "simulation_types.hpp"
#include "sim_effects_compile.hpp"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>

namespace sim::effects::compile::internal {

bool is_valid_stat_name(const godot::StringName &stat_name);
AoShapeParams parse_aoe_shape_metadata(const godot::Dictionary &params, compile::ParamTracker &tracker);

bool try_fill_damage(EffectRecord &compiled, const godot::StringName &kind, compile::ParamTracker &tracker, const godot::Dictionary &params);
bool try_fill_status(EffectRecord &compiled, const godot::StringName &kind, compile::ParamTracker &tracker, const godot::Dictionary &params);
bool try_fill_aoe(EffectRecord &compiled, const godot::StringName &kind, compile::ParamTracker &tracker, const godot::Dictionary &params);
bool try_fill_spawn(EffectRecord &compiled, const godot::StringName &kind, compile::ParamTracker &tracker, const godot::Dictionary &params);

void fill_compiled_kind_fields(
		EffectRecord &compiled,
		const godot::StringName &kind,
		compile::ParamTracker &tracker,
		const godot::Dictionary &params);

} // namespace sim::effects::compile::internal

#endif // SIM_EFFECTS_COMPILE_INTERNAL_HPP
