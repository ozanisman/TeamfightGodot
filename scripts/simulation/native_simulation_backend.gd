class_name NativeSimulationBackend
extends SimulationBackend

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

func run_match_stats(match_input):
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return {}
	if _backend.has_method("run_match_stats"):
		var compact_result: Variant = _backend.call("run_match_stats", match_input)
		if compact_result is Dictionary:
			return compact_result
		return {}
	push_error("Native simulation backend is missing run_match_stats().")
	return {}

func run_matches(match_inputs: Array):
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return []
	if _backend.has_method("run_matches"):
		return _detach_native_summary(_backend.call("run_matches", match_inputs))
	push_error("Native simulation backend is missing run_matches().")
	return []

func run_matches_stats(match_inputs: Array):
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return []
	if _backend.has_method("run_matches_stats"):
		return _detach_native_summary(_backend.call("run_matches_stats", match_inputs))
	push_error("Native simulation backend is missing run_matches_stats().")
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

## Stats generation fast path: native owns deterministic draft generation and aggregates stats.
func run_generated_matches_stats_partial(base_seed: int, batch_count: int, team_size: int, include_match_log: bool, tick_rate: float = SimConstantsScript.DEFAULT_TICK_RATE) -> Dictionary:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return {}
	if _backend.has_method("run_generated_matches_stats_partial"):
		var result: Variant = _backend.call("run_generated_matches_stats_partial", base_seed, batch_count, team_size, include_match_log, tick_rate)
		if result is Dictionary:
			return result
		return {}
	push_error("Native simulation backend is missing run_generated_matches_stats_partial().")
	return {}

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

func _snapshot_mismatch(scope: String, field: String, expected: String, payload: Variant) -> void:
	push_error(
		"Native snapshot mismatch [%s] field=%s expected=%s payload=%s"
		% [scope, field, expected, JSON.stringify(payload, "\t")]
	)


func _require_snapshot_array(snapshot: Dictionary, field: String, scope: String) -> Array:
	if not snapshot.has(field):
		_snapshot_mismatch(scope, field, "Array", snapshot)
		return []
	var value: Variant = snapshot.get(field)
	if value is Array:
		return value
	_snapshot_mismatch(scope, field, "Array", value)
	return []


func _validate_snapshot_unit(unit_data: Dictionary, index: int) -> bool:
	var scope: String = "snapshot.units[%d]" % index
	var required_fields: Array[String] = [
		"instance_id",
		"unit_id",
		"team",
		"pos_x",
		"pos_y",
		"hp",
		"max_hp",
		"shield",
		"mana",
		"mana_cost",
		"alive",
		"state",
		"target_id",
		"attack_cooldown",
		"abi",
		"attack_speed",
		"casting_remaining",
		"casting_kind",
		"stun_remaining",
		"slow_remaining",
		"root_remaining",
		"silence_remaining",
		"disarm_remaining",
		"stealth_remaining",
		"reflect_remaining",
		"kills",
		"deaths",
		"assists",
		"respawn_timer",
		"taunt_remaining",
		"damage_dealt",
		"healing_done",
		"damage_mitigated",
		"attack_range",
		"in_range",
	]
	for field in required_fields:
		if not unit_data.has(field):
			_snapshot_mismatch(scope, field, "present", unit_data)
			return false
	return true


func _validate_snapshot_projectile(projectile_data: Dictionary, index: int) -> bool:
	var scope: String = "snapshot.projectiles[%d]" % index
	var required_fields: Array[String] = [
		"id",
		"pos_x",
		"pos_y",
		"radius",
		"source_id",
		"target_id",
		"team",
		"reason",
		"action_kind",
		"visual_id",
		"motion",
		"collision",
	]
	for field in required_fields:
		if not projectile_data.has(field):
			_snapshot_mismatch(scope, field, "present", projectile_data)
			return false
	return true


func _validate_snapshot_fx(fx_data: Dictionary, index: int) -> bool:
	var scope: String = "snapshot.tick_fx[%d]" % index
	var required_fields: Array[String] = ["kind", "target_id", "src_id", "x", "y", "val"]
	for field in required_fields:
		if not fx_data.has(field):
			_snapshot_mismatch(scope, field, "present", fx_data)
			return false
	return true


func get_tick_snapshot() -> Dictionary:
	if not _ensure_native_backend():
		return {}
	if not _backend.has_method("get_tick_snapshot"):
		return {}
	var snap: Variant = _backend.call("get_tick_snapshot")
	if not snap is Dictionary:
		_snapshot_mismatch("snapshot", "root", "Dictionary", snap)
		return {}
	var s: Dictionary = snap
	var required_top_level_fields: Array[String] = [
		"time",
		"match_duration",
		"time_remaining",
		"player_kills",
		"enemy_kills",
		"live_winner",
		"units",
		"projectiles",
		"tick_fx",
	]
	for field in required_top_level_fields:
		if not s.has(field):
			_snapshot_mismatch("snapshot", field, "present", s)
			return {}
	var units: Array = _require_snapshot_array(s, "units", "snapshot")
	for i in range(units.size()):
		var unit_variant: Variant = units[i]
		if unit_variant is not Dictionary:
			_snapshot_mismatch("snapshot.units[%d]" % i, "entry", "Dictionary", unit_variant)
			return {}
		if not _validate_snapshot_unit(unit_variant as Dictionary, i):
			return {}
	var projectiles: Array = _require_snapshot_array(s, "projectiles", "snapshot")
	for j in range(projectiles.size()):
		var projectile_variant: Variant = projectiles[j]
		if projectile_variant is not Dictionary:
			_snapshot_mismatch("snapshot.projectiles[%d]" % j, "entry", "Dictionary", projectile_variant)
			return {}
		if not _validate_snapshot_projectile(projectile_variant as Dictionary, j):
			return {}
	var tfx: Array = _require_snapshot_array(s, "tick_fx", "snapshot")
	for k in range(tfx.size()):
		var fx_variant: Variant = tfx[k]
		if fx_variant is not Dictionary:
			_snapshot_mismatch("snapshot.tick_fx[%d]" % k, "entry", "Dictionary", fx_variant)
			return {}
		if not _validate_snapshot_fx(fx_variant as Dictionary, k):
			return {}
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


func get_draft_recommendation_names(allies: Array, enemies: Array, available: Array, top_n: int = 3, stats_dir: String = "res://stats_output", base_weight: float = 0.50, synergy_weight: float = 0.25, counter_weight: float = 0.25) -> Array:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return []
	if _backend.has_method("get_draft_recommendation_names"):
		return _backend.call("get_draft_recommendation_names", allies, enemies, available, top_n, stats_dir, base_weight, synergy_weight, counter_weight)
	push_error("Native simulation backend is missing get_draft_recommendation_names().")
	return []


func get_draft_recommendations_with_breakdowns(allies: Array, enemies: Array, available: Array, top_n: int = 3, stats_dir: String = "res://stats_output", base_weight: float = 0.50, synergy_weight: float = 0.25, counter_weight: float = 0.25, draft_position: int = 0, early_pick_base_weight: float = 0.7, late_pick_counter_weight: float = 0.4) -> Array:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return []
	if _backend.has_method("get_draft_recommendations_with_breakdowns"):
		return _backend.call("get_draft_recommendations_with_breakdowns", allies, enemies, available, top_n, stats_dir, base_weight, synergy_weight, counter_weight, 1.2, 1.2, draft_position, early_pick_base_weight, late_pick_counter_weight)
	push_error("Native simulation backend is missing get_draft_recommendations_with_breakdowns().")
	return []


func debug_print_draft_recommendations(allies: Array, enemies: Array, available: Array, top_n: int = 5, stats_dir: String = "res://stats_output", base_weight: float = 0.50, synergy_weight: float = 0.25, counter_weight: float = 0.25, debug_mode: bool = false) -> void:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return
	if _backend.has_method("debug_print_draft_recommendations"):
		_backend.call("debug_print_draft_recommendations", allies, enemies, available, top_n, stats_dir, base_weight, synergy_weight, counter_weight, debug_mode)
		return
	push_error("Native simulation backend is missing debug_print_draft_recommendations().")


func run_debug_draft_evaluation_batch(allies: Array, enemies: Array, available: Array, num_runs: int = 50, stats_dir: String = "res://stats_output", base_weight: float = 0.50, synergy_weight: float = 0.25, counter_weight: float = 0.25) -> void:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return
	if _backend.has_method("run_debug_draft_evaluation_batch"):
		_backend.call("run_debug_draft_evaluation_batch", allies, enemies, available, num_runs, stats_dir, base_weight, synergy_weight, counter_weight)
		return
	push_error("Native simulation backend is missing run_debug_draft_evaluation_batch().")


func predict_draft_winner(team1: Array, team2: Array, stats_dir: String = "res://stats_output", base_weight: float = 0.50, synergy_weight: float = 0.25, counter_weight: float = 0.25, matchup_weight: float = 0.25, composition_weight: float = 0.0, logistic_k: float = 10.0, include_breakdown: bool = false, synergy_amplification: float = 1.2, matchup_amplification: float = 1.2, logit_sharpness: float = 1.5, score_sharpness: float = 1.0, interaction_weight: float = 0.0, scoring_mode: int = 3, variance_weight: float = 0.0, cc_weight: float = 0.0, mobility_weight: float = 0.0, sustain_weight: float = 0.0, best_counter_weight: float = 0.0, worst_counter_weight: float = 0.0, best_synergy_weight: float = 0.0, worst_synergy_weight: float = 0.0, synergy_aggregation: int = 0, counter_aggregation: int = 0, use_decorrelated_scoring: bool = false, draft_position: int = 0, early_pick_base_weight: float = 0.7, late_pick_counter_weight: float = 0.4) -> Dictionary:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return {"team1_prob": 0.5, "team2_prob": 0.5}
	if _backend.has_method("predict_draft_winner"):
		return _backend.call("predict_draft_winner", team1, team2, stats_dir, base_weight, synergy_weight, counter_weight, matchup_weight, composition_weight, logistic_k, include_breakdown, synergy_amplification, matchup_amplification, logit_sharpness, score_sharpness, interaction_weight, scoring_mode, variance_weight, cc_weight, mobility_weight, sustain_weight, best_counter_weight, worst_counter_weight, best_synergy_weight, worst_synergy_weight, synergy_aggregation, counter_aggregation, use_decorrelated_scoring, draft_position, early_pick_base_weight, late_pick_counter_weight)
	push_error("Native simulation backend is missing predict_draft_winner().")
	return {"team1_prob": 0.5, "team2_prob": 0.5}


func predict_draft_winner_hybrid(team1: Array, team2: Array, stats_dir: String = "res://stats_output") -> Dictionary:
	if not _ensure_native_backend():
		push_error("Simulation backend is not available.")
		return {"team1_prob": 0.5, "team2_prob": 0.5, "model": "error"}
	if _backend.has_method("predict_draft_winner_hybrid"):
		return _backend.call("predict_draft_winner_hybrid", team1, team2, stats_dir)
	push_error("Native simulation backend is missing predict_draft_winner_hybrid().")
	return {"team1_prob": 0.5, "team2_prob": 0.5, "model": "error"}
