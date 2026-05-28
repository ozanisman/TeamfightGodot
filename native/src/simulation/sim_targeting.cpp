#include "sim_targeting.hpp"

#include "sim_stats.hpp"
#include "sim_targeting_internal.hpp"

namespace sim {
namespace targeting {

using namespace internal;

UnitState *unit_by_id(SimWorld &world, int64_t instance_id) {
	auto it = world.unit_index_map.find(instance_id);
	if (it == world.unit_index_map.end()) {
		return nullptr;
	}
	int64_t index = it->second;
	if (index < 0 || index >= int64_t(world.units.size())) {
		return nullptr;
	}
	return &world.units[static_cast<size_t>(index)];
}

const UnitState *unit_by_id(const SimWorld &world, int64_t instance_id) {
	auto it = world.unit_index_map.find(instance_id);
	if (it == world.unit_index_map.end()) {
		return nullptr;
	}
	int64_t index = it->second;
	if (index < 0 || index >= int64_t(world.units.size())) {
		return nullptr;
	}
	return &world.units[static_cast<size_t>(index)];
}

int64_t unit_index_by_id(const SimWorld &world, int64_t instance_id) {
	auto it = world.unit_index_map.find(instance_id);
	if (it != world.unit_index_map.end()) {
		return it->second;
	}
	return -1;
}

int64_t target_index_for_unit(SimWorld &world, UnitState &unit) {
	if (unit.target_id == 0) {
		unit.target_index = -1;
		return -1;
	}
	if (unit.target_index >= 0 && unit.target_index < int64_t(world.units.size())) {
		const UnitState &target = world.units[static_cast<size_t>(unit.target_index)];
		if (target.instance_id == unit.target_id) {
			return unit.target_index;
		}
	}
	unit.target_index = unit_index_by_id(world, unit.target_id);
	return unit.target_index;
}

double attack_range(const UnitState &unit) {
	return get_effective_attack_range(unit);
}

double effective_attack_range(const UnitState &unit) {
	double range = attack_range(unit);
	if (range <= RANGED_THRESHOLD) {
		return range + MELEE_CONTACT_BUFFER;
	}
	return range + RANGED_CONTACT_BUFFER;
}

TargetingFrameEntry make_targeting_frame_entry(const UnitState &unit) {
	TargetingFrameEntry frame;
	frame.instance_id = unit.instance_id;
	frame.is_player_team = unit.team == player_team_name();
	frame.role_slot = unit.role_slot;
	frame.is_tank_role = unit.is_tank_role;
	frame.is_fighter_role = unit.is_fighter_role;
	frame.is_assassin_role = unit.is_assassin_role;
	frame.is_marksman_role = unit.is_marksman_role;
	frame.is_mage_role = unit.is_mage_role;
	frame.is_support_role = unit.is_support_role;
	frame.pos_x = unit.pos_x;
	frame.pos_y = unit.pos_y;
	frame.hp = unit.hp;
	frame.max_hp = get_effective_max_hp(unit);
	frame.alive = unit.alive;
	frame.target_id = unit.target_id;
	frame.incoming_target_count = unit.incoming_target_count;
	frame.perceived_threat = unit.perceived_threat;
	frame.stealth_remaining = unit.stealth_remaining;
	return frame;
}

void sync_targeting_frame_index(SimWorld &world, int64_t index, const UnitState &unit) {
	if (index < 0 || index >= int64_t(world.targeting_frame.size())) {
		return;
	}
	TargetingFrameEntry &frame = world.targeting_frame[static_cast<size_t>(index)];
	frame.pos_x = unit.pos_x;
	frame.pos_y = unit.pos_y;
	frame.hp = unit.hp;
	frame.max_hp = get_effective_max_hp(unit);
	frame.alive = unit.alive;
	frame.target_id = unit.target_id;
	frame.incoming_target_count = unit.incoming_target_count;
	frame.perceived_threat = unit.perceived_threat;
	frame.stealth_remaining = unit.stealth_remaining;
}

void sync_targeting_frame_unit(SimWorld &world, const UnitState &unit) {
	sync_targeting_frame_index(world, unit_index_by_id(world, unit.instance_id), unit);
}

} // namespace targeting
} // namespace sim
