# Glossary

Snapshot = capture of mutable state at a point in time

Opcode = numeric identifier for effect kind (damage=2, projectile=3, stun=4, etc.)

Fixture = golden test case with input, expected summary, and signatures

Parity = equivalence between GDScript and native simulation outputs

Contract = schema definition for match input/summary and champion data

Tick = fixed time step (default 0.1s) in simulation loop

Role = unit archetype: tank, fighter, assassin, marksman, mage, support

Hook = trigger point for passive effects: on_tick, on_take_damage, on_deal_damage, etc.

Windup = casting delay before ability execution (default 0.5s). Can be overridden per ability/ultimate via "windup_override" param in effect params.

CC = crowd control status: stun, slow, root, silence, disarm, stealth

Debug/Logging Schema = Use UtilityFunctions::push_error(vformat("message", args)) for debug output in native code. Never use printf.