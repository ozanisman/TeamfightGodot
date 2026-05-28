#include "sim_viewer.hpp"

#include "sim_constants.hpp"
#include "sim_stats.hpp"
#include "sim_stats.inl.hpp"
#include "sim_status.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

namespace sim {
namespace viewer {

void ViewerFxBuffer::clear() {
	events.clear();
}

void ViewerFxBuffer::push(const ViewerFxEvent &ev) {
	if (events.size() >= VIEWER_FX_CAP) {
		return;
	}
	events.push_back(ev);
}

void record_damage_fx(
		ViewerFxBuffer &buffer,
		const UnitState &source,
		const UnitState &target,
		double total_damage,
		const StringName &action_kind,
		const StringName &damage_type) {
	ViewerFxEvent ev;
	ev.kind = StringName("damage");
	ev.target_id = target.instance_id;
	ev.src_id = source.instance_id;
	ev.pos_x = target.pos_x;
	ev.pos_y = target.pos_y;
	ev.val = total_damage;
	ev.damage_type = damage_type;
	buffer.push(ev);
	if (action_kind == StringName("auto") && get_effective_attack_range(source) <= RANGED_THRESHOLD) {
		ViewerFxEvent slash;
		slash.kind = StringName("melee_slash");
		slash.pos_x = target.pos_x;
		slash.pos_y = target.pos_y;
		buffer.push(slash);
	}
}

void record_heal_fx(ViewerFxBuffer &buffer, const UnitState &target, double amount) {
	ViewerFxEvent ev;
	ev.kind = StringName("heal");
	ev.target_id = target.instance_id;
	ev.pos_x = target.pos_x;
	ev.pos_y = target.pos_y;
	ev.val = amount;
	buffer.push(ev);
}

void record_shield_fx(ViewerFxBuffer &buffer, const UnitState &target, double amount) {
	ViewerFxEvent ev;
	ev.kind = StringName("shield");
	ev.target_id = target.instance_id;
	ev.pos_x = target.pos_x;
	ev.pos_y = target.pos_y;
	ev.val = amount;
	buffer.push(ev);
}

void record_aoe_shape_fx(
		ViewerFxBuffer &buffer,
		const SimWorld &world,
		const UnitState &source,
		const UnitState *target,
		const AoShapeParams &params,
		const StringName &kind) {
	ViewerFxEvent ev;
	ev.kind = kind;
	ev.src_id = source.instance_id;
	ev.target_id = target != nullptr ? target->instance_id : params.target_id;

	if (params.anchor == AoAnchorKind::Self) {
		ev.pos_x = source.pos_x;
		ev.pos_y = source.pos_y;
	} else if (params.anchor == AoAnchorKind::Target) {
		const UnitState *resolved = target != nullptr ? target : targeting::unit_by_id(world, params.target_id);
		if (resolved != nullptr) {
			ev.pos_x = resolved->pos_x;
			ev.pos_y = resolved->pos_y;
		} else {
			ev.pos_x = source.pos_x;
			ev.pos_y = source.pos_y;
		}
	} else if (params.anchor == AoAnchorKind::Forward) {
		const Vector2 direction = status::resolve_aoe_direction(world, source, params, target);
		if (params.shape == AoShapeKind::Rectangle) {
			ev.pos_x = source.pos_x + direction.x * params.height * 0.5;
			ev.pos_y = source.pos_y + direction.y * params.height * 0.5;
		} else if (params.shape == AoShapeKind::Cone) {
			ev.pos_x = source.pos_x;
			ev.pos_y = source.pos_y;
		} else if (params.shape == AoShapeKind::Circle) {
			ev.pos_x = source.pos_x + direction.x * params.radius * 0.5;
			ev.pos_y = source.pos_y + direction.y * params.radius * 0.5;
		} else {
			ev.pos_x = source.pos_x;
			ev.pos_y = source.pos_y;
		}
	} else {
		ev.pos_x = params.anchor_x;
		ev.pos_y = params.anchor_y;
	}

	Dictionary shape_dict;
	switch (params.shape) {
		case AoShapeKind::Circle:
			shape_dict["shape"] = "circle";
			break;
		case AoShapeKind::Cone:
			shape_dict["shape"] = "cone";
			break;
		case AoShapeKind::Rectangle:
			shape_dict["shape"] = "rectangle";
			break;
	}

	switch (params.anchor) {
		case AoAnchorKind::Self:
			shape_dict["anchor"] = "self";
			break;
		case AoAnchorKind::Target:
			shape_dict["anchor"] = "target";
			break;
		case AoAnchorKind::Point:
			shape_dict["anchor"] = "point";
			break;
		case AoAnchorKind::Forward:
			shape_dict["anchor"] = "forward";
			break;
	}

	shape_dict["radius"] = params.radius;
	shape_dict["width"] = params.width;
	shape_dict["height"] = params.height;
	shape_dict["rotation_radians"] = params.rotation_radians;
	shape_dict["anchor_x"] = params.anchor_x;
	shape_dict["anchor_y"] = params.anchor_y;
	shape_dict["target_id"] = ev.target_id;
	const Vector2 forward = status::resolve_aoe_direction(world, source, params, target);
	shape_dict["forward_x"] = forward.x;
	shape_dict["forward_y"] = forward.y;

	ev.val = 0.0;
	ev.radius = params.radius;
	ev.extra = shape_dict;
	buffer.push(ev);
}

void record_hot_status_fx(ViewerFxBuffer &buffer, const UnitState &target, double duration, const StringName & /*effect_type*/) {
	ViewerFxEvent ev;
	ev.kind = StringName("hot_status");
	ev.target_id = target.instance_id;
	ev.val = duration;
	ev.radius = 0.0;
	buffer.push(ev);
}

void record_passive_aoe_fx(ViewerFxBuffer &buffer, const UnitState &unit, double radius, const StringName &passive_id) {
	ViewerFxEvent ev;
	ev.kind = StringName("passive_aoe");
	ev.target_id = unit.instance_id;
	ev.src_id = unit.instance_id;
	ev.pos_x = unit.pos_x;
	ev.pos_y = unit.pos_y;
	ev.val = 0.0;
	ev.radius = radius;
	Dictionary extra;
	extra["passive_id"] = String(passive_id);
	ev.extra = extra;
	buffer.push(ev);
}

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
		d["archetype_id"] = String(uc.archetype_id);
		d["hp"] = u.hp;
		d["max_hp"] = get_effective_max_hp(u);
		d["shield"] = u.shield;
		d["mana"] = u.mana;
		d["max_mana"] = get_effective_max_mana(u);
		d["target"] = u.target_id;
		d["target_id"] = u.target_id;
		d["stun"] = u.stun_remaining;
		d["stun_remaining"] = u.stun_remaining;
		d["slow_remaining"] = u.slow_remaining;
		d["root_remaining"] = u.root_remaining;
		d["silence_remaining"] = u.silence_remaining;
		d["disarm_remaining"] = u.disarm_remaining;
		d["stealth_remaining"] = u.stealth_remaining;
		d["reflect_buff_remaining"] = uc.reflect_buffs.empty() ? 0.0 : 1.0;
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
		pd["pos_x"] = p.pos_x;
		pd["pos_y"] = p.pos_y;
		pd["radius"] = p.radius;
		pd["source_id"] = p.source_id;
		pd["target_id"] = p.target_id;
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
