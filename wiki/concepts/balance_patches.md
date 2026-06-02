# Balance Patches

Versioned overrides for champion stats and abilities.

Balance patches defined in balance_patches.json with: targets (archetype list, empty = all), roles (role list, empty = all), stat_multipliers (stat → multiplier), stat_additions (stat → addition), kit_id (swap ability/ultimate/passives), ability_override, ultimate_override, passive_ids_override.

Applied via balance_version string in MatchReplayInput. Patch applies if unit_id in targets OR role in roles. Stat modifications override base champion stats. Kit swaps replace entire ability/ultimate/passive definitions.

Balance patches enable rapid iteration without modifying champion_catalog.gd. Used for balance testing and seasonal changes. Schema signature changes when balance patches modified, invalidating old fixtures.

## Performance Note

Current patch application uses O(champions × patches) complexity during catalog load. This is acceptable for current scale (<100 champions, <20 patches) since it's a one-time operation. If catalog loading becomes a bottleneck, consider indexing patches by target/role to reduce lookup overhead.
