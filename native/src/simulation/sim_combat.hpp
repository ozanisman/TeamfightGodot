#ifndef SIM_COMBAT_HPP
#define SIM_COMBAT_HPP

#include "sim_world.hpp"

#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <cstdint>
#include <vector>

namespace sim {
namespace combat {

struct CombatHostHooks {
	void *user_data = nullptr;
	double (*heal_unit)(void *user_data, UnitState &source, UnitState &target, double amount, const StringName &action_kind, bool allow_overheal) = nullptr;
	UnitState *(*select_ally_target)(void *user_data, UnitState &unit) = nullptr;
};

struct ProjectileBuffers {
	std::vector<ProjectileState> &projectiles;
	std::vector<ProjectileState> &active_projectiles;
	std::vector<ProjectileState> &scratch_projectiles;
};

struct ProjectileMatchState {
	int64_t sudden_death_ticks = 0;
	int64_t player_kills = 0;
	int64_t enemy_kills = 0;
};

EffectContext build_context(UnitState &source, UnitState *target, UnitState *target_ally, double damage, const StringName &action_kind);
int effect_bucket_index(const StringName &kind);
const std::vector<EffectRecord> &collect_effects(const SimWorld &world, const UnitState &unit, const StringName &kind);
bool effect_record_contains_opcode(const EffectRecord &effect, EffectOpcode opcode);

void run_post_attack_effects(SimWorld &world, SimHostCallbacks &host, UnitState &source, UnitState &target, double damage, const EffectContext &context);
void run_post_heal_effects(SimWorld &world, SimHostCallbacks &host, UnitState &source, UnitState &target, double heal_amount, double heal_gained, const EffectContext &base_context);
void run_on_takedown_effects(
		SimWorld &world,
		SimHostCallbacks &host,
		UnitState &participant,
		UnitState &victim,
		double damage_dealt,
		bool is_kill,
		const EffectContext &base_context);

bool try_cast_ability(SimWorld &world, SimHostCallbacks &host, const CombatHostHooks &hooks, UnitState &unit, UnitState &target, double distance);
bool try_cast_ultimate(SimWorld &world, SimHostCallbacks &host, const CombatHostHooks &hooks, UnitState &unit, UnitState &target, double distance);
bool start_cast(SimWorld &world, SimHostCallbacks &host, const CombatHostHooks &hooks, UnitState &unit, UnitState &target, double distance, const StringName &action_kind);
void resolve_cast(SimWorld &world, SimHostCallbacks &host, const CombatHostHooks &hooks, UnitState &unit);
void perform_auto_attack(
		SimWorld &world,
		SimHostCallbacks &host,
		const CombatHostHooks &hooks,
		UnitState &unit,
		UnitState &target,
		double distance,
		std::vector<ProjectileState> &projectiles);
void resolve_projectile(SimWorld &world, SimHostCallbacks &host, const CombatHostHooks &hooks, const ProjectileState &projectile);
void update_projectiles(
		SimWorld &world,
		SimHostCallbacks &host,
		const CombatHostHooks &hooks,
		ProjectileBuffers &buffers,
		const ProjectileMatchState &match);

} // namespace combat
} // namespace sim

#endif // SIM_COMBAT_HPP
