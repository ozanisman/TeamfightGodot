#include "sim_effects_exec.hpp"
#include "sim_effects_exec_internal.hpp"
#include "sim_effects_compile.hpp"

#include "sim_channel.hpp"
#include "sim_combat.hpp"
#include "sim_constants.hpp"
#include "sim_movement.hpp"
#include "sim_match_spawn.hpp"
#include "sim_stats_modifiers.hpp"
#include "sim_targeting.hpp"
#include "sim_damage.hpp"
#include "sim_periodic.hpp"
#include "sim_stats.hpp"
#include "sim_status.hpp"

#include "stat_definitions.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#define X(name, def, min_val, max_val) using sim::get_effective_##name;
STAT_LIST
#undef X


namespace sim::effects::execution {
namespace internal {

Dictionary exec_spawn(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, UnitState &source, UnitState *target, UnitState *target_ally) {
	switch (effect.opcode) {
		case EFFECT_OPCODE_PROJECTILE: {
			Dictionary projectile_result;
			projectile_result["success"] = false;
			if (target == nullptr) {
				return projectile_result;
			}
			ProjectileState projectile_state;
			projectile_state.source_id = source.instance_id;
			projectile_state.target_id = target->instance_id;
			double damage = get_effective_attack_damage(source) * effect.scalar2;
			
			// Apply ability/ultimate modifiers if applicable
			if (context.action_kind == sn_ability()) {
				damage = sim::damage::apply_ability_modifiers(world, host, source, target, damage);
			} else if (context.action_kind == sn_ultimate()) {
				damage = sim::damage::apply_ultimate_modifiers(world, host, source, target, damage);
			}
			
			projectile_state.damage = damage;
			projectile_state.damage_type = effect.damage_type.is_empty() ? sn_physical() : effect.damage_type;
			projectile_state.stun_duration = effect.scalar3;
			// Python parity: null speed/radius override â†’ fall back to unit's projectile stats.
			double projectile_speed = (effect.scalar0 < 0.0)
				? Math::max(0.0001, get_effective_projectile_speed(source))
				: Math::max(0.0001, effect.scalar0);
			projectile_state.radius = (effect.scalar1 < 0.0)
				? get_effective_projectile_radius(source)
				: effect.scalar1;
			projectile_state.speed = projectile_speed;
			projectile_state.pos_x = source.pos_x;
			projectile_state.pos_y = source.pos_y;
			projectile_state.action_kind = context.action_kind;
			projectile_state.reason = String(effect.reason);
			if (match_host.projectiles != nullptr) {
				match_host.projectiles->push_back(projectile_state);
			}
			projectile_result["success"] = true;
			projectile_result["projectile_created"] = true;
			return projectile_result;
		}
		case EFFECT_OPCODE_SUMMON_ALLY: {
			Dictionary summon_result;
			summon_result["success"] = true;
			summon_result["minions_spawned"] = 0;

			double spawn_radius = effect.scalar0;
			int64_t total_spawned = 0;

			// Copy source data before modifying _units (push_back can reallocate and invalidate references)
			int64_t source_instance_id = source.instance_id;
			double source_pos_x = source.pos_x;
			double source_pos_y = source.pos_y;
			StringName source_team = source.team;

			// Use tracked max instance ID for efficient ID generation
			int64_t next_instance_id = (match_host.max_instance_id != nullptr ? *match_host.max_instance_id : 0) + 1;

			// Max radius for expansion fallback (3x initial radius, capped at 5.0)
			constexpr double max_spawn_radius_expansion = 5.0;
			double max_spawn_radius = Math::min(spawn_radius * 3.0, max_spawn_radius_expansion);

			// Track pending spawn positions to prevent intra-batch overlap
			std::vector<Vector2> pending_positions;

			// Iterate through children (each child is a minion spec with minion_id and count)
			for (const EffectRecord &minion_spec : effect.children) {
				StringName minion_id = StringName(minion_spec.string0);
				int64_t count = minion_spec.int0;

				// Get minion data from minion_catalog
				Dictionary minion_data;
				if (match_host.catalog != nullptr) {
					minion_data = Dictionary(match_host.catalog->minion_catalog.get(String(minion_id), Dictionary()));
				}
				if (minion_data.is_empty()) {
					UtilityFunctions::push_error(vformat("Summon failed: unknown minion archetype: %s", String(minion_id)));
					continue;
				}

			// Spawn count minions of this type
			for (int64_t i = 0; i < count; ++i) {
				// Find random valid position within spawn_radius, with expansion fallback on failure
				Vector2 spawn_pos = sim::match::find_random_spawn_position_near_excluding_with_expansion(
						world,
						host,
						source_pos_x,
						source_pos_y,
						spawn_radius,
						max_spawn_radius,
						source_instance_id,
						pending_positions);
				if (spawn_pos.x < 0.0) {
					// Failed to find valid position even with expansion
					UtilityFunctions::push_error(vformat("Summon failed: could not find valid spawn position for minion %s (%d/%d) near (%.2f, %.2f) with radius %.2f (expanded to %.2f). Active units: %d",
						String(minion_id), i + 1, count, source_pos_x, source_pos_y, spawn_radius, max_spawn_radius, world.units.size()));
					continue;
				}

				// Add to pending positions to prevent overlap with subsequent minions in this batch
				pending_positions.push_back(spawn_pos);

				// Create spawn spec
				Dictionary spawn_spec;
				spawn_spec["archetype_id"] = minion_id;
				spawn_spec["x"] = spawn_pos.x;
				spawn_spec["y"] = spawn_pos.y;

				// Queue the spawn for later processing (at end of tick)
				PendingSpawn pending;
				pending.spawn_spec = spawn_spec;
				pending.team = source_team;
				pending.summoner_instance_id = source_instance_id;
				if (match_host.pending_spawns != nullptr) {
					match_host.pending_spawns->push_back(pending);
				}

				next_instance_id++;
				if (match_host.max_instance_id != nullptr) {
					*match_host.max_instance_id = next_instance_id;
				}
				total_spawned++;
			}
			}

			summon_result["minions_spawned"] = total_spawned;
			return summon_result;
		}
	}
	UtilityFunctions::push_error(vformat("exec_spawn: unhandled opcode %d", effect.opcode));
	Dictionary fallback_result;
	fallback_result["success"] = false;
	fallback_result["error"] = "Unhandled opcode in exec_spawn";
	return fallback_result;
}

} // namespace internal
} // namespace sim::effects::execution
