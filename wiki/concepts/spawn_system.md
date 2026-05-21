# Spawn System

Initial team composition and positioning at match start.

SpawnSpec defines: archetype_id, team, x, y position. MatchReplayInput contains player_units and enemy_units arrays of SpawnSpec. Default positioning via SimConstantsScript.spawn_position(index, team) using grid layout (4 columns, row-based).

Player team spawns at x=0.9 + column*0.9, enemy team at x=10.0 - column*0.9. Y positions: 1.2 + row*1.8. Custom positions allowed via explicit x,y in SpawnSpec.

Draft generation for benchmarks uses deterministic RNG with base_seed to select random champions from catalog. Native core owns draft generation for benchmark fast paths to avoid GDScript overhead.
