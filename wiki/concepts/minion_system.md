# Minion System

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

## Implementation Details

### GDScript

- `build_minion_catalog()`: Builds minion specs from MINION_DATA
- `get_minion(minion_id)`: Retrieves a minion spec
- Thread-local caching for multi-threading safety
- Frozen specs for worker reuse (parallel with champion catalog)

### C++

- `EFFECT_OPCODE_SUMMON_ALLY = 55`: Effect opcode for summoning minions
- `MINION_SCHEMA_PATH`: Path to `minion_schema.json` fixture file
- `_minion_catalog`: Dictionary member storing minion data loaded from separate schema
- `_find_random_spawn_position_near()`: Finds valid spawn positions with collision checking
- `_handle_death()`: Skips scoring for minions (detected by role check)

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
