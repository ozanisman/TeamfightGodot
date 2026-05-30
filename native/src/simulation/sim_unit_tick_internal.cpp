#include "sim_unit_tick_internal.hpp"

#include <chrono>

namespace sim {
namespace unit_tick {
namespace internal {

uint64_t profile_elapsed_ns(std::chrono::steady_clock::time_point t0) {
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now() - t0).count();
}

SimProfileAccScope::SimProfileAccScope(bool profile_sim, uint64_t *out_accum)
		: enabled(profile_sim && out_accum != nullptr), accum(out_accum) {
	if (enabled) {
		t0 = std::chrono::steady_clock::now();
	}
}

SimProfileAccScope::~SimProfileAccScope() {
	if (enabled && accum != nullptr) {
		*accum += profile_elapsed_ns(t0);
	}
}

const StringName &sn_player() {
	static const StringName s("player");
	return s;
}

const StringName &sn_enemy() {
	static const StringName s("enemy");
	return s;
}

const StringName &sn_on_tick() {
	static const StringName s("on_tick");
	return s;
}

const StringName &sn_passive() {
	static const StringName s("passive");
	return s;
}

void sync_targeting_frame_unit(SimHostCallbacks &host, const UnitState &unit) {
	if (host.sync_targeting_frame_unit != nullptr) {
		host.sync_targeting_frame_unit(host.user_data, unit);
	}
}

void execute_effect(SimHostCallbacks &host, const EffectRecord &effect, EffectContext &context) {
	if (host.execute_effect != nullptr) {
		host.execute_effect(host, effect, context);
	}
}

} // namespace internal
} // namespace unit_tick
} // namespace sim
