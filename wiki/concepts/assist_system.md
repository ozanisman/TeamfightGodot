# Assist System

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_match_lifecycle`, `sim_damage_apply`

Credit for kills based on damage contribution within time window.

Assist window: 5 seconds (ASSIST_WINDOW). Damage sources tracked per unit with timestamp and amount. Recent benefactors tracked for assist credit.

On unit death: killer gets kill credit, all recent damage dealers (within assist window) get assist credit. Takedown effects trigger for both killer and assistants with damage_dealt and is_kill flag.

Damage sources dictionary maps source_instance_id to {damage, last_time}. Pruned when last_time exceeds assist window. Used for threat calculation and assist tracking.
