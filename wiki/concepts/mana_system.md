# Mana System

Resource for ability casting with generation and consumption.

Mana stats: max_mana (default 50), mana_per_attack (default 10). Mana gained on auto-attack: `unit.mana = min(max_mana, unit.mana + mana_per_attack)`.

Mana restoration via effects: mana_restore (flat amount + max_hp_ratio). Casting requires mana >= 0. Ability/ultimate consume mana on cast (implied by ability_cd). Mana resets to max on spawn/respawn.

Role-specific mana generation (defined in GDScript constants but not yet implemented in native core): tanks gain mana from damage taken, mages have passive mana regen.
