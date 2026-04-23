class_name HeadlessRunner
extends RefCounted

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const MatchReplayInputScript := preload("res://scripts/simulation/match_replay_input.gd")
const MatchReplaySummaryScript := preload("res://scripts/simulation/match_replay_summary.gd")
const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const SimulationSchemaScript := preload("res://scripts/simulation/simulation_schema.gd")
const ParityToolsScript := preload("res://scripts/simulation/parity_tools.gd")
const ReplayIOScript := preload("res://scripts/simulation/replay_io.gd")

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

	var fixtures: Array = Array(fixture_doc.get("fixtures", []))
	if fixture_signature == "":
		push_error("Fixture set signature missing in %s" % fixture_path)
		return false
	var failures: Array = []
	for entry in fixtures:
		if not (entry is Dictionary):
			continue
		var fixture: Dictionary = Dictionary(entry)
		var input_data: Dictionary = Dictionary(fixture.get("input", {}))
		var expected_data: Dictionary = Dictionary(fixture.get("summary", {}))
		var expected_signature: String = String(fixture.get("signature", ""))
		var match_input: Object = MatchReplayInputScript.from_dict(input_data)
		var actual_summary: Object = backend.run_match(match_input)
		var actual_payload: Dictionary = ParityToolsScript.canonical_match_payload(actual_summary)
		var expected_summary: Object = MatchReplaySummaryScript.from_dict(expected_data)
		var expected_payload: Dictionary = ParityToolsScript.canonical_match_payload(expected_summary)
		var actual_signature: String = ParityToolsScript.match_signature(actual_summary)
		if actual_payload != expected_payload or actual_signature != expected_signature:
			var payload_diff: String = _summarize_payload_diff(actual_payload, expected_payload)
			failures.append({
				"name": String(fixture.get("name", "")),
				"expected_signature": expected_signature,
				"actual_signature": actual_signature,
				"payload_diff": payload_diff,
			})
		if backend.has_method("clear"):
			backend.call("clear")

	if not failures.is_empty():
		push_error("Fixture parity failed for %d case(s)." % failures.size())
		for failure in failures:
			push_error(JSON.stringify(failure, "\t"))
		return false

	print("Fixture parity passed for %d case(s)." % fixtures.size())
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
			"shielding_done",
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

static func _build_batch_inputs(batch_count: int, base_seed: int, team_size: int):
	var rng := RandomNumberGenerator.new()
	rng.seed = base_seed
	var archetypes: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	var results: Array = []
	results.resize(batch_count)

	for match_index in range(batch_count):
		var match_seed := base_seed + match_index
		var players: Array[StringName] = []
		var enemies: Array[StringName] = []

		if archetypes.size() < team_size * 2:
			for _i in range(team_size):
				players.append(archetypes[rng.randi_range(0, archetypes.size() - 1)])
			for _i in range(team_size):
				enemies.append(archetypes[rng.randi_range(0, archetypes.size() - 1)])
		else:
			var pool := archetypes.duplicate()
			pool.shuffle()
			for i in range(team_size):
				players.append(pool[i])
			for i in range(team_size, team_size * 2):
				enemies.append(pool[i])

		results[match_index] = MatchReplayInputScript.build_match_input(match_seed, players, enemies, SimConstantsScript.SIMULATION_TICK_RATE)

	return results

static func run_from_cli(tree: SceneTree) -> void:
	await tree.process_frame
	var backend: Object = _build_backend()
	await tree.process_frame
	if not backend.is_available():
		push_error("Headless simulation requires a simulation backend.")
		tree.quit(1)
		return
	var dump_schema_hash := _extract_argument("--dump-schema-hash=", "")
	var dump_schema_path := _extract_argument("--dump-schema-json=", "")
	if dump_schema_hash != "" or dump_schema_path != "":
		var schema_payload: Dictionary = SimulationSchemaScript.export_champion_schema()
		if dump_schema_path != "":
			var schema_file := FileAccess.open(dump_schema_path, FileAccess.WRITE)
			if schema_file == null:
				push_error("Failed to write schema dump file: %s" % dump_schema_path)
				tree.quit(1)
				return
			schema_file.store_string(JSON.stringify(schema_payload, "\t", true, true))
		if dump_schema_hash != "":
			print(ParityToolsScript.hash_payload(schema_payload))
		tree.quit()
		return
	var input_path := _extract_argument("--match-file=")
	var output_path := _extract_argument("--out=")
	var batch_count := _parse_int("--batch-count=", 1)
	var team_size := _parse_int("--team-size=", 1)
	var seed := _parse_int("--seed=", 0)
	var fixture_file := _extract_argument("--fixture-file=", "")
	var match_input: Object = _build_default_input()

	if fixture_file != "":
		var fixture_ok := _compare_fixture_set(backend, fixture_file)
		tree.quit(0 if fixture_ok else 1)
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
		var inputs: Array = _build_batch_inputs(batch_count, seed, team_size)
		var summaries: Array = backend.run_matches(inputs)
		if output_path != "":
			ReplayIOScript.save_summary_batch_file(output_path, summaries)
		else:
			print(ReplayIOScript.serialize_summary_batch(summaries))
		if backend.has_method("clear"):
			backend.call("clear")
	else:
		var summary: Object = backend.run_match(match_input)
		if output_path != "":
			ReplayIOScript.save_summary_file(output_path, summary)
		else:
			print(ReplayIOScript.serialize_summary(summary))
		if backend.has_method("clear"):
			backend.call("clear")

	tree.quit()
