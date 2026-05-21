# Status Effects

Crowd control and state modifiers applied via effects.

Status effects: stun (cannot act), slow (reduced move speed), root (cannot move), silence (cannot cast abilities), disarm (cannot auto-attack), stealth (cannot be targeted by enemies), knockback (position displacement), taunt (forced target).

Duration-based timers tick down each delta. Tenacity reduces stun duration. Multiple effects of same type stack by refresh (reset duration), extend (add duration), or separate (independent instances).

Stealth has break conditions: on_attack, on_ability, on_damage_taken. Taunt forces target_id to caster for duration. Knockback moves unit away from source by distance.

AOE variants apply status to all units in radius/shape: aoe_stun, aoe_slow, aoe_root, aoe_silence, aoe_disarm, aoe_taunt, aoe_knockback.
