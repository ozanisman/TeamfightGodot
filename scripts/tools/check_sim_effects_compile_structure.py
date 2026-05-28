#!/usr/bin/env python3
"""Guard against try_fill_* return true outside kind blocks (Iter 5d regression)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NATIVE_SIM = ROOT / "native" / "src" / "simulation"
CATEGORY_FILES = [
    "sim_effects_compile_damage.cpp",
    "sim_effects_compile_status.cpp",
    "sim_effects_compile_aoe.cpp",
    "sim_effects_compile_spawn.cpp",
]


def check_file(path: Path) -> list[str]:
    errors: list[str] = []
    text = path.read_text(encoding="utf-8")
    func_pat = re.compile(r"bool\s+try_fill_\w+\([^)]*\)\s*\{", re.MULTILINE)
    for match in func_pat.finditer(text):
        start = match.end()
        depth = 1
        i = start
        while i < len(text) and depth > 0:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
            i += 1
        body = text[start : i - 1]
        depth = 0
        kind_if_min_depth = -1
        base_line = text[:start].count("\n") + 2
        for offset, line in enumerate(body.splitlines()):
            line_no = base_line + offset
            compact = line.replace(" ", "")
            if "if(kind==" in compact:
                kind_if_min_depth = depth + line.count("{")
            if line.strip() == "return true;" and kind_if_min_depth < 0:
                errors.append(
                    f"{path.name}:{line_no}: bare `return true;` outside `if (kind == ...)` "
                    "(possible compile parameter mismatch)"
                )
            depth += line.count("{") - line.count("}")
            if kind_if_min_depth >= 0 and depth < kind_if_min_depth:
                kind_if_min_depth = -1
    return errors


def main() -> int:
    all_errors: list[str] = []
    for name in CATEGORY_FILES:
        path = NATIVE_SIM / name
        if not path.is_file():
            all_errors.append(f"missing {path}")
            continue
        all_errors.extend(check_file(path))
    if all_errors:
        for err in all_errors:
            print(err, file=sys.stderr)
        return 1
    print("check_sim_effects_compile_structure: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
