#include "sim_periodic.hpp"

#include "sim_periodic_internal.hpp"

#include "sim_viewer.hpp"
#include "sim_combat.hpp"
#include "sim_constants.hpp"
#include "sim_damage.hpp"
#include "sim_movement.hpp"
#include "sim_stats.hpp"
#include "sim_status.hpp"
#include "sim_targeting.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <algorithm>

#define X(name, def, min_val, max_val) using sim::get_effective_##name;
STAT_LIST
#undef X

namespace sim {
namespace periodic {

using namespace internal;

void apply_reflect_buff(
		SimWorld &world,
		UnitState &source,
		UnitState &target,
		double pct,
		double duration,
		const StringName &action_kind,
		const StringName &damage_type,
		const String &reason) {
	(void)action_kind;
	if (duration <= 0.0 || pct <= 0.0) {
		return;
	}
	const double p = Math::clamp(pct, 0.0, 1.0);

	UnitStateCold::ReflectBuff new_buff;
	new_buff.percentage = p;
	new_buff.remaining_duration = duration;
	new_buff.action_kind = action_kind;
	new_buff.source_instance_id = source.instance_id;
	new_buff.damage_type = damage_type;
	new_buff.reason = reason;

	uc(world, target).reflect_buffs.push_back(new_buff);
}

void apply_aoe_reflect_shape(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &source,
		UnitState *target,
		const EffectRecord &effect,
		double pct,
		double duration,
		bool all_damage_types,
		const StringName &action_kind,
		const String &reason) {
	if (effect.aoe_shape_params.radius <= 0.0 || duration <= 0.0 || pct <= 0.0) {
		return;
	}
	record_aoe_shape_fx(host.viewer_hooks, world, source, target, effect, StringName("aoe_reflect"));
	const StringName damage_type = all_damage_types ? StringName("all") : StringName("physical");
	for_each_ally_in_aoe_shape(world, source, target, effect, 0, [&](UnitState &ally) {
		apply_reflect_buff(world, source, ally, pct, duration, action_kind, damage_type, reason);
	});
}


} // namespace periodic
} // namespace sim
