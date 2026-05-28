from pathlib import Path

core_path = Path(__file__).resolve().parent.parent / "teamfight_simulation_core.cpp"
lines = core_path.read_text(encoding="utf-8").splitlines()
rs = next(i for i, l in enumerate(lines) if l.startswith("void TeamfightSimulationCore::_sim_profile_reset()"))
re = next(i for i, l in enumerate(lines) if l.startswith("void TeamfightSimulationCore::_sim_profile_emit_json_stderr()"))
re_end = next(i for i in range(re, len(lines)) if lines[i].startswith("void TeamfightSimulationCore::_prune_assist_window"))


def xform(body):
    out = []
    for l in body:
        s = l.lstrip()
        if s.startswith("_sim_"):
            out.append("\tcore." + s)
        else:
            out.append(l)
    return out


reset_body = xform(lines[rs + 1 : re])
emit_body = xform(lines[re + 1 : re_end])

cpp = (
    '#include "sim_profile.hpp"\n\n'
    '#include "../teamfight_simulation_core.hpp"\n\n'
    "#include <godot_cpp/classes/json.hpp>\n"
    "#include <godot_cpp/variant/utility_functions.hpp>\n\n"
    "using namespace godot;\n\n"
    "namespace sim::profile {\n\n"
    "void reset(TeamfightSimulationCore &core) {\n"
    + "\n".join(reset_body)
    + "\n}\n\n"
    "void emit_json_stderr(const TeamfightSimulationCore &core) {\n"
    + "\n".join(emit_body)
    + "\n}\n\n"
    "} // namespace sim::profile\n"
)

(Path(__file__).resolve().parent / "sim_profile.cpp").write_text(cpp, encoding="utf-8")

new_lines = (
    lines[:rs]
    + [
        "void TeamfightSimulationCore::_sim_profile_reset() {",
        "\tsim::profile::reset(*this);",
        "}",
        "",
        "void TeamfightSimulationCore::_sim_profile_emit_json_stderr() const {",
        "\tsim::profile::emit_json_stderr(*this);",
        "}",
        "",
    ]
    + lines[re_end:]
)
core_path.write_text("\n".join(new_lines) + "\n", encoding="utf-8")
print("profile extracted")
