# Combat Pipeline

Tick-based simulation loop with fixed time step (default 0.1s DEFAULT_TICK_RATE).

Each tick runs `_step_tick()` which:
1. Updates projectiles (move, collision, resolve)
2. Prepares tick context (team centers, backliner lists, spatial grids)
3. Updates each alive unit via `_update_unit()`:
   - Handle death/respawn
   - Update cooldowns and CC timers
   - Regenerate HP/mana
   - Process on_tick passive effects
   - Handle casting windup/resolve
   - Select target (enemy or ally for support)
   - Cast ability/ultimate if ready and in range
   - Auto-attack if ready and in range
   - Move toward target
4. Refresh target pressure (incoming target counts, cluster density)
5. Check match end conditions (time limit or team wipe)

Match ends at 60s duration (MATCH_DURATION) or when one team has no alive units. Sudden death extends up to 10000 ticks (SUDDEN_DEATH_MAX_TICKS) if kills are equal.