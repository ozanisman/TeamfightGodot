extends RefCounted
class_name WorldState

signal phase_changed(new_phase: int)

enum Phase {
	DRAFTING,
	PREPARATION,
	COMBAT,
	MATCH_OVER,
}

const DEFAULT_TICK_RATE := 0.1
const DEFAULT_WORLD_SIZE := Vector2(10.0, 10.0)

var tick_rate: float = DEFAULT_TICK_RATE
var world_size: Vector2 = DEFAULT_WORLD_SIZE
var phase: Phase = Phase.DRAFTING
var tick: int = 0
var time: float = 0.0


func set_phase(new_phase: Phase) -> void:
	if phase == new_phase:
		return
	phase = new_phase
	phase_changed.emit(new_phase)


func advance_tick() -> void:
	tick += 1
	time = maxf(0.0, time - tick_rate)
