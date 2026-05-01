class_name NativeSimulationBackend
extends RefCounted

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const NativeClassName := "TeamfightSimulationCore"
const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"
## Must match [libraries] windows.* entry in teamfight_simulation_core.gdextension.
const NativeWindowsDllResPath := "res://native/bin/teamfight_simulation_core.dll"

var _backend: Object = null
static var _logged_extension_load_failure: bool = false
static var _logged_native_unavailable: bool = false


static func is_windows_native_dll_file_present() -> bool:
	if OS.get_name() != "Windows":
		return true
	return FileAccess.file_exists(NativeWindowsDllResPath)


## Load GDExtension once when [NativeClassName] is not registered (idempotent). Returns false if load failed.
static func ensure_gdextension_loaded() -> bool:
	if ClassDB.class_exists(NativeClassName) and ClassDB.can_instantiate(NativeClassName):
		return true
	var load_status: int = GDExtensionManager.load_extension(NativeExtensionPath)
	# load_extension returns GDExtensionManager.LoadStatus, not Error. 2 == LOAD_STATUS_ALREADY_LOADED.
	if (
		load_status != GDExtensionManager.LOAD_STATUS_OK
		and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED
	):
		if not _logged_extension_load_failure:
			_logged_extension_load_failure = true
			push_warning(
				"GDExtension load failed (status %s) for %s (e.g. missing DLL or bad binary)."
				% [load_status, NativeExtensionPath]
			)
		return false
	return true


func _try_load_native_extension() -> void:
	ensure_gdextension_loaded()


func _detach_native_summary(result: Variant) -> Variant:
	if result is Dictionary:
		return (result as Dictionary).duplicate(true)
	if result is Array:
		return (result as Array).duplicate(true)
	return result


func _attach_native_only() -> void:
	_try_load_native_extension()
	if ClassDB.class_exists(NativeClassName) and ClassDB.can_instantiate(NativeClassName):
		_backend = ClassDB.instantiate(NativeClassName)
		if _backend != null:
			return
		push_warning("Native simulation core failed to instantiate.")
	if not _logged_native_unavailable:
		_logged_native_unavailable = true
		push_error(
			"Native class %s is unavailable. Build native/bin/teamfight_simulation_core.dll to run the production path."
			% NativeClassName
		)


func _init() -> void:
	_attach_native_only()

func _ensure_native_backend() -> bool:
	if _backend != null:
		return true
	_try_load_native_extension()
	if ClassDB.class_exists(NativeClassName) and ClassDB.can_instantiate(NativeClassName):
		var native_backend: Object = ClassDB.instantiate(NativeClassName)
		if native_backend != null:
			if native_backend.has_method("is_ready") and not bool(native_backend.call("is_ready")):
				native_backend = null
			else:
				_backend = native_backend
				return true
	if not _logged_native_unavailable:
		_logged_native_unavailable = true
		push_error(
			"Native class %s is unavailable. Build native/bin/teamfight_simulation_core.dll to run the production path."
			% NativeClassName
		)
	return false

func is_available() -> bool:
	return _ensure_native_backend()

func clear() -> void:
	if not _ensure_native_backend():
		return
	# Returned summaries from run_match/finish_and_summarize are deep-copied; safe after clear().
	_backend.call("clear")

func run_match(match_input):
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return {}
	return _detach_native_summary(_backend.call("run_match", match_input))

func run_matches(match_inputs: Array):
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return []
	if _backend.has_method("run_matches"):
		return _detach_native_summary(_backend.call("run_matches", match_inputs))
	push_error("Native simulation backend is missing run_matches().")
	return []

func run_match_simulation_only(match_input: Variant) -> void:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return
	if _backend.has_method("run_match_simulation_only"):
		_backend.call("run_match_simulation_only", match_input)
		return
	push_error("Native simulation backend is missing run_match_simulation_only().")

## Runs N full simulations without building summaries. Call from one thread at a time (e.g. bench with --workers=1).
func run_matches_simulation_only(match_inputs: Array) -> void:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return
	if _backend.has_method("run_matches_simulation_only"):
		_backend.call("run_matches_simulation_only", match_inputs)
		return
	push_error("Native simulation backend is missing run_matches_simulation_only().")

## Benchmark-only fast path: native owns deterministic draft generation and skips summary allocation.
func run_generated_matches_simulation_only(base_seed: int, batch_count: int, team_size: int) -> void:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return
	if _backend.has_method("run_generated_matches_simulation_only"):
		_backend.call("run_generated_matches_simulation_only", base_seed, batch_count, team_size)
		return
	push_error("Native simulation backend is missing run_generated_matches_simulation_only().")

## Incremental match API (used by simulation viewer and gameplay loops).
func begin_match(match_input: Variant) -> void:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return
	if _backend.has_method("begin_match"):
		_backend.call("begin_match", match_input)
		return
	push_error("Incremental API requires native backend.")

func advance_one_tick() -> void:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return
	if _backend.has_method("advance_one_tick"):
		_backend.call("advance_one_tick")
		return
	push_error("Incremental API requires native backend.")

func match_ticks_exhausted() -> bool:
	if not _ensure_native_backend():
		return true
	if _backend.has_method("match_ticks_exhausted"):
		return _backend.call("match_ticks_exhausted")
	push_error("Native simulation backend is missing match_ticks_exhausted().")
	return true

func finish_and_summarize() -> Dictionary:
	if not _ensure_native_backend():
		return {}
	if _backend.has_method("finish_and_summarize"):
		var detached: Variant = _detach_native_summary(_backend.call("finish_and_summarize"))
		if detached is Dictionary:
			return detached
		push_error(
			"finish_and_summarize returned non-Dictionary after detach (typeof=%s)"
			% typeof(detached)
		)
		return {}
	push_error("Native simulation backend is missing finish_and_summarize().")
	return {}

func get_tick_snapshot() -> Dictionary:
	if not _ensure_native_backend():
		return {}
	if not _backend.has_method("get_tick_snapshot"):
		return {}
	var snap: Variant = _backend.call("get_tick_snapshot")
	if not snap is Dictionary:
		return {}
	var s: Dictionary = snap
	# Backward compat: older native only exposed id, x, y, target; normalize for viewer.
	var units: Array = Array(s.get("units", []))
	for i in range(units.size()):
		if units[i] is Dictionary:
			var u: Dictionary = units[i]
			if not u.has("instance_id"):
				u["instance_id"] = int(u.get("id", 0))
			if not u.has("pos_x"):
				u["pos_x"] = float(u.get("x", 0.0))
			if not u.has("pos_y"):
				u["pos_y"] = float(u.get("y", 0.0))
			if not u.has("target_id"):
				u["target_id"] = int(u.get("target", 0))
			if not u.has("max_hp"):
				u["max_hp"] = float(u.get("hp", 0.0))
			if not u.has("state"):
				u["state"] = "ALIVE" if bool(u.get("alive", true)) else "DEAD"
	s["units"] = units
	if not s.has("tick_fx"):
		s["tick_fx"] = []
	if not s.has("projectiles"):
		s["projectiles"] = []
	if not s.has("time_remaining"):
		s["time_remaining"] = 0.0
	if not s.has("player_kills"):
		s["player_kills"] = 0
	if not s.has("enemy_kills"):
		s["enemy_kills"] = 0
	if not s.has("live_winner"):
		s["live_winner"] = "draw"
	if not s.has("match_duration"):
		s["match_duration"] = 60.0
	var tfx: Array = Array(s.get("tick_fx", []))
	for i in range(tfx.size()):
		if tfx[i] is not Dictionary:
			continue
		var te: Dictionary = tfx[i]
		var k: String = str(te.get("kind", ""))
		if (k == "aoe_damage" or k == "aoe_taunt" or k == "aoe_splash" or k.begins_with("aoe_")) and not te.has("r"):
			if te.has("radius"):
				te["r"] = float(te.get("radius", 0.0))
		if k == "aoe_splash" and (not te.has("r") or float(te.get("r", 0.0)) <= 0.0):
			te["r"] = SimConstantsScript.VIEWER_AOE_FALLBACK_SPLASH_RADIUS_WORLD
	s["tick_fx"] = tfx
	return s

func get_trace_events() -> Array:
	if not _ensure_native_backend():
		return []
	if not _backend.has_method("get_trace_events"):
		return []
	var trace_events: Variant = _backend.call("get_trace_events")
	if trace_events is Array:
		return trace_events
	return []
