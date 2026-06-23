# Kit System

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_catalog`, `kit_catalog.gd`

Ability/ultimate/passive swap mechanism for balance testing.

Kits are defined in `KIT_DATA` in [`kit_catalog.gd`](../../scripts/simulation/kit_catalog.gd) (source of truth). `fixtures/goldens/champion_kits.json` is the golden export; balance patches reference `kit_id` to swap entire ability/ultimate/passive definitions on target champions.

KitCatalog builds kits from `KIT_DATA`. Kits allow rapid ability reconfiguration without modifying `champion_catalog.gd`. Used for balance suite testing and experimental designs. See [DATA_SOURCE.md](../../fixtures/goldens/DATA_SOURCE.md).

Kit merging: _merge_kit_into_champion replaces ability/ultimate/passive_ids if present in kit. Effective champion cache rebuilt after kit application.
