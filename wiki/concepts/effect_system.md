# Effect System

Modular ability/ultimate/passive execution using composable opcodes.

Effects are defined as `EffectSpec` with a `kind` (opcode) and `params` dict. Opcodes (numeric values in C++): DAMAGE=2, PROJECTILE=3, STUN=4, SHIELD=5, HEAL=6, AOE_TAUNT=9, AOE_DAMAGE=10, DAMAGE_THRESHOLD_TRIGGER=11, MANA_REGEN=13, POST_DAMAGE_MANA_GAIN=14, DAMAGE_BASED_HEAL=15, MANA_RESTORE_ON_HIT=16, DRAIN_TARGET_MANA_ON_HIT=17, SELF_DASH=19, AUTO_DODGE=20, CONSTANT_MULTIPLIER=21, SLOW=25, ROOT=26, SILENCE=27, DISARM=28, KNOCKBACK=29, REFLECT=30, STEALTH=42, DAMAGE_OVER_TIME=43, HEAL_OVER_TIME=44, STAT_MODIFIER=41, MULTI_EFFECT=1, MULTI_TARGET=47, CHANNEL=53, CONSUME_STACKS_DAMAGE=49, CONSUME_STACKS_HEAL=50, CONSUME_STACKS_SHIELD=51, SET_STACKS=52, REDIRECT_DAMAGE=54, SUMMON_ALLY=55.

AOE variants: AOE_SLOW, AOE_ROOT, AOE_SILENCE, AOE_DISARM, AOE_KNOCKBACK, AOE_REFLECT, AOE_STUN, AOE_DAMAGE_OVER_TIME, AOE_HEAL_OVER_TIME.

Effects can nest recursively via `effects` arrays and `splash` dicts. Execution passes an `EffectContext` with source/target/distance/action_kind. Results are accumulated in a per-opcode slot store for conditional chaining via `requires_result_from`, `requires_field`, `requires_value`.

Passive effects trigger on hooks: on_tick, on_take_damage, on_deal_damage, on_heal, on_takedown, on_ally_defense.
