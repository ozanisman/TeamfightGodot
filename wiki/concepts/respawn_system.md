# Respawn System

Units return to battlefield after death with timer-based revival.

On death: unit sets alive=false, records death time, increments death counter. Respawn timer counts down from respawn_time (default 10s RESPAWN_TIME, champion-specific). When timer expires, unit respawns at random Y position within [RESPAWN_Y_MIN, RESPAWN_Y_MAX] (3.0 to 7.0).

Spawn slots track used positions per team to prevent overlap. Player spawns at x=1.0, enemy at x=9.0. Release slot on death, reassign on respawn. Respawn restores HP to max, mana to max, clears all CC and temporary modifiers.

Match ends when one team has no alive units and no pending respawns. Sudden death extends match if both teams have units after 60s duration.
