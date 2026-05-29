#include "../teamfight_simulation_core.hpp"

#include "sim_match_runtime_state.hpp"
#include "sim_viewer.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

void TeamfightSimulationCore::_emit_trace(const StringName &kind, int64_t src_id, int64_t tgt_id, double val) {
	if (!_debug_combat_trace) {
		return;
	}
	if (_trace_buffer.size() >= TRACE_BUFFER_CAP) {
		return;
	}
	sim::TraceEvent ev;
	ev.t = _time;
	ev.kind = kind;
	ev.src = src_id;
	ev.tgt = tgt_id;
	ev.val = val;
	_trace_buffer.push_back(ev);
}

void TeamfightSimulationCore::_print_score_breakdown(const sim::ScoreBreakdown &breakdown, const StringName &attacker_archetype, const StringName &enemy_archetype) const {
	UtilityFunctions::print("[SCORE BREAKDOWN] " + String(attacker_archetype) + " -> " + String(enemy_archetype));
	UtilityFunctions::print("  Distance: " + String::num_real(breakdown.distance) + " (weighted: " + String::num_real(breakdown.distance_weighted) + ")");
	UtilityFunctions::print("  HP Ratio: " + String::num_real(breakdown.hp_ratio) + " (weighted: " + String::num_real(breakdown.hp_weighted) + ")");
	UtilityFunctions::print("  Role Priority: " + String::num_real(breakdown.role_priority));
	UtilityFunctions::print("  Threat: " + String::num_real(breakdown.threat) + " (weighted: " + String::num_real(breakdown.threat_weighted) + ")");
	UtilityFunctions::print("  In-Range Bonus: " + String::num_real(breakdown.in_range_bonus));
	UtilityFunctions::print("  Execute Bonus: " + String::num_real(breakdown.execute_bonus));
	UtilityFunctions::print("  Support Peel: " + String::num_real(breakdown.support_peel));
	UtilityFunctions::print("  TOTAL: " + String::num_real(breakdown.total));
}

Dictionary TeamfightSimulationCore::get_tick_snapshot() const {
	sim::viewer::TickSnapshotInput input;
	input.tick = _tick;
	input.time = _time;
	input.player_kills = _player_kills;
	input.enemy_kills = _enemy_kills;
	input.live_winner = sim::match::determine_winner(_player_kills, _enemy_kills);
	input.units = &_units;
	input.unit_cold = &_unit_cold;
	input.projectiles = &_projectiles;
	input.viewer_fx = &_viewer_fx;
	sim::SimWorld w = _sim_world();
	input.world = &w;
	return sim::viewer::build_tick_snapshot(input);
}

Array TeamfightSimulationCore::get_trace_events() const {
	Array out;
	out.resize(int64_t(_trace_buffer.size()));
	for (int64_t i = 0; i < int64_t(_trace_buffer.size()); ++i) {
		const sim::TraceEvent &e = _trace_buffer[static_cast<size_t>(i)];
		Dictionary d;
		d["t"] = e.t;
		d["kind"] = e.kind;
		d["src"] = e.src;
		d["tgt"] = e.tgt;
		d["val"] = e.val;
		out[i] = d;
	}
	return out;
}
