# Effect System

Modular ability/ultimate/passive execution using composable opcodes.

Effects are defined as `EffectSpec` with a `kind` (opcode) and `params` dict. Kinds include: damage, projectile, stun, shield, heal, aoe_damage, aoe_taunt, damage_over_time, stat_modifier, multi_effect, multi_target, channel, and status effects (slow, root, silence, disarm, stealth, knockback, reflect).

Effects can nest recursively via `effects` arrays and `splash` dicts. Execution passes an `EffectContext` with source/target/distance/action_kind. Results are accumulated in a per-opcode slot store for conditional chaining via `requires_result_from`, `requires_field`, `requires_value`.

Passive effects trigger on hooks: on_tick, on_take_damage, on_deal_damage, on_heal, on_takedown, on_ally_defense.
