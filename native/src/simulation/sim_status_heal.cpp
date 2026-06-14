#include "sim_status.hpp"
#include "sim_status_internal.hpp"

#include "sim_stats.hpp"
#include <godot_cpp/core/math.hpp>

namespace sim {
namespace status {

using namespace internal;

void add_shield(SimWorld &world, UnitState &source, UnitState &target, double amount, const StringName &action_kind, SimHostCallbacks *host) {
	if (amount <= 0.0) {
		return;
	}
	target.shield += amount;
	record_shielding_by_action_kind(uc(world, source), amount, action_kind);
	record_benefactor(world, source, target);
	if (amount > 1e-9 && host != nullptr && host->viewer_record_shield_fx != nullptr) {
		host->viewer_record_shield_fx(host->user_data, target, amount);
	}
}

double heal_unit(SimWorld &world, UnitState &source, UnitState &target, double amount, const StringName &action_kind, bool allow_overheal, SimHostCallbacks *host) {
	if (amount <= 0.0) {
		return 0.0;
	}

	const double max_hp = target.stats_dirty ? get_effective_max_hp(target) : target.cached_max_hp;
	const double old_hp = target.hp;
	const double new_hp = allow_overheal ? old_hp + amount : Math::min(max_hp, old_hp + amount);

	target.hp = new_hp;
	const double gained = new_hp - old_hp;

	if (gained > 1e-9) {
		record_healing_by_action_kind(uc(world, source), gained, action_kind);
		record_benefactor(world, source, target);
		if (host != nullptr && host->viewer_record_heal_fx != nullptr) {
			host->viewer_record_heal_fx(host->user_data, target, gained);
		}
		if (host != nullptr && host->sync_targeting_frame_unit != nullptr) {
			host->sync_targeting_frame_unit(host->user_data, target);
		}
	}
	return gained;
}

void restore_mana(SimWorld &world, UnitState &source, UnitState &target, double amount) {
	(void)source;
	(void)world;
	if (amount <= 0.0) {
		return;
	}
	const double mana_cost = target.stats_dirty ? get_effective_mana_cost(target) : target.cached_mana_cost;
	target.mana = Math::min(mana_cost, target.mana + amount);
}

} // namespace status
} // namespace sim
