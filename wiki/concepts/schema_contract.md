# Schema Contract

Parity testing framework with signed fixtures.

SimulationSchema exports champion schema, role configs, and match input/summary contracts. Schema signature hashes the contract definition. Fixture signature hashes per-match summary payload.

Golden fixtures in fixtures/goldens/ contain: match inputs, expected summaries, schema signature, fixture signature, rules_version. Parity tools verify signatures and compare payloads with float tolerance.

Contract schema defines: match_input (seed, tick_rate, player_units, enemy_units, record_events, rules_version, balance_version), match_summary (seed, winner_team, duration, unit_stats, player_comp, enemy_comp), and champions (archetype definitions).

Versioning: SIMULATION_RULES_VERSION invalidates old fixtures. Balance patches applied via balance_version string.
