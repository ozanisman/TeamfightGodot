#include "sim_targeting.hpp"

#include "sim_constants.hpp"

namespace sim {
namespace targeting {

void prepare_tick_context(SimWorld &world, const SimHostCallbacks &host) {
	const size_t n = world.units.size();
	if (world.tick_ctx.density_by_unit_index.size() != n) {
		world.tick_ctx.density_by_unit_index.assign(n, 0);
	}
	world.tick_ctx.player_backliner_indices.clear();
	world.tick_ctx.enemy_backliner_indices.clear();
	world.tick_ctx.player_frontline_indices.clear();
	world.tick_ctx.enemy_frontline_indices.clear();

	double pcx = 0.0;
	double pcy = 0.0;
	int pc = 0;
	for (int64_t idx : world.alive_player_indices) {
		UnitState &u = world.units[static_cast<size_t>(idx)];
		if (!u.alive) {
			continue;
		}
		pcx += u.pos_x;
		pcy += u.pos_y;
		pc += 1;
		if (u.is_marksman_role || u.is_mage_role || u.is_support_role) {
			world.tick_ctx.player_backliner_indices.push_back(idx);
			u.is_backliner = true;
		} else {
			u.is_backliner = false;
		}
		if (u.is_tank_role || u.is_fighter_role) {
			world.tick_ctx.player_frontline_indices.push_back(idx);
		}
	}
	world.tick_ctx.has_player_center = pc > 0;
	if (pc > 0) {
		world.tick_ctx.player_team_center = Vector2(pcx / double(pc), pcy / double(pc));
	}

	double ecx = 0.0;
	double ecy = 0.0;
	int ec = 0;
	for (int64_t idx : world.alive_enemy_indices) {
		UnitState &u = world.units[static_cast<size_t>(idx)];
		if (!u.alive) {
			continue;
		}
		ecx += u.pos_x;
		ecy += u.pos_y;
		ec += 1;
		if (u.is_marksman_role || u.is_mage_role || u.is_support_role) {
			world.tick_ctx.enemy_backliner_indices.push_back(idx);
			u.is_backliner = true;
		} else {
			u.is_backliner = false;
		}
		if (u.is_tank_role || u.is_fighter_role) {
			world.tick_ctx.enemy_frontline_indices.push_back(idx);
		}
	}
	world.tick_ctx.has_enemy_center = ec > 0;
	if (ec > 0) {
		world.tick_ctx.enemy_team_center = Vector2(ecx / double(ec), ecy / double(ec));
	}
	world.tick_ctx.player_backliner_alive_count = int(world.tick_ctx.player_backliner_indices.size());
	world.tick_ctx.enemy_backliner_alive_count = int(world.tick_ctx.enemy_backliner_indices.size());

	for (int64_t idx : world.alive_player_indices) {
		const UnitState &u = world.units[idx];
		if (!u.alive || u.target_id == 0) {
			continue;
		}
		UnitState &live_unit = world.units[static_cast<size_t>(idx)];
		int64_t target_index = target_index_for_unit(world, live_unit);
		if (target_index < 0) {
			continue;
		}
		UnitState &target = world.units[static_cast<size_t>(target_index)];
		if (!target.alive) {
			continue;
		}
		target.incoming_target_count += 1;
	}
	for (int64_t idx : world.alive_enemy_indices) {
		const UnitState &u = world.units[idx];
		if (!u.alive || u.target_id == 0) {
			continue;
		}
		UnitState &live_unit = world.units[static_cast<size_t>(idx)];
		int64_t target_index = target_index_for_unit(world, live_unit);
		if (target_index < 0) {
			continue;
		}
		UnitState &target = world.units[static_cast<size_t>(target_index)];
		if (!target.alive) {
			continue;
		}
		target.incoming_target_count += 1;
	}

	for (int64_t idx : world.alive_player_indices) {
		const UnitState &u = world.units[static_cast<size_t>(idx)];
		if (u.alive) {
			sync_targeting_frame_index(world, idx, u);
		}
	}
	for (int64_t idx : world.alive_enemy_indices) {
		const UnitState &u = world.units[static_cast<size_t>(idx)];
		if (u.alive) {
			sync_targeting_frame_index(world, idx, u);
		}
	}
	(void)host;
}

} // namespace targeting
} // namespace sim
