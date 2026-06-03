#include "sim_viewer.hpp"

#include "sim_constants.hpp"
#include "sim_stats.hpp"
#include "sim_stats.inl.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim {
namespace viewer {

String viewer_state_string(const UnitState &unit, const UnitStateCold &cold) {
	if (!unit.alive) {
		return String("DEAD");
	}
	if (unit.stun_remaining > 0.0) {
		return String("STUNNED");
	}
	if (unit.root_remaining > 0.0) {
		return String("ROOTED");
	}
	if (unit.silence_remaining > 0.0 && (unit.silence_blocks_abilities || unit.silence_blocks_ultimates)) {
		return String("SILENCED");
	}
	if (!cold.reflect_buffs.empty()) {
		return String("REFLECTING");
	}
	if (unit.disarm_remaining > 0.0) {
		return String("DISARMED");
	}
	if (unit.slow_remaining > 0.0) {
		return String("SLOWED");
	}
	if (cold.is_channeling) {
		return String("CHANNELING");
	}
	if (unit.casting_remaining > 0.0) {
		return String("CASTING");
	}
	if (unit.last_kite_timer > 0.0) {
		return String("KITING");
	}
	return String("ALIVE");
}

Dictionary build_tick_snapshot(const TickSnapshotInput &input) {
	Dictionary root;
	root["tick"] = input.tick;
	root["time"] = input.time;
	root["match_duration"] = MATCH_DURATION;
	root["time_remaining"] = Math::max(0.0, MATCH_DURATION - input.time);
	root["player_kills"] = input.player_kills;
	root["enemy_kills"] = input.enemy_kills;
	root["live_winner"] = String(input.live_winner);

	if (input.units == nullptr || input.unit_cold == nullptr || input.projectiles == nullptr || input.viewer_fx == nullptr ||
			input.world == nullptr) {
		return root;
	}

	const std::vector<UnitState> &units = *input.units;
	const std::vector<UnitStateCold> &unit_cold = *input.unit_cold;
	const SimWorld &world = *input.world;

	Array units_arr;
	for (size_t i = 0; i < units.size(); ++i) {
		const UnitState &u = units[i];
		const UnitStateCold &uc = unit_cold[i];
		Dictionary d;
		d["id"] = u.instance_id;
		d["instance_id"] = u.instance_id;
		d["x"] = u.pos_x;
		d["y"] = u.pos_y;
		d["pos_x"] = u.pos_x;
		d["pos_y"] = u.pos_y;
		d["team"] = String(u.team);
		d["unit_id"] = String(uc.unit_id);
		d["hp"] = u.hp;
		d["max_hp"] = get_effective_max_hp(u);
		d["shield"] = u.shield;
		d["mana"] = u.mana;
		d["mana_cost"] = get_effective_mana_cost(u);
		d["target"] = u.target_id;
		d["target_id"] = u.target_id;
		d["stun"] = u.stun_remaining;
		d["stun_remaining"] = u.stun_remaining;
		d["slow_remaining"] = u.slow_remaining;
		d["root_remaining"] = u.root_remaining;
		d["silence_remaining"] = u.silence_remaining;
		d["disarm_remaining"] = u.disarm_remaining;
		d["stealth_remaining"] = u.stealth_remaining;
		d["reflect_remaining"] = uc.reflect_buffs.empty() ? 0.0 : 1.0;
		d["alive"] = u.alive;
		d["state"] = viewer_state_string(u, uc);
		d["acd"] = u.attack_cooldown;
		d["abi"] = u.ability_cooldown;
		d["attack_cooldown"] = u.attack_cooldown;
		d["attack_period"] = u.attack_period;
		d["attack_range"] = get_effective_attack_range(u);
		d["attack_speed"] = get_effective_attack_speed(u);
		d["ability_cd_max"] = get_effective_ability_cd(u);
		d["attack_damage"] = get_effective_attack_damage(u);
		d["move_speed"] = get_effective_move_speed(u);
		d["armor"] = get_effective_armor(u);
		d["magic_resist"] = get_effective_magic_resist(u);
		d["tenacity"] = get_effective_tenacity(u);
		d["life_steal"] = get_effective_life_steal(u);
		d["casting_remaining"] = u.casting_remaining;
		d["casting_kind"] = String(uc.casting_kind);
		d["is_channeling"] = uc.is_channeling;
		d["channel_remaining_duration"] = uc.channel_remaining_duration;
		d["channel_action_kind"] = String(uc.channel_action_kind);

		bool in_range = false;
		if (u.target_id > 0) {
			const UnitState *target = targeting::unit_by_id(world, u.target_id);
			if (target != nullptr && target->alive) {
				const double distance = distance_between_coords(u.pos_x, u.pos_y, target->pos_x, target->pos_y);
				d["target_distance"] = distance;
				const double effective_range = targeting::effective_attack_range(u);
				in_range = distance <= effective_range;
			}
		} else {
			d["target_distance"] = -1.0;
		}
		d["in_range"] = in_range;
		d["kills"] = uc.kills;
		d["deaths"] = uc.deaths;
		d["assists"] = uc.assists;
		d["respawn_timer"] = u.respawn_timer;
		d["taunt_remaining"] = u.taunt_remaining;
		d["damage_dealt"] = uc.damage_dealt;
		d["healing_done"] = uc.healing_done;
		d["damage_mitigated"] = uc.damage_mitigated;
		d["role"] = String(uc.role_id);
		units_arr.append(d);
	}
	root["units"] = units_arr;

	Array projs;
	for (int64_t i = 0; i < int64_t(input.projectiles->size()); ++i) {
		const ProjectileState &p = (*input.projectiles)[static_cast<size_t>(i)];
		Dictionary pd;
		pd["id"] = i;
		pd["projectile_id"] = p.projectile_id;
		pd["pos_x"] = p.pos_x;
		pd["pos_y"] = p.pos_y;
		pd["radius"] = p.radius;
		pd["source_id"] = p.source_id;
		pd["target_id"] = p.target_id;
		pd["reason"] = p.reason;
		pd["action_kind"] = String(p.action_kind);
		pd["visual_id"] = String(p.visual_id);
		pd["motion"] = String(p.motion);
		pd["collision"] = String(p.collision);
		const UnitState *src = targeting::unit_by_id(world, p.source_id);
		if (src != nullptr) {
			pd["team"] = String(src->team);
		} else {
			pd["team"] = String("player");
		}
		projs.append(pd);
	}
	root["projectiles"] = projs;

	Array fx;
	for (const ViewerFxEvent &ve : input.viewer_fx->events) {
		Dictionary e;
		e["kind"] = String(ve.kind);
		e["target_id"] = ve.target_id;
		e["src_id"] = ve.src_id;
		e["x"] = ve.pos_x;
		e["y"] = ve.pos_y;
		e["val"] = ve.val;
		if (ve.radius > 0.0) {
			const double r = ve.radius;
			e["r"] = r;
			e["radius"] = r;
		}
		if (!ve.damage_type.is_empty()) {
			e["damage_type"] = String(ve.damage_type);
		}
		if (ve.extra.get_type() != Variant::NIL) {
			e["extra"] = ve.extra;
		}
		fx.append(e);
	}
	root["tick_fx"] = fx;
	return root;
}

} // namespace viewer
} // namespace sim
