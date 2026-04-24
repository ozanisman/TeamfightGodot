# balance_and_kit_example.gd
# Reference-only: not autoloaded. Shows how native balance patches, ability kits, and inline
# overrides interact. See TeamfightSimulationCore.set_balance_patches() in the GDExtension.
#
# Flow (high level):
# 1. Base data lives in fixtures/goldens/champion_schema.json (never edited for live tuning).
# 2. fixtures/goldens/balance_patches.json loads at catalog init: stat tweaks + optional kit_id / inline ability blobs.
# 3. fixtures/goldens/ability_kits.json holds named presets ("kits") that can replace ability, ultimate, passive_ids.
#    See fixtures/goldens/ability_kits.example.json for a copy-paste preset (guardian_healer_kit).
# 4. When the patch list changes, the sim rebuilds one cached "effective champion" dict per archetype (fast spawns).
#
# Per-patch merge order for each matching champion:
#   - Apply stat_multipliers / stat_additions on stats (after role stat_mods from code).
#   - If kit_id is set: copy ability / ultimate / passive_ids from that kit (whole replace).
#   - If the patch also has inline ability / ultimate / passive_ids: those replace again (patch wins).
#
# Vanilla meta: pass an empty patch array — effective stats and kits match the catalog + role mods only.

extends RefCounted
# No class_name: reference-only helper; avoids registering a global type for a doc/example file.

## Example: runtime patch list (same shape as JSON objects under balance_patches.json "patches").
static func example_patch_list() -> Array:
	# --- 1) Stat-only patch (like season_1_artillery_adjustment) ---
	var stat_patch := {
		"patch_id": "example_artillery_damage",
		"targets": ["artillery"],
		"roles": [],
		"stat_multipliers": {"attack_damage": 0.95},
		"stat_additions": {},
		"reason": "Comment: lowers auto and ability-scaled damage that uses attack_damage.",
	}

	# --- 2) Patch that applies a named kit from ability_kits.json ---
	# Requires a matching entry under "kits" in fixtures/goldens/ability_kits.json, e.g.:
	#   "guardian_healer_kit": { "ability": { ... }, "ultimate": { ... }, "passive_ids": [...] }
	var kit_patch := {
		"patch_id": "example_guardian_kit_swap",
		"targets": ["guardian"],
		"roles": [],
		"stat_multipliers": {},
		"stat_additions": {},
		"kit_id": "guardian_healer_kit",
		"reason": "Comment: replaces whole ability/ultimate/passive_ids from the preset kit.",
	}

	# --- 3) Inline rework without a kit file (good for generated / experimental variants) ---
	# Effect "kind" and "params" must match what the native sim compiles (see _compile_effect in C++).
	var inline_patch := {
		"patch_id": "example_inline_ability",
		"targets": ["cleric"],
		"roles": [],
		"stat_multipliers": {},
		"stat_additions": {},
		"ability": {
			"kind": "heal",
			"params": {
				"max_hp_ratio": 0.12,
				"current_hp_ratio": 0.0,
				"flat_amount": 0.0,
				"reason": "Example: single-target heal instead of whatever was in schema",
			},
		},
		"reason": "Comment: cleric ability subtree replaced before units spawn; validated at cache rebuild.",
	}

	return [stat_patch, kit_patch, inline_patch]


## Pseudocode: how a headless runner would swap meta between batches.
static func swap_meta_between_batches(core: Object) -> void:
	if core == null or not core.has_method("set_balance_patches"):
		return
	# Batch A — tuned meta (kits + stats + inline).
	core.set_balance_patches(example_patch_list())
	# run_match(...)
	# Batch B — back to file defaults (or true vanilla).
	core.set_balance_patches([])
	# run_match(...)
