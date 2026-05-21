# Mana System

Resource for ability casting with generation and consumption.

Mana stats: max_mana (default 50), mana_per_attack (default 10). Mana gained on auto-attack. Role-specific generation: tanks gain mana from damage taken (TANK_MANA_GAIN_DAMAGE_RATIO = 0.1), mages have passive mana regen (MAGE_MANA_REGEN_TICK = 1.0).

Mana restoration via effects: mana_regen (flat amount), mana_restore_on_hit, drain_target_mana_on_hit. Post-damage mana gain for tanks via role config passive.

Casting requires mana >= 0. Ability/ultimate consume mana on cast (implied by ability_cd). Mana resets to max on spawn/respawn.
