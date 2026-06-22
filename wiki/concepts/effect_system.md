# Effect System

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_effects_compile*`, `sim_effects_exec*`, `sim_effects_host`.

Modular ability/ultimate/passive execution using composable opcodes.

Effects are defined as `EffectSpec` with a `kind` (opcode) and `params` dict. Opcodes (numeric values in C++):

**Core effects (1-99)**
- `MULTI_EFFECT = 1`
- `DAMAGE = 2`
- `PROJECTILE = 3`
- `MULTI_TARGET = 4`
- `DAMAGE_THRESHOLD_TRIGGER = 5`
- `AUTO_DODGE = 6`
- `CONSTANT_MULTIPLIER = 7`
- `HP_THRESHOLD_DAMAGE_MULTIPLIER = 8`
- `DISTANCE_THRESHOLD_MULTIPLIER = 9`
- `REFLECT_DAMAGE = 10`
- `TARGET_STATUS_MULTIPLIER = 11`
- `STAT_MODIFIER = 12`
- `DAMAGE_OVER_TIME = 13`
- `CONSUME_STACKS_DAMAGE = 14`
- `REDIRECT_DAMAGE = 15`
- `SUMMON_ALLY = 16`

**Status effects (100-199)**
- `STUN = 100`
- `SHIELD = 101`
- `HEAL = 102`
- `MANA_REGEN = 103`
- `POST_DAMAGE_MANA_GAIN = 104`
- `DAMAGE_BASED_HEAL = 105`
- `MANA_RESTORE_ON_HIT = 106`
- `DRAIN_TARGET_MANA_ON_HIT = 107`
- `EVERY_N_ATTACKS_STUN = 108`
- `SELF_DASH = 109`
- `SLOW = 110`
- `ROOT = 111`
- `SILENCE = 112`
- `DISARM = 113`
- `KNOCKBACK = 114`
- `REFLECT = 115`
- `KNOCKBACK_SHIELD = 116`
- `STEALTH = 117`
- `HEAL_OVER_TIME = 118`
- `DAMAGE_BASED_SHIELD = 119`
- `CONSUME_STACKS_HEAL = 120`
- `CONSUME_STACKS_SHIELD = 121`
- `SET_STACKS = 122`
- `CHANNEL = 123`

**AOE effects (200-299)**
- `AOE_TAUNT = 200`
- `AOE_DAMAGE = 201`
- `AOE_SLOW = 202`
- `AOE_ROOT = 203`
- `AOE_SILENCE = 204`
- `AOE_DISARM = 205`
- `AOE_KNOCKBACK = 206`
- `AOE_REFLECT = 207`
- `AOE_STUN = 208`
- `AOE_DAMAGE_OVER_TIME = 209`
- `AOE_HEAL_OVER_TIME = 210`

AOE variants: AOE_SLOW, AOE_ROOT, AOE_SILENCE, AOE_DISARM, AOE_KNOCKBACK, AOE_REFLECT (supports damage_type parameter: physical, magic, true, or all), AOE_STUN, AOE_DAMAGE_OVER_TIME, AOE_HEAL_OVER_TIME.

Effects can nest recursively via `effects` arrays and `splash` dicts. Execution passes an `EffectContext` with source/target/distance/action_kind. Results are accumulated in a per-opcode slot store for conditional chaining via `requires_result_from`, `requires_field`, `requires_value`.

Passive effects trigger on hooks: on_tick, on_take_damage, on_deal_damage, on_heal, on_takedown, on_ally_defense.

## MULTI_TARGET parameters

`MULTI_TARGET` selects N units from a team and applies `sub_effects` to each.

| Param | Type | Default | Description |
|-------|------|---------|-------------|
| `target_count` | int | 1 | Number of targets to select. `-1` = all matching candidates. |
| `selection_strategy` | string | `"closest"` | One of: `closest`, `random`, `lowest_hp`, `highest_hp`, `closest_to_target`, `lowest_percent_hp`, `highest_percent_hp`. |
| `team_filter` | string | (required) | `"ally"` or `"enemy"`. Selects which team pool to draw from. |
| `include_self` | bool | false | If true, the source unit is a valid candidate. |
| `excess_handling` | string | `"drop"` | `"drop"` clamps selection to available candidates; `"stack"` cycles candidates to fill `target_count` (can select the same unit multiple times). |
| `repeat_count` | int | 1 | How many times to apply `sub_effects` to each selected target. |
| `radius` | float | 0.0 | Euclidean distance limit from source. `0.0` = no limit (select from anywhere on the map). `>0.0` = drop candidates farther than `radius` tiles before selection. |
| `sub_effects` | dict or array | (required) | One or more effects to apply to each selected target. |

`radius` is independent of the ability's `cast_range` gate. `cast_range` controls whether the ability can fire at all; `radius` controls which candidates are eligible once it fires. For ally-targeted `multi_target`, `compile_cast_range_spec` sets `skips_proximity = true`, so `cast_range` is ignored — use `radius` to restrict ally selection by distance.
