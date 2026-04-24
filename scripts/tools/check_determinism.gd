extends SceneTree

const NativeClassName := "TeamfightSimulationCore"
const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const MatchReplaySummaryScript := preload("res://scripts/simulation/match_replay_summary.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const ParityToolsScript := preload("res://scripts/simulation/parity_tools.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

func _init() -> void:
	call_deferred("_probe_determinism")

func _summary_object(summary_value: Variant):
	if summary_value is Object:
		return summary_value
	if summary_value is Dictionary:
		return MatchReplaySummaryScript.from_dict(Dictionary(summary_value))
	return MatchReplaySummaryScript.new()

func _load_fixture_inputs() -> Array:
	var fixture_path: String = "res://fixtures/goldens/match_fixtures.json"
	var file := FileAccess.open(fixture_path, FileAccess.READ)
	if file == null:
		push_error("Failed to open fixture file: %s" % fixture_path)
		return []
	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if not (parsed is Dictionary):
		push_error("Failed to parse fixture file: %s" % fixture_path)
		return []
	var fixture_doc: Dictionary = Dictionary(parsed)
	var fixtures: Array = Array(fixture_doc.get("fixtures", []))
	var selected_names: Array[String] = [
		"duel_swordsman_vs_guardian",
		"duel_archer_vs_guardian",
		"support_clash",
		"backline_skirmish",
		"mixed_5v5",
	]
	var selected: Array = []
	for fixture_entry in fixtures:
		if not (fixture_entry is Dictionary):
			continue
		var fixture: Dictionary = Dictionary(fixture_entry)
		var fixture_name: String = String(fixture.get("name", ""))
		if selected_names.has(fixture_name):
			selected.append(fixture.get("input", {}))
	return selected

func _probe_determinism() -> void:
	var extension_path: String = ProjectSettings.globalize_path(NativeExtensionPath)
	var load_status: int = GDExtensionManager.load_extension(extension_path)
	if load_status != GDExtensionManager.LOAD_STATUS_OK and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED:
		push_error("Failed to load %s (status %d)" % [NativeExtensionPath, load_status])
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	await process_frame
	var failures: Array = []
	var fixture_inputs: Array = _load_fixture_inputs()
	if fixture_inputs.is_empty():
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	for fixture_input_value in fixture_inputs:
		var fixture_input: Dictionary = Dictionary(fixture_input_value)
		var match_input: Object = MatchReplayInputScript.from_dict(fixture_input)
		var backend_a = NativeSimulationBackendScript.new()
		var backend_b = NativeSimulationBackendScript.new()
		var summary_a = _summary_object(backend_a.run_match(match_input))
		var summary_b = _summary_object(backend_b.run_match(match_input))
		var payload_a: Dictionary = ParityToolsScript.canonical_match_payload(summary_a)
		var payload_b: Dictionary = ParityToolsScript.canonical_match_payload(summary_b)
		var signature_a: String = ParityToolsScript.match_signature(summary_a)
		var signature_b: String = ParityToolsScript.match_signature(summary_b)
		if payload_a != payload_b or signature_a != signature_b:
			failures.append({
				"seed": int(fixture_input.get("seed", 0)),
				"signature_a": signature_a,
				"signature_b": signature_b,
			})

	if not failures.is_empty():
		push_error("Replay determinism failed for %d case(s)." % failures.size())
		for failure in failures:
			push_error(JSON.stringify(failure, "\t"))
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return

	print("Replay determinism passed for %d case(s)." % fixture_inputs.size())
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)
