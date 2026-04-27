class_name HeadlessRunner
extends RefCounted

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const MatchReplaySummaryScript := preload("res://scripts/simulation/match_replay_summary.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const SimulationBatchWorkerScript := preload("res://scripts/simulation/simulation_batch_worker.gd")
const SimulationSchemaScript := preload("res://scripts/simulation/simulation_schema.gd")
const ParityToolsScript := preload("res://scripts/simulation/parity_tools.gd")
const ReplayIOScript := preload("res://scripts/simulation/replay_io.gd")
const NativeExtensionPath := "res://teamfight_simulation_core.gdextension"
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

static func _build_backend():
	var backend: Object = NativeSimulationBackendScript.new()
	if backend.is_available():
		return backend
	push_error("No simulation backend available.")
	return backend

static func _extract_argument(prefix: String, fallback: String = "") -> String:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return fallback

static func _build_default_input():
	var player_units: Array[StringName] = [&"swordsman"]
	var enemy_units: Array[StringName] = [&"guardian"]
	return MatchReplayInputScript.build_match_input(0, player_units, enemy_units, SimConstantsScript.SIMULATION_TICK_RATE)

static func _parse_int(prefix: String, fallback: int) -> int:
	var value := _extract_argument(prefix, "")
	if value.is_empty():
		return fallback
	return int(value)

static func _load_json_variant(path: String) -> Variant:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		push_error("Failed to open JSON file: %s" % path)
		return null
	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if parsed == null:
		push_error("Failed to parse JSON file: %s" % path)
	return parsed

static func _summary_object(summary_value: Variant):
	if summary_value is Object:
		return summary_value
	if summary_value is Dictionary:
		return MatchReplaySummaryScript.from_dict(Dictionary(summary_value))
	return MatchReplaySummaryScript.new()

static func _parse_float(prefix: String, fallback: float) -> float:
	var value: String = _extract_argument(prefix, "")
	if value.is_empty():
		return fallback
	return float(value)

static func _floats_match(actual: float, expected: float, abs_eps: float, rel_eps: float) -> bool:
	var diff: float = abs(actual - expected)
	if diff <= abs_eps:
		return true
	if rel_eps <= 0.0:
		return false
	var scale: float = maxf(abs(actual), abs(expected))
	if scale <= 0.0:
		return true
	return diff <= rel_eps * scale

static func _payloads_match_with_tolerance(actual_payload: Dictionary, expected_payload: Dictionary, abs_eps: float, rel_eps: float) -> bool:
	# Strict keys must match exactly (these indicate real gameplay divergence).
	for key in ["seed", "winner_team", "player_comp", "enemy_comp"]:
		if actual_payload.get(key, null) != expected_payload.get(key, null):
			return false

	# Duration should be extremely close; allow only tiny absolute epsilon.
	var actual_duration := float(actual_payload.get("duration", 0.0))
	var expected_duration := float(expected_payload.get("duration", 0.0))
	if not _floats_match(actual_duration, expected_duration, 1e-9, 0.0):
		return false

	var actual_units: Array = Array(actual_payload.get("unit_stats", []))
	var expected_units: Array = Array(expected_payload.get("unit_stats", []))
	if actual_units.size() != expected_units.size():
		return false

	for index in range(actual_units.size()):
		var actual_unit: Dictionary = Dictionary(actual_units[index])
		var expected_unit: Dictionary = Dictionary(expected_units[index])

		# Strict per-unit keys.
		for key in ["instance_id", "archetype", "team", "won"]:
			if actual_unit.get(key, null) != expected_unit.get(key, null):
				return false

		# Strict counters (timing/cadence-sensitive).
		for key in ["auto_attacks", "abilities", "ultimates", "stuns", "kills", "deaths", "assists"]:
			if actual_unit.get(key, null) != expected_unit.get(key, null):
				return false

		# Float metrics: allow tolerance to reduce noise from rounding/serialization.
		for key in [
			"damage_dealt",
			"damage_dealt_auto",
			"damage_dealt_ability",
			"damage_dealt_ultimate",
			"damage_received",
			"damage_mitigated",
			"healing_done",
			"healing_done_auto",
			"healing_done_ability",
			"healing_done_ultimate",
			"shielding_done",
			"shielding_done_auto",
			"shielding_done_ability",
			"shielding_done_ultimate",
		]:
			var actual_value := float(actual_unit.get(key, 0.0))
			var expected_value := float(expected_unit.get(key, 0.0))
			if not _floats_match(actual_value, expected_value, abs_eps, rel_eps):
				return false

	return true

static func _compare_fixture_set(backend: Object, fixture_path: String) -> bool:
	var parsed: Variant = _load_json_variant(fixture_path)
	if not (parsed is Dictionary):
		return false

	var fixture_doc: Dictionary = Dictionary(parsed)
	var schema_signature: String = String(fixture_doc.get("schema_signature", ""))
	var fixture_signature: String = String(fixture_doc.get("fixture_signature", ""))
	if int(fixture_doc.get("rules_version", SimConstantsScript.SIMULATION_RULES_VERSION)) != SimConstantsScript.SIMULATION_RULES_VERSION:
		push_error("Fixture rules version mismatch in %s" % fixture_path)
		return false
	if schema_signature == "":
		push_error("Fixture schema signature missing in %s" % fixture_path)
		return false
	var expected_schema_signature: String = ParityToolsScript.hash_payload(SimulationSchemaScript.export_contract_schema())
	if schema_signature != expected_schema_signature:
		push_error("Fixture schema signature mismatch in %s" % fixture_path)
		push_error("Expected %s, got %s" % [expected_schema_signature, schema_signature])
		return false

	var fixtures: Array = Array(fixture_doc.get("fixtures", []))
	if fixture_signature == "":
		push_error("Fixture set signature missing in %s" % fixture_path)
		return false
	var expected_fixture_signature: String = ParityToolsScript.fixture_set_signature(fixtures)
	if fixture_signature != expected_fixture_signature:
		push_error("Fixture set signature mismatch in %s" % fixture_path)
		push_error("Expected %s, got %s" % [expected_fixture_signature, fixture_signature])
		return false
	var float_abs_eps := _parse_float("--parity-float-abs-eps=", 0.01)
	var float_rel_eps := _parse_float("--parity-float-rel-eps=", 0.005)
	var failures: Array = []
	for entry in fixtures:
		if not (entry is Dictionary):
			continue
		var fixture: Dictionary = Dictionary(entry)
		var input_data: Dictionary = Dictionary(fixture.get("input", {}))
		var expected_data: Dictionary = Dictionary(fixture.get("summary", {}))
		var expected_signature: String = String(fixture.get("signature", ""))
		var match_input: Object = MatchReplayInputScript.from_dict(input_data)
		var actual_summary: Object = _summary_object(backend.run_match(match_input))
		var actual_payload: Dictionary = ParityToolsScript.canonical_match_payload(actual_summary)
		var expected_summary: Object = MatchReplaySummaryScript.from_dict(expected_data)
		var expected_payload: Dictionary = ParityToolsScript.canonical_match_payload(expected_summary)
		var actual_signature: String = ParityToolsScript.match_signature(actual_summary)
		var payload_match := _payloads_match_with_tolerance(actual_payload, expected_payload, float_abs_eps, float_rel_eps)
		# Signature mismatch is reported but does not block pass if payload matches within tolerance.
		# Float values that round identically but differ in binary representation would fail an exact
		# hash check even when functionally equivalent (cross-platform/cross-language precision limits).
		if not payload_match:
			var payload_diff: String = _summarize_payload_diff(actual_payload, expected_payload)
			failures.append({
				"name": String(fixture.get("name", "")),
				"expected_signature": expected_signature,
				"actual_signature": actual_signature,
				"payload_diff": payload_diff,
			})
		elif actual_signature != expected_signature:
			print("Fixture signature mismatch (payload OK): %s" % String(fixture.get("name", "")))
		if backend.has_method("clear"):
			backend.call("clear")

	if not failures.is_empty():
		push_error("Fixture parity failed for %d case(s)." % failures.size())
		for failure in failures:
			push_error(JSON.stringify(failure, "\t"))
		return false

	print("Fixture parity passed for %d case(s)." % fixtures.size())
	return true

static func _rewrite_fixture_summaries(backend: Object, fixture_path: String) -> bool:
	var parsed: Variant = _load_json_variant(fixture_path)
	if not (parsed is Dictionary):
		return false
	var fixture_doc: Dictionary = Dictionary(parsed)
	if int(fixture_doc.get("rules_version", SimConstantsScript.SIMULATION_RULES_VERSION)) != SimConstantsScript.SIMULATION_RULES_VERSION:
		push_error("Fixture rules version mismatch in %s" % fixture_path)
		return false
	var fixtures: Array = Array(fixture_doc.get("fixtures", []))
	for index in range(fixtures.size()):
		var entry: Variant = fixtures[index]
		if not (entry is Dictionary):
			continue
		var fixture: Dictionary = Dictionary(entry)
		var input_data: Dictionary = Dictionary(fixture.get("input", {}))
		var match_input: Object = MatchReplayInputScript.from_dict(input_data)
		var actual_summary: Object = _summary_object(backend.run_match(match_input))
		fixture["summary"] = actual_summary.to_dict()
		fixtures[index] = fixture
		if backend.has_method("clear"):
			backend.call("clear")
	fixture_doc["fixtures"] = fixtures
	var out_file := FileAccess.open(fixture_path, FileAccess.WRITE)
	if out_file == null:
		push_error("Failed to write fixture file: %s" % fixture_path)
		return false
	out_file.store_string(JSON.stringify(fixture_doc, "\t", true, true))
	out_file.flush()
	out_file.close()
	print("Rewrote summaries for %d fixtures in %s" % [fixtures.size(), fixture_path])
	return _sign_fixture_set(fixture_path, fixture_path)

static func _sign_fixture_set(fixture_in_path: String, fixture_out_path: String) -> bool:
	var parsed: Variant = _load_json_variant(fixture_in_path)
	if not (parsed is Dictionary):
		return false

	var fixture_doc: Dictionary = Dictionary(parsed)
	var fixtures: Array = Array(fixture_doc.get("fixtures", []))
	if fixtures.is_empty():
		push_error("No fixtures found in %s" % fixture_in_path)
		return false

	# Ensure schema_signature matches the *current* contract (champions + role configs + IO schema).
	var contract_payload: Dictionary = SimulationSchemaScript.export_contract_schema()
	fixture_doc["schema_signature"] = ParityToolsScript.hash_payload(contract_payload)
	fixture_doc["rules_version"] = SimConstantsScript.SIMULATION_RULES_VERSION

	# Recompute per-fixture signatures from the normalized MatchReplaySummary payload.
	for index in range(fixtures.size()):
		var entry: Variant = fixtures[index]
		if not (entry is Dictionary):
			continue
		var fixture: Dictionary = Dictionary(entry)
		var summary_data: Dictionary = Dictionary(fixture.get("summary", {}))
		var summary_obj: Object = MatchReplaySummaryScript.from_dict(summary_data)
		fixture["signature"] = ParityToolsScript.match_signature(summary_obj)
		fixtures[index] = fixture

	fixture_doc["fixtures"] = fixtures
	fixture_doc["fixture_signature"] = ParityToolsScript.fixture_set_signature(fixtures)

	var out_path := fixture_out_path if fixture_out_path != "" else fixture_in_path
	var file := FileAccess.open(out_path, FileAccess.WRITE)
	if file == null:
		push_error("Failed to write signed fixture set: %s" % out_path)
		return false
	file.store_string(JSON.stringify(fixture_doc, "\t", true, true))
	file.flush()
	file.close()
	return true

static func _summarize_payload_diff(actual_payload: Dictionary, expected_payload: Dictionary) -> String:
	var lines: Array[String] = []
	for key in ["seed", "winner_team", "duration", "player_comp", "enemy_comp"]:
		var actual_value: Variant = actual_payload.get(key, null)
		var expected_value: Variant = expected_payload.get(key, null)
		if actual_value != expected_value:
			lines.append("%s: actual=%s expected=%s" % [key, JSON.stringify(actual_value), JSON.stringify(expected_value)])

	var actual_units: Array = Array(actual_payload.get("unit_stats", []))
	var expected_units: Array = Array(expected_payload.get("unit_stats", []))
	var unit_count: int = mini(actual_units.size(), expected_units.size())
	if actual_units.size() != expected_units.size():
		lines.append("unit_stats.count: actual=%d expected=%d" % [actual_units.size(), expected_units.size()])

	for index in range(unit_count):
		var actual_unit: Dictionary = Dictionary(actual_units[index])
		var expected_unit: Dictionary = Dictionary(expected_units[index])
		for key in [
			"instance_id",
			"archetype",
			"team",
			"won",
			"damage_dealt",
			"damage_dealt_auto",
			"damage_dealt_ability",
			"damage_dealt_ultimate",
			"damage_received",
			"damage_mitigated",
			"healing_done",
			"healing_done_auto",
			"healing_done_ability",
			"healing_done_ultimate",
			"shielding_done",
			"shielding_done_auto",
			"shielding_done_ability",
			"shielding_done_ultimate",
			"auto_attacks",
			"abilities",
			"ultimates",
			"stuns",
			"kills",
			"deaths",
			"assists",
		]:
			var actual_value: Variant = actual_unit.get(key, null)
			var expected_value: Variant = expected_unit.get(key, null)
			if actual_value != expected_value:
				lines.append("unit_stats[%d].%s: actual=%s expected=%s" % [index, key, JSON.stringify(actual_value), JSON.stringify(expected_value)])
		if lines.size() >= 24:
			break

	if lines.is_empty():
		return "No top-level payload differences detected."
	return "\n".join(lines)

static func run_from_cli(tree: SceneTree) -> void:
	await tree.process_frame
	var load_status: int = GDExtensionManager.load_extension(NativeExtensionPath)
	if (
		load_status != GDExtensionManager.LOAD_STATUS_OK
		and load_status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED
	):
		push_error("Failed to load native simulation extension: %s" % NativeExtensionPath)
	await tree.process_frame
	var backend: Object = _build_backend()
	await tree.process_frame
	if not backend.is_available():
		push_error("Headless simulation requires a simulation backend.")
		await HeadlessShutdownScript.teardown_extension_then_quit(tree, 1)
		return
	var dump_schema_hash := _extract_argument("--dump-schema-hash=", "")
	var dump_schema_path := _extract_argument("--dump-schema-json=", "")
	var dump_contract_hash := _extract_argument("--dump-contract-hash=", "")
	var dump_contract_path := _extract_argument("--dump-contract-json=", "")
	var sign_fixture_in := _extract_argument("--sign-fixture-in=", "")
	var sign_fixture_out := _extract_argument("--sign-fixture-out=", "")
	if dump_schema_hash != "" or dump_schema_path != "" or dump_contract_hash != "" or dump_contract_path != "":
		if dump_schema_hash != "" or dump_schema_path != "":
			var schema_payload: Dictionary = SimulationSchemaScript.export_champion_schema()
			if dump_schema_path != "":
				var schema_file := FileAccess.open(dump_schema_path, FileAccess.WRITE)
				if schema_file == null:
					push_error("Failed to write schema dump file: %s" % dump_schema_path)
					await HeadlessShutdownScript.teardown_extension_then_quit(tree, 1)
					return
				schema_file.store_string(JSON.stringify(schema_payload, "\t", true, true))
			if dump_schema_hash != "":
				print(ParityToolsScript.hash_payload(schema_payload))
		if dump_contract_hash != "" or dump_contract_path != "":
			var contract_payload: Dictionary = SimulationSchemaScript.export_contract_schema()
			if dump_contract_path != "":
				var contract_file := FileAccess.open(dump_contract_path, FileAccess.WRITE)
				if contract_file == null:
					push_error("Failed to write contract dump file: %s" % dump_contract_path)
					await HeadlessShutdownScript.teardown_extension_then_quit(tree, 1)
					return
				contract_file.store_string(JSON.stringify(contract_payload, "\t", true, true))
			if dump_contract_hash != "":
				print(ParityToolsScript.hash_payload(contract_payload))
		await HeadlessShutdownScript.teardown_extension_then_quit(tree, 0)
		return

	if sign_fixture_in != "":
		var sign_ok := _sign_fixture_set(sign_fixture_in, sign_fixture_out)
		await HeadlessShutdownScript.teardown_extension_then_quit(tree, 0 if sign_ok else 1)
		return
	var rewrite_fixture_path := _extract_argument("--rewrite-fixture-summaries=", "")
	if rewrite_fixture_path != "":
		var rewrite_ok := _rewrite_fixture_summaries(backend, rewrite_fixture_path)
		await HeadlessShutdownScript.teardown_extension_then_quit(tree, 0 if rewrite_ok else 1)
		return
	var input_path := _extract_argument("--match-file=")
	var output_path := _extract_argument("--out=")
	var batch_count := _parse_int("--batch-count=", 1)
	var team_size := _parse_int("--team-size=", 1)
	var seed := _parse_int("--seed=", 0)
	var fixture_file := _extract_argument("--fixture-file=", "")
	var debug_fixture_name := _extract_argument("--debug-fixture-name=", "")
	var match_input: Object = _build_default_input()

	if fixture_file != "":
		if debug_fixture_name != "":
			var parsed: Variant = _load_json_variant(fixture_file)
			if not (parsed is Dictionary):
				await HeadlessShutdownScript.teardown_extension_then_quit(tree, 1)
				return
			var fixture_doc: Dictionary = Dictionary(parsed)
			for entry in Array(fixture_doc.get("fixtures", [])):
				if not (entry is Dictionary):
					continue
				var fixture: Dictionary = Dictionary(entry)
				if String(fixture.get("name", "")) != debug_fixture_name:
					continue
				var input_data: Dictionary = Dictionary(fixture.get("input", {}))
				input_data["record_events"] = true
				input_data["debug_targeting"] = true
				input_data["debug_combat_trace"] = true
				input_data["debug_fixture_name"] = debug_fixture_name
				var match_obj: Object = MatchReplayInputScript.from_dict(input_data)
				backend.run_match(match_obj)
				print("Debug fixture run complete: %s" % debug_fixture_name)
				await HeadlessShutdownScript.teardown_extension_then_quit(tree, 0)
				return
			push_error("Debug fixture name not found: %s" % debug_fixture_name)
			await HeadlessShutdownScript.teardown_extension_then_quit(tree, 1)
			return
		var fixture_ok := _compare_fixture_set(backend, fixture_file)
		await HeadlessShutdownScript.teardown_extension_then_quit(tree, 0 if fixture_ok else 1)
		return
	elif input_path != "":
		match_input = ReplayIOScript.load_input_file(input_path)
	else:
		var player_arg := _extract_argument("--player=", "")
		var enemy_arg := _extract_argument("--enemy=", "")
		if player_arg != "" or enemy_arg != "":
			var players: Array[StringName] = []
			var enemies: Array[StringName] = []
			if player_arg != "":
				for value in player_arg.split(","):
					var trimmed := value.strip_edges()
					if trimmed != "":
						players.append(StringName(trimmed))
			if enemy_arg != "":
				for value in enemy_arg.split(","):
					var trimmed := value.strip_edges()
					if trimmed != "":
						enemies.append(StringName(trimmed))
			if not players.is_empty() or not enemies.is_empty():
				match_input = MatchReplayInputScript.build_match_input(
					seed,
					players,
					enemies,
					float(_extract_argument("--tick-rate=", str(SimConstantsScript.SIMULATION_TICK_RATE)))
				)

	if batch_count > 1:
		var cpu_count: int = maxi(1, OS.get_processor_count())
		var worker_count: int = mini(batch_count, cpu_count)
		var chunk_size: int = int(ceil(float(batch_count) / float(worker_count)))
		var worker_runners: Array = []
		var threads: Array[Thread] = []
		var output_file = null
		if output_path != "":
			output_file = FileAccess.open(output_path, FileAccess.WRITE)
			if output_file == null:
				push_error("Failed to write replay summary batch file: %s" % output_path)
				await HeadlessShutdownScript.teardown_extension_then_quit(tree, 1)
				return
			output_file.store_string("[")
		else:
			print("[")
		for worker_index in range(worker_count):
			var start_index: int = worker_index * chunk_size
			var end_index: int = mini(batch_count, start_index + chunk_size)
			if start_index >= end_index:
				break
			var worker_runner := SimulationBatchWorkerScript.new()
			worker_runners.append(worker_runner)
			var thread := Thread.new()
			threads.append(thread)
			var thread_data := {
				"start_index": start_index,
				"end_index": end_index,
				"team_size": team_size,
				"base_seed": seed,
			}
			var start_error: int = thread.start(Callable(worker_runner, "run_chunk").bind(thread_data))
			if start_error != OK:
				push_error("Failed to start batch worker thread.")
				for started_thread in threads:
					if started_thread is Thread:
						(started_thread as Thread).wait_to_finish()
				if output_file != null:
					output_file.close()
				await HeadlessShutdownScript.teardown_extension_then_quit(tree, 1)
				return
		var wrote_any: bool = false
		for thread in threads:
			var chunk_results: Array = thread.wait_to_finish()
			for entry in chunk_results:
				var summary: Object = MatchReplaySummaryScript.from_dict(Dictionary(entry)) if entry is Dictionary else _summary_object(entry)
				var summary_json: String = ReplayIOScript.serialize_summary(summary)
				if wrote_any:
					if output_file != null:
						output_file.store_string(",")
					else:
						print(",")
				if output_file != null:
					output_file.store_string(summary_json)
				else:
					print(summary_json)
				wrote_any = true
		if output_file != null:
			output_file.store_string("]")
			output_file.flush()
			output_file.close()
			output_file = null
		else:
			print("]")
		threads.clear()
		worker_runners.clear()
	else:
		var summary: Object = _summary_object(backend.run_match(match_input))
		if output_path != "":
			ReplayIOScript.save_summary_file(output_path, summary)
		else:
			print(ReplayIOScript.serialize_summary(summary))
		if backend.has_method("clear"):
			backend.call("clear")

	backend = null
	await HeadlessShutdownScript.teardown_extension_then_quit(tree, 0)
