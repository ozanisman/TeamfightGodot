# Kit System

Ability/ultimate/passive swap mechanism for balance testing.

Kits defined in champion_kits.json with kit_id mapping to ability, ultimate, passive_ids. Balance patches reference kit_id to swap entire ability/ultimate/passive definitions on target champions.

KitCatalog builds kits from GDScript KIT_DATA constant or JSON. Kits allow rapid ability reconfiguration without modifying champion_catalog.gd. Used for balance suite testing and experimental designs.

Kit merging: _merge_kit_into_champion replaces ability/ultimate/passive_ids if present in kit. Effective champion cache rebuilt after kit application.
