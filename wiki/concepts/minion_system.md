# Minion System

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_catalog`, `sim_unit_builder`, `sim_match_spawn`

Minions are summonable units that fight alongside champions. They are defined in MINION_DATA (parallel to CHAMPION_DATA) and use the role="minion" to distinguish them from champions.

## Minion Definition

Minions are defined in `champion_catalog.gd` under the `MINION_DATA` constant. Each minion entry follows the same structure as champions:

```gdscript
const MINION_DATA := {
    &"skeleton": {
        "stats": {
            "unit_id": &"skeleton",
            "name": &"Skeleton",
            "role": &"minion",  # Critical: distinguishes from champions
            "max_hp": 80.0,
            "attack_damage": 8.0,
            # ... other stats
        },
        "description": "A fragile undead warrior that fights with basic attacks.",
        "ability": {},
        "ultimate": {},
        "passive_ids": [],
    },
}
```

## Summon Ally Effect

The `summon_ally` effect creates minion units during combat.

### Params Structure

```json
{
    "minions": [
        {"minion_id": "skeleton", "count": 2},
        {"minion_id": "wolf", "count": 1}
    ],
    "spawn_radius": 2.0
}
```

- `minions`: Array of minion specifications, each with:
  - `minion_id`: ID from MINION_DATA
  - `count`: Number of minions of this type to spawn
- `spawn_radius`: Maximum distance from summoner to spawn (default 2.0)

### Behavior

- Minions spawn at random valid positions within `spawn_radius` of the summoner
- Position finding includes collision detection to avoid overlapping units
- Minions inherit the summoner's team
- Multiple minion types can be spawned in a single effect

## Minion Lifecycle

- **Spawn**: Created via `summon_ally` effect during combat
- **Death**: Permanent - minions do not respawn
- **Scoring**: Minion deaths do not count toward team score
- **Role Detection**: Minions are identified by having `role="minion"` (no champion role flags set)

## System Limitations

### Memory Accumulation

The simulation uses index-based arrays (`_units`, `_unit_cold`, `_targeting_frame`) that grow monotonically. Units are never removed from these arrays on death to maintain index stability throughout the simulation. This design choice has implications:

- **Dead units accumulate**: Both champions and minions remain in the arrays after death, marked as `alive = false`
- **Index stability**: All systems reference units by index (via `_unit_index_map`), so removal would require expensive index remapping
- **Memory overhead**: For typical match sizes (10 champions per team, ~50 minions total), the memory overhead is negligible
- **Champions**: Already follow this pattern - they toggle `alive` on death/respawn without array removal
- **Minions**: Since they don't respawn, dead minion entries persist until match end

This is a deliberate trade-off: simpler, more performant code at the cost of minor memory growth. If memory ever becomes a concern, a slot-reuse system (free list) would be preferable to removal-with-shifting.

### Performance Consideration

If minion count grows significantly (e.g., hundreds of minions per match), consider separating minions into their own array. This would eliminate dead-minion processing overhead entirely (respawn checks, targeting updates, etc.) since dead minions could be removed from the minion array without affecting champion indexing. This is a larger architectural change affecting targeting/indexing systems.

## Implementation Details

### GDScript

- `build_minion_catalog()`: Builds minion specs from MINION_DATA
- `get_minion(minion_id)`: Retrieves a minion spec
- Thread-local caching for multi-threading safety
- Frozen specs for worker reuse (parallel with champion catalog)

### C++

- `EFFECT_OPCODE_SUMMON_ALLY = 16`: Effect opcode for summoning minions
- `MINION_SCHEMA_PATH`: Path to `minion_schema.json` fixture file
- `_minion_catalog`: Dictionary member storing minion data loaded from separate schema
- `_find_random_spawn_position_near_excluding_with_expansion()`: Finds valid spawn positions with collision checking and radius expansion fallback
- `_handle_death()`: Skips scoring for minions (detected by role check)
- **Note**: Minion spec children in `EffectRecord` are data containers (storing minion_id in string0, count in int0), not executable sub-effects. The opcode field is intentionally not set for these children to avoid confusion with recursive effect execution.

### Schema Export

Minions are exported to a separate `minion_schema.json` file (not champion_schema.json), keeping minion data cleanly separated from champion data. The C++ backend loads minions from this dedicated file.

## Usage Example

```gdscript
{
    "kind": &"summon_ally",
    "params": {
        "minions": [
            {"minion_id": "skeleton", "count": 3}
        ],
        "spawn_radius": 1.5
    }
}
```

This spawns 3 skeleton minions within 1.5 tiles of the caster.
