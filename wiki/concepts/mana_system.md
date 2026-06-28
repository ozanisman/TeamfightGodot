# Mana System

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_status_heal`, `sim_unit_tick_regen`, `sim_effects_exec_mana`

Resource for ability casting with generation and consumption.

Mana stats: mana_cost (default 50), mana_per_attack (default 10). Mana gained on auto-attack: `unit.mana = min(mana_cost, unit.mana + mana_per_attack)`.

Mana restoration via effects: mana_regen (flat amount + mana_cost_ratio), post_damage_mana_gain, mana_restore_on_hit, and drain_target_mana_on_hit. Casting requires mana >= 0. Ability/ultimate consume mana on cast (implied by ability_cd). Mana resets to max on spawn/respawn.

Role-specific mana generation (defined in GDScript constants but not yet implemented in native core): tanks gain mana from damage taken, mages have passive mana regen.
