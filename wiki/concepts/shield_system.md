# Shield System

Temporary damage absorption with decay mechanics.

Shield applied via shield opcode (max_hp_ratio or flat_amount). Shield stored in UnitState.shield field. Damage subtracts from shield before HP. Shield decays each tick: `shield *= (1.0 - SHIELD_DECAY_RATE * tick_rate)` where SHIELD_DECAY_RATE = 0.1.

Shield effects: shield (single target), aoe_shield (area), damage_based_shield (based on damage dealt), knockback_shield (shield + knockback on hit). Shield tracking in telemetry: shielding_done, shielding_done_auto, shielding_done_ability, shielding_done_ultimate, shielding_done_passive.

Shield resets to 0 on spawn/respawn. Shield can be refreshed by reapplying effect.
