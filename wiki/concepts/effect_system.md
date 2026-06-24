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
- `STEALTH = 116`
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

Passive effects trigger on hooks: `on_tick`, `on_take_damage`, `on_deal_damage`, `on_heal`, `on_takedown`, `on_ally_defense`, `on_knockback`, `on_knockback_action`.

## Cumulative Damage Context

`EffectContext.damage` is a running total of damage dealt within the current effect chain. Every `damage` opcode and `aoe_damage` opcode adds its dealt amount to the current value. Effects that scale from dealt damage, such as `damage_based_heal`, `damage_based_shield`, and `shield` with `damage_ratio`, read this cumulative value.

This lets multi-effect passives and abilities build on previous damage effects. For example, a `post_attack` passive with a `damage` effect followed by a `damage_based_shield` will create a shield based on the attack damage plus the extra damage from the first effect.

Hook helpers initialize the context with the triggering event damage (`run_post_attack_effects`, `run_on_takedown_effects`, `run_post_take_damage_passives`). `run_post_attack_effects` also merges any extra dealt damage from passive sub-effects back into the parent action context.

Knockback hooks preserve action `damage` and `accumulated_results` so conditional hook effects can chain off prior opcodes in the same action.

`damage_based_shield` supports `use_accumulated_damage` (same as `damage_based_heal`) to scale from `channel_accumulated_damage` on channel finishers.

### `multi_target` context

`MULTI_TARGET` keeps a **per-target scratch context** keyed by `target_id`:

- The first application to a unit copies the parent context into scratch; later `stack` selections for the same unit reuse that scratch.
- All `sub_effects` and `repeat_count` iterations for that unit share scratch, so sub-effects can chain (e.g. `damage` then `damage_based_heal`).
- Each sub-effect stores its result in scratch `accumulated_results` for `requires_result_from` conditionals on later sub-effects (no `multi_effect` wrapper required).
- When a target application pass finishes, only the **dealt-damage delta** is merged into the parent `context.damage`.

`knockback_applied` is merged to the parent after each sub-effect execution.

### Channel accumulated damage

Channel ticks credit dealt damage to `channel_accumulated_damage` using the tick execution context delta (not only a top-level `"damage"` result field). This includes nested `multi_effect` / `aoe_damage` chains.

Channel `multi_target` projectiles credit dealt damage on impact while the source is still channeling.

Effects with `use_accumulated_damage` (`damage_based_heal`, `damage_based_shield`, `aoe_damage`, `damage`) read `channel_accumulated_damage` on post-complete and post-interrupt finishers.

### Projectile deferred damage

Defer only when a projectile sub-effect must wait for impact before later work that reads cumulative damage:

- **Defer when:** a later `sub_effect` in the same target pass reads cumulative `context.damage` (`damage_based_heal`, `damage_based_shield`, `shield` with `damage_ratio`, `damage_threshold_trigger`), or the parent `multi_effect` has remaining siblings.
- **Do not defer for:** `repeat_count` alone or advancing to the next selected target — those projectiles spawn in parallel without waiting for prior impacts.

When defer applies:

- Each tracked projectile increments `deferred_effect_outstanding_projectiles` on the source.
- `multi_target` saves its iteration state and resumes when outstanding hits zero; `multi_effect` siblings pause until `multi_target` fully completes.
- On resume, dealt damage merges into both the parent context and the per-target scratch context when the resumed target matches the saved `scratch_target_id`.
- Abandoned tracked projectiles (target lost/dead before impact) decrement outstanding and may resume the chain.

**Channel interaction:**

- Channel ticks skip `sub_effect` execution while `deferred_multi_target_active` or `deferred_effect_outstanding_projectiles > 0` (a skipped tick is not retried).
- Channel completion/interrupt clears channel fields but preserves open defer chains until outstanding projectiles resolve. Unit death still hard-clears defer state.

Channel projectile impacts still credit `channel_accumulated_damage` when the source is channeling.

> **Out of scope for this pass:** Only `damage` is accumulated across effects. `heal_amount`, `heal_gained`, `distance`, and other `EffectContext` fields are not yet accumulated by their respective opcodes.

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
