# Balance Patches

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_catalog`

Versioned overrides for champion stats and abilities.

Balance patches defined in balance_patches.json with: targets (archetype list, empty = all), roles (role list, empty = all), stat_multipliers (stat → multiplier), stat_additions (stat → addition), kit_id (swap ability/ultimate/passives), ability_override, ultimate_override, passive_ids_override.

Applied via balance_version string in MatchReplayInput. Patch applies if unit_id in targets OR role in roles. Stat modifications override base champion stats. Kit swaps replace entire ability/ultimate/passive definitions.

Balance patches enable rapid iteration without modifying champion_catalog.gd. Used for balance testing and seasonal changes. Schema signature changes when balance patches modified, invalidating old fixtures.

## Performance Note

Current patch application uses O(champions × patches) complexity during catalog load. This is acceptable for current scale (<100 champions, <20 patches) since it's a one-time operation. If catalog loading becomes a bottleneck, consider indexing patches by target/role to reduce lookup overhead.

## Filter semantics and application order

**Filter match (`patch_applies_to`, `sim_catalog.cpp:43-71`):** `matches_target OR matches_role`.
- `targets` empty -> matches every champion; `roles` empty -> matches every role.
- **Footgun:** if a patch sets *both* `targets` and `roles`, OR means it applies to the listed
  champions **and** to every champion of the listed roles (not "this champion only when in this
  role"). If both are empty, the patch applies to **all** champions.

**Application order (`rebuild_effective_champion_cache`, `sim_catalog.cpp:190-241`):**
1. Duplicate the base champion entry.
2. Overlay `role_configs[role].stat_mods` onto stats (these **overwrite** stat values).
3. For each patch in `balance_patches` **JSON array order**, if it applies:
   a. `stat_multipliers` (skip `attack_speed` when base is 0), then `stat_additions`;
   b. `kit_id` merge (ability/ultimate/passive_ids from the named kit);
   c. inline `ability` / `ultimate` / `passive_ids` overrides — applied **after** the kit, so
      inline overrides win over a kit in the same patch.
4. Validate the effective champion; on failure, fall back to the vanilla entry + role stat_mods.

**Stacking:** multiple matching patches accumulate in array order (later patches see earlier
patches' mutated stats); the last inline ability/ultimate/passive override wins.
