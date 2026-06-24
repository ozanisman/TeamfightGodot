# Damage Calculation

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_damage*`, `sim_stats_modifiers`

Armor and magic resist reduce incoming damage by percentage. Values are integer points: 22 armor means 22% damage reduction.

Damage types: physical (reduced by armor), magic (reduced by magic resist), true (ignores all defense). Defense multiplier formula: `1.0 - defense_stat / 100.0` for armor/magic resist. Negative defense values increase damage taken (defense multiplier > 1.0); there is no upper clamp on the multiplier.

Final damage calculation: `base_damage * attack_modifiers * ability_modifiers * ultimate_modifiers * defense_multiplier * dodge_multiplier`. Auto-dodge (agility passive) can negate auto attacks entirely.

Lifesteal heals for percentage of damage dealt: `heal = damage * life_steal`. Reflect damage returns portion of received damage to attacker: `reflected = damage * reflect_pct`. Reflect can filter by damage type (physical, magic, true, or all). Damage sources tracked for assist credit (5s window).

Critical damage not implemented. Execute bonuses are targeting preferences (not damage bonuses) when target HP below 25%.

## Cumulative Damage Context

`EffectContext.damage` is a running total of damage dealt within the current effect chain. Each `damage` and `aoe_damage` opcode adds its dealt amount to this value. Damage-based effects such as `damage_based_heal`, `damage_based_shield`, `shield` with `damage_ratio`, and `post_damage_mana_gain` scale from this cumulative total. This allows multi-effect passives and abilities to build on previous damage effects in the same chain.

`post_attack` passives merge any extra dealt damage from their internal chain back into the parent action context.

`MULTI_TARGET` reuses per-target scratch contexts (including `stack` re-selections of the same unit), chains `accumulated_results` across sub-effects, and merges dealt-damage deltas into the parent. See [effect_system.md](effect_system.md#multi_target-context).

Channel ticks and channel projectile impacts accumulate dealt damage into `channel_accumulated_damage` for `use_accumulated_damage` finishers (`damage_based_heal`, `damage_based_shield`, `aoe_damage`, `damage`). Projectile deferral covers remaining `multi_target` work and parent `multi_effect` siblings. See [effect_system.md](effect_system.md#projectile-deferred-damage).

> **Out of scope for this pass:** Only `damage` is accumulated. Other `EffectContext` fields such as `heal_amount`, `heal_gained`, and `distance` are not yet accumulated by their opcodes.
