#!/usr/bin/env python3
"""Split sim_effects_exec.cpp into category files (Phase 4)."""
from __future__ import annotations

import re
from pathlib import Path

BASE = Path(__file__).resolve().parents[1] / "src" / "simulation"
SRC = BASE / "sim_effects_exec.cpp"
ORIGINAL = Path(__file__).resolve().parents[2] / ".split_original_sim_effects_exec.cpp"

CATEGORIES = {
    "damage": {
        "EFFECT_OPCODE_DAMAGE",
        "EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER",
        "EFFECT_OPCODE_CONSUME_STACKS_DAMAGE",
        "EFFECT_OPCODE_REFLECT_DAMAGE",
        "EFFECT_OPCODE_REDIRECT_DAMAGE",
        "EFFECT_OPCODE_AUTO_DODGE",
        "EFFECT_OPCODE_CONSTANT_MULTIPLIER",
        "EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER",
        "EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER",
        "EFFECT_OPCODE_STAT_MODIFIER",
    },
    "status": {
        "EFFECT_OPCODE_STUN",
        "EFFECT_OPCODE_SHIELD",
        "EFFECT_OPCODE_HEAL",
        "EFFECT_OPCODE_MANA_REGEN",
        "EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN",
        "EFFECT_OPCODE_DAMAGE_BASED_HEAL",
        "EFFECT_OPCODE_DAMAGE_BASED_SHIELD",
        "EFFECT_OPCODE_CONSUME_STACKS_HEAL",
        "EFFECT_OPCODE_CONSUME_STACKS_SHIELD",
        "EFFECT_OPCODE_SET_STACKS",
        "EFFECT_OPCODE_CHANNEL",
        "EFFECT_OPCODE_MANA_RESTORE_ON_HIT",
        "EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT",
        "EFFECT_OPCODE_EVERY_N_ATTACKS_STUN",
        "EFFECT_OPCODE_SLOW",
        "EFFECT_OPCODE_ROOT",
        "EFFECT_OPCODE_SILENCE",
        "EFFECT_OPCODE_DISARM",
        "EFFECT_OPCODE_STEALTH",
        "EFFECT_OPCODE_KNOCKBACK_SHIELD",
        "EFFECT_OPCODE_KNOCKBACK",
        "EFFECT_OPCODE_REFLECT",
        "EFFECT_OPCODE_SELF_DASH",
    },
    "spawn": {
        "EFFECT_OPCODE_PROJECTILE",
        "EFFECT_OPCODE_SUMMON_ALLY",
    },
    "aoe": {
        "EFFECT_OPCODE_AOE_TAUNT",
        "EFFECT_OPCODE_AOE_DAMAGE",
        "EFFECT_OPCODE_DAMAGE_OVER_TIME",
        "EFFECT_OPCODE_HEAL_OVER_TIME",
        "EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME",
        "EFFECT_OPCODE_AOE_HEAL_OVER_TIME",
        "EFFECT_OPCODE_AOE_SLOW",
        "EFFECT_OPCODE_AOE_ROOT",
        "EFFECT_OPCODE_AOE_SILENCE",
        "EFFECT_OPCODE_AOE_DISARM",
        "EFFECT_OPCODE_AOE_KNOCKBACK",
        "EFFECT_OPCODE_AOE_REFLECT",
        "EFFECT_OPCODE_AOE_STUN",
    },
    "main": {
        "EFFECT_OPCODE_MULTI_EFFECT",
        "EFFECT_OPCODE_MULTI_TARGET",
        "default",
    },
}

COMMON_INCLUDES = """\
#include "sim_effects_exec.hpp"
#include "sim_effects_exec_internal.hpp"
#include "sim_effects_compile.hpp"

#include "sim_channel.hpp"
#include "sim_combat.hpp"
#include "sim_constants.hpp"
#include "sim_movement.hpp"
#include "sim_match_spawn.hpp"
#include "sim_stats_modifiers.hpp"
#include "sim_targeting.hpp"
#include "sim_damage.hpp"
#include "sim_periodic.hpp"
#include "sim_stats.hpp"
#include "sim_status.hpp"

#include "stat_definitions.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#define X(name, def, min_val, max_val) using sim::get_effective_##name;
STAT_LIST
#undef X
"""

EXEC_PARAMS = (
    "const EffectRecord &effect, EffectContext &context, SimWorld &world, "
    "SimHostCallbacks &host, const SimExecCallbacks &hooks, const ::sim::effects::SimMatchHost &match_host, "
    "UnitState &source, UnitState *target, UnitState *target_ally"
)

DELEGATE_CASES = """\
\t\tcase EFFECT_OPCODE_DAMAGE:
\t\tcase EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER:
\t\tcase EFFECT_OPCODE_CONSUME_STACKS_DAMAGE:
\t\tcase EFFECT_OPCODE_REFLECT_DAMAGE:
\t\tcase EFFECT_OPCODE_REDIRECT_DAMAGE:
\t\tcase EFFECT_OPCODE_AUTO_DODGE:
\t\tcase EFFECT_OPCODE_CONSTANT_MULTIPLIER:
\t\tcase EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER:
\t\tcase EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER:
\t\tcase EFFECT_OPCODE_STAT_MODIFIER:
\t\t\treturn exec_damage(effect, context, world, host, hooks, match_host, source, target, target_ally);
\t\tcase EFFECT_OPCODE_STUN:
\t\tcase EFFECT_OPCODE_SHIELD:
\t\tcase EFFECT_OPCODE_HEAL:
\t\tcase EFFECT_OPCODE_MANA_REGEN:
\t\tcase EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN:
\t\tcase EFFECT_OPCODE_DAMAGE_BASED_HEAL:
\t\tcase EFFECT_OPCODE_DAMAGE_BASED_SHIELD:
\t\tcase EFFECT_OPCODE_CONSUME_STACKS_HEAL:
\t\tcase EFFECT_OPCODE_CONSUME_STACKS_SHIELD:
\t\tcase EFFECT_OPCODE_SET_STACKS:
\t\tcase EFFECT_OPCODE_CHANNEL:
\t\tcase EFFECT_OPCODE_MANA_RESTORE_ON_HIT:
\t\tcase EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT:
\t\tcase EFFECT_OPCODE_EVERY_N_ATTACKS_STUN:
\t\tcase EFFECT_OPCODE_SLOW:
\t\tcase EFFECT_OPCODE_ROOT:
\t\tcase EFFECT_OPCODE_SILENCE:
\t\tcase EFFECT_OPCODE_DISARM:
\t\tcase EFFECT_OPCODE_STEALTH:
\t\tcase EFFECT_OPCODE_KNOCKBACK_SHIELD:
\t\tcase EFFECT_OPCODE_KNOCKBACK:
\t\tcase EFFECT_OPCODE_REFLECT:
\t\tcase EFFECT_OPCODE_SELF_DASH:
\t\t\treturn exec_status(effect, context, world, host, hooks, match_host, source, target, target_ally);
\t\tcase EFFECT_OPCODE_PROJECTILE:
\t\tcase EFFECT_OPCODE_SUMMON_ALLY:
\t\t\treturn exec_spawn(effect, context, world, host, hooks, match_host, source, target, target_ally);
\t\tcase EFFECT_OPCODE_AOE_TAUNT:
\t\tcase EFFECT_OPCODE_AOE_DAMAGE:
\t\tcase EFFECT_OPCODE_DAMAGE_OVER_TIME:
\t\tcase EFFECT_OPCODE_HEAL_OVER_TIME:
\t\tcase EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME:
\t\tcase EFFECT_OPCODE_AOE_HEAL_OVER_TIME:
\t\tcase EFFECT_OPCODE_AOE_SLOW:
\t\tcase EFFECT_OPCODE_AOE_ROOT:
\t\tcase EFFECT_OPCODE_AOE_SILENCE:
\t\tcase EFFECT_OPCODE_AOE_DISARM:
\t\tcase EFFECT_OPCODE_AOE_KNOCKBACK:
\t\tcase EFFECT_OPCODE_AOE_REFLECT:
\t\tcase EFFECT_OPCODE_AOE_STUN:
\t\t\treturn exec_aoe(effect, context, world, host, hooks, match_host, source, target, target_ally);
"""


def category_for_labels(labels: list[str]) -> str:
    for cat, opcodes in CATEGORIES.items():
        if any(label in opcodes for label in labels):
            return cat
    raise RuntimeError(f"Unassigned case labels: {labels}")


def parse_switch_cases(switch_lines: list[str]) -> dict[str, list[tuple[list[str], str]]]:
    blocks: dict[str, list[tuple[list[str], str]]] = {}
    current_labels: list[str] = []
    current_lines: list[str] = []
    i = 0
    while i < len(switch_lines):
        line = switch_lines[i]
        match = re.match(r"\t*case (EFFECT_OPCODE_\w+|default):", line)
        if match:
            if current_labels:
                cat = category_for_labels(current_labels)
                blocks.setdefault(cat, []).append((current_labels, "".join(current_lines)))
            current_labels = [match.group(1)]
            current_lines = [line]
            i += 1
            while i < len(switch_lines):
                line2 = switch_lines[i]
                match2 = re.match(r"\t*case (EFFECT_OPCODE_\w+|default):", line2)
                if match2 and not line2.rstrip().endswith("{"):
                    current_labels.append(match2.group(1))
                    current_lines.append(line2)
                    i += 1
                    continue
                break
            continue
        current_lines.append(line)
        i += 1

    if current_labels:
        cat = category_for_labels(current_labels)
        blocks.setdefault(cat, []).append((current_labels, "".join(current_lines)))
    return blocks


def build_case_blocks(blocks: list[tuple[list[str], str]]) -> str:
    return "".join(body for _, body in blocks)


def build_category_switch(blocks: list[tuple[list[str], str]]) -> str:
    parts = ["\tswitch (effect.opcode) {\n"]
    for _, body in blocks:
        parts.append(body)
    parts.append("\t}\n")
    return "".join(parts)


def load_source_lines() -> list[str]:
    source = ORIGINAL if ORIGINAL.exists() else SRC
    return source.read_text(encoding="utf-8").splitlines(keepends=True)


def find_line(prefix: str, lines: list[str], start: int = 0) -> int:
    for index, line in enumerate(lines[start:], start=start):
        if line.startswith(prefix):
            return index
    raise ValueError(f"Could not find line starting with {prefix!r}")


def find_execute_impl_start(lines: list[str]) -> int:
    for index, line in enumerate(lines):
        if line.startswith("Dictionary execute_impl(const EffectRecord") and line.rstrip().endswith("{"):
            return index
    raise ValueError("Could not find execute_impl definition")


def main() -> None:
    if SRC.exists():
        ORIGINAL.write_text(SRC.read_text(encoding="utf-8"), encoding="utf-8")

    lines = load_source_lines()
    sn_player_line = find_line("inline const StringName &sn_player()", lines)
    unit_by_id_line = find_line("UnitState *unit_by_id(SimWorld &world", lines)
    forward_decl_line = None
    for index, line in enumerate(lines):
        if line.startswith("Dictionary execute_impl(const EffectRecord") and line.strip().endswith(");"):
            forward_decl_line = index
            break
    if forward_decl_line is None:
        raise ValueError("Could not find execute_impl forward declaration")

    merge_start = find_line("void merge_accumulated_results(Dictionary &target", lines)
    execute_impl_start = find_execute_impl_start(lines)

    helper_sn_and_constants = "".join(lines[sn_player_line:unit_by_id_line])
    helper_unit_by_id = "inline " + "".join(lines[unit_by_id_line:forward_decl_line]).lstrip()
    helper_merge_check = "".join(lines[merge_start:execute_impl_start])
    helper_merge_check = helper_merge_check.replace("void merge_accumulated_results", "inline void merge_accumulated_results", 1)
    helper_merge_check = helper_merge_check.replace("bool check_condition", "inline bool check_condition", 1)
    helper_merge_check = helper_merge_check.replace("bool check_target_status_condition", "inline bool check_target_status_condition", 1)
    helper_merge_check = helper_merge_check.replace("bool check_all_conditions", "inline bool check_all_conditions", 1)
    helper_merge_check = helper_merge_check.replace("void merge_result", "inline void merge_result", 1)
    helper_merge_check = helper_merge_check.replace("sim::status::target_has_status", "::sim::status::target_has_status")

    switch_start = find_line("\tswitch (effect.opcode) {", lines, execute_impl_start)
    switch_end = find_line("\t// Should never reach here - all cases should return", lines, switch_start)
    execute_impl_end = find_line("\treturn fallback_result;", lines, switch_end) + 2

    impl_preamble = "".join(lines[execute_impl_start:switch_start])
    switch_lines = lines[switch_start + 1:switch_end - 1]
    tail = "".join(lines[switch_end:execute_impl_end])

    blocks = parse_switch_cases(switch_lines)

    internal_hpp = f"""#ifndef SIM_EFFECTS_EXEC_INTERNAL_HPP
#define SIM_EFFECTS_EXEC_INTERNAL_HPP

#include "sim_effects_host.hpp"
#include "sim_status.hpp"
#include "sim_world.hpp"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::execution {{
namespace internal {{

{helper_sn_and_constants}
{helper_unit_by_id}

{helper_merge_check}

Dictionary execute_impl(
\t\tconst EffectRecord &effect,
\t\tEffectContext &context,
\t\tSimWorld &world,
\t\tSimHostCallbacks &host,
\t\tconst SimExecCallbacks &hooks,
\t\tconst ::sim::effects::SimMatchHost &match_host);

Dictionary execute_recursive(
\t\tconst EffectRecord &effect,
\t\tEffectContext &context,
\t\tSimWorld &world,
\t\tSimHostCallbacks &host,
\t\tconst SimExecCallbacks &hooks,
\t\tconst ::sim::effects::SimMatchHost &match_host);

Dictionary exec_damage({EXEC_PARAMS});
Dictionary exec_status({EXEC_PARAMS});
Dictionary exec_spawn({EXEC_PARAMS});
Dictionary exec_aoe({EXEC_PARAMS});

}} // namespace internal
}} // namespace sim::effects::execution

#endif // SIM_EFFECTS_EXEC_INTERNAL_HPP
"""
    (BASE / "sim_effects_exec_internal.hpp").write_text(internal_hpp, encoding="utf-8")

    for cat in ("damage", "status", "spawn", "aoe"):
        body = build_category_switch(blocks[cat])
        content = f"""{COMMON_INCLUDES}

namespace sim::effects::execution {{
namespace internal {{

Dictionary exec_{cat}({EXEC_PARAMS}) {{
{body}\tUtilityFunctions::push_error(vformat("exec_{cat}: unhandled opcode %d", effect.opcode));
\tDictionary fallback_result;
\tfallback_result["success"] = false;
\tfallback_result["error"] = "Unhandled opcode in exec_{cat}";
\treturn fallback_result;
}}

}} // namespace internal
}} // namespace sim::effects::execution
"""
        (BASE / f"sim_effects_exec_{cat}.cpp").write_text(content, encoding="utf-8")

    main_switch = build_case_blocks(blocks["main"]).rstrip()
    if main_switch.endswith("\t}"):
        main_switch = main_switch[:-2].rstrip() + "\n"

    main_cpp = f"""#include "sim_effects_exec.hpp"
#include "sim_effects_exec_internal.hpp"
#include "sim_effects_compile.hpp"

#include "sim_targeting.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::execution {{
namespace internal {{

Dictionary execute_recursive(
\t\tconst EffectRecord &effect,
\t\tEffectContext &context,
\t\tSimWorld &world,
\t\tSimHostCallbacks &host,
\t\tconst SimExecCallbacks &hooks,
\t\tconst ::sim::effects::SimMatchHost &match_host) {{
\treturn execute_impl(effect, context, world, host, hooks, match_host);
}}

{impl_preamble}\tswitch (effect.opcode) {{
{DELEGATE_CASES}{main_switch}\t}}
{tail}
}} // namespace internal

Dictionary execute(
\t\tconst EffectRecord &effect,
\t\tEffectContext &context,
\t\tSimWorld &world,
\t\tSimHostCallbacks &host,
\t\tconst SimExecCallbacks &hooks,
\t\t::sim::effects::SimMatchHost match_host) {{
\treturn internal::execute_impl(effect, context, world, host, hooks, match_host);
}}

}} // namespace sim::effects::execution
"""
    (BASE / "sim_effects_exec.cpp").write_text(main_cpp, encoding="utf-8")

    for path in [
        BASE / "sim_effects_exec.cpp",
        BASE / "sim_effects_exec_internal.hpp",
        BASE / "sim_effects_exec_damage.cpp",
        BASE / "sim_effects_exec_status.cpp",
        BASE / "sim_effects_exec_spawn.cpp",
        BASE / "sim_effects_exec_aoe.cpp",
    ]:
        count = len(path.read_text(encoding="utf-8").splitlines())
        print(f"{path.name}: {count}")


if __name__ == "__main__":
    main()
