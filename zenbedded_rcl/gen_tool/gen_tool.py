#!/usr/bin/env python3

# Copyright 2026 Open Source Robotics Foundation, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


"""
Payload generator for ros2_control URDF/xacro for use with zenbedded.

Parse a ros2_control URDF/xacro and emit a #pragma pack(push,1) C header
with per-interface sub-structs and a bundled RobotModulePayload, matching
the hand-written layout the user already has.

Usage:
    python3 generate_ros2_control_header.py robot.urdf > robot_payload.h
    python3 generate_ros2_control_header.py robot.urdf -o robot_payload.h
    python3 generate_ros2_control_header.py robot.urdf --xacro          # pre-process xacro first
    python3 generate_ros2_control_header.py robot.urdf --dry-run        # print layout table only
"""

import argparse
import subprocess
import sys
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path

# Maps interface types to (c_type, byte_size). // default to double if no match, since most interfaces are float-based.
INTERFACE_TYPE_MAP: dict = {
    "short": ("int16_t", 2),
    "ushort": ("uint16_t", 2),
    "int": ("int32_t", 4),
    "uint": ("uint32_t", 4),
    "long": ("int64_t", 8),
    "ulong": ("uint64_t", 8),
    "bool": ("uint8_t", 1),
    "float": ("float", 4),
    "double": ("double", 8),
}


@dataclass
class InterfaceField:
    c_name: str  # e.g. position
    c_type: str  # e.g. double
    byte_size: int


@dataclass
class ComponentStruct:
    struct_name: str  # e.g. Joint1
    component_name: str  # original URDF name
    component_type: str  # joint / sensor / gpio
    fields: list[InterfaceField] = field(default_factory=list)

    @property
    def byte_size(self) -> int:
        return sum(f.byte_size for f in self.fields)


@dataclass
class PayloadLayout:
    structs: list[ComponentStruct] = field(default_factory=list)
    # header fields always present
    timestamp_field: InterfaceField = field(
        default_factory=lambda: InterfaceField("timestamp_ns", "uint64_t", 8)
    )
    sequence_field: InterfaceField = field(
        default_factory=lambda: InterfaceField("sequence_id", "uint32_t", 4)
    )

    @property
    def payload_byte_size(self) -> int:
        return (
            self.timestamp_field.byte_size
            + self.sequence_field.byte_size
            + sum(s.byte_size for s in self.structs)
        )


# ── URDF parsing ──────────────────────────────────────────────────────────────


def load_xml(urdf_path: Path, use_xacro: bool) -> ET.Element:
    if use_xacro:
        result = subprocess.run(
            ["xacro", str(urdf_path)], capture_output=True, text=True, check=True
        )
        return ET.fromstring(result.stdout)
    return ET.parse(str(urdf_path)).getroot()


def to_c_ident(name: str) -> str:
    """my-joint 1 -> my_joint_1."""
    return re.sub(r"[^a-zA-Z0-9]", "_", name)


def pascal(s: str) -> str:
    """my_joint_1 -> 'MyJoint1."""
    return "".join(w.capitalize() for w in to_c_ident(s).split("_") if w)


def build_component_structs(
    component_name: str,
    component_type: str,
    cmd_interfaces: list[tuple[str, str]],
    state_interfaces: list[tuple[str, str]],
) -> list[ComponentStruct]:
    """
    Build two ComponentStructs from lists of command/state interface names.

    _cmd_t suffix for command structs, _state_t suffix for state structs.
    """
    state_struct = ComponentStruct(
        struct_name=f"{to_c_ident(component_name)}_state_t",
        component_name=component_name,
        component_type=component_type,
    )
    cmd_struct = ComponentStruct(
        struct_name=f"{to_c_ident(component_name)}_cmd_t",
        component_name=component_name,
        component_type=component_type,
    )

    cmd_seen: set[str] = set()
    state_seen: set[str] = set()

    def add_field(iface: tuple[str, str], iface_type: str):
        iface_name, iface_value_type = iface
        c_field = to_c_ident(iface_name)
        if (iface_type == "cmd" and c_field in cmd_seen) or (
            iface_type == "state" and c_field in state_seen
        ):
            raise ValueError(
                f"duplicate/similar interface name '{iface_name}' in component '{component_name}'"
            )

        if iface_type == "cmd":
            cmd_seen.add(c_field)
        elif iface_type == "state":
            state_seen.add(c_field)

        ctype, size = INTERFACE_TYPE_MAP.get(iface_value_type, ("double", 8))

        if iface_type == "cmd":
            cmd_struct.fields.append(InterfaceField(c_field, ctype, size))
        elif iface_type == "state":
            state_struct.fields.append(InterfaceField(c_field, ctype, size))

    for iface in cmd_interfaces:
        add_field(iface, "cmd")
    for iface in state_interfaces:
        add_field(iface, "state")

    return [cmd_struct, state_struct]


def parse_urdf(root: ET.Element) -> PayloadLayout:
    layout = PayloadLayout()

    for rc in root.iter("ros2_control"):
        for component in rc.iter():
            tag = component.tag.lower()
            if tag not in ("joint", "sensor", "gpio"):
                continue

            comp_name = component.get("name", "")

            if comp_name == "":
                raise ValueError(
                    f"ros2_control component missing 'name' attribute: {ET.tostring(component, encoding='unicode')}"
                )

            cmd_ifaces = [
                (ci.get("name", ""), ci.get("value_type", "double"))
                for ci in component.findall("command_interface")
            ]
            state_ifaces = [
                (si.get("name", ""), si.get("value_type", "double"))
                for si in component.findall("state_interface")
            ]

            structs = build_component_structs(comp_name, tag, cmd_ifaces, state_ifaces)
            layout.structs.extend(structs)

    return layout


# ── code generation ───────────────────────────────────────────────────────────

HEADER_BANNER = """\
/**
 * ZENBEDDED Tier 2 PAYLOAD HEADER
 * {filename}
 *
 * AUTO-GENERATED — do not edit by hand.
 * Source: {source}
 * Generated: {ts}
 *
 * Packed wire-format structs for Zephyr <-> ROS2 hardware interface over Zenoh (Tier 2).
 * Include on both sides (Zephyr module and ROS2 hardware interface).
 *
 * Guarantees:
 *   - #pragma pack(push, 1) — zero hidden padding bytes.
 *   - All multi-byte fields are little-endian (ARM LE, x86, RISC-V LE).
 *   - Fixed-width types only.
 *   - sizeof() stable across compilers.
 *
 * Total payload: {total_bytes} bytes
 */
"""


def field_line(f: InterfaceField, name_width: int) -> str:
    return f"  {f.c_type:<10} {f.c_name:<{name_width}}; // {f.byte_size} B"


def emit_struct(s: ComponentStruct) -> str:
    lines = [
        f"// {s.component_type}: {s.component_name}  ({s.byte_size} bytes total)",
        f"struct {s.struct_name} {{",
    ]
    name_w = max((len(f.c_name) for f in s.fields), default=10)
    for f in s.fields:
        lines.append(field_line(f, name_w))
    lines.append("};")
    return "\n".join(lines)


def emit_assert(struct_name: str, expected: int) -> str:
    return (
        f"static_assert(sizeof({struct_name}) == {expected},\n"
        f'              "{struct_name} size mismatch — check packing");'
    )


def generate_header(layout: PayloadLayout, source_name: str) -> str:
    filename = "payload.hpp"
    guard = "ZENBEDDED_RCL__GEN_TOOL__PAYLOAD_HPP_"
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")

    lines: list[str] = []

    # banner
    lines.append(
        HEADER_BANNER.format(
            filename=filename,
            source=source_name,
            ts=ts,
            total_bytes=layout.payload_byte_size,
        )
    )

    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("#ifdef __cplusplus")
    lines.append("#include <cstdint>")
    lines.append("#endif")
    lines.append("")

    # enforce packing
    lines.append("// Enforcing byte alignment — no hidden padding bytes inserted by compiler")
    lines.append("#pragma pack(push, 1)")
    lines.append("")

    # sub-structs
    lines.append(
        "// ── sub-structs for individual hardware components ───────────────────────────"
    )
    lines.append("")
    for s in layout.structs:
        lines.append(emit_struct(s))
        lines.append("")

    # master payload
    lines.append(
        "// ── master bundled payload ───────────────────────────────────────────────────"
    )
    lines.append(f"// Total size: {layout.payload_byte_size} bytes")
    lines.append("struct zenbedded_payload_t {")

    # header fields
    tf = layout.timestamp_field
    sf = layout.sequence_field
    hw = max(len(tf.c_name), len(sf.c_name), *(len(s.struct_name) for s in layout.structs), 14)
    lines.append(field_line(tf, hw))
    lines.append(field_line(sf, hw))
    lines.append("")
    lines.append("  // Embedded components")
    for s in layout.structs:
        lines.append(
            f"  {s.struct_name:<{hw + 2}} {to_c_ident(s.component_name):<20}; // {s.byte_size}B"
        )

    lines.append("};")
    lines.append("")

    # restore packing
    lines.append("#pragma pack(pop) // restore default packing")
    lines.append("")

    # static asserts
    lines.append(
        "// ── compile-time size assertions ──────────────────────────────────────────────"
    )
    lines.append("#ifdef __cplusplus")
    for s in layout.structs:
        lines.append(emit_assert(s.struct_name, s.byte_size))
    lines.append(emit_assert("zenbedded_payload_t", layout.payload_byte_size))
    lines.append("#endif // __cplusplus")
    lines.append("")

    lines.append(f"#endif // {guard}")
    return "\n".join(lines)


# ── layout table (--dry-run) ──────────────────────────────────────────────────


def print_layout_table(layout: PayloadLayout):
    print(f"{'Struct':<28} {'Field':<26} {'Type':<10} {'Bytes':>5}  Comment")
    print("─" * 90)

    def row(struct, field: InterfaceField):
        print(f"  {struct:<26} {field.c_name:<26} {field.c_type:<10} {field.byte_size:>5}")

    row("zenbedded_payload_t", layout.timestamp_field)
    row("zenbedded_payload_t", layout.sequence_field)
    for s in layout.structs:
        for f in s.fields:
            row(s.struct_name, f)

    print("─" * 90)
    print(f"  Total payload: {layout.payload_byte_size} bytes")


# ── CLI ───────────────────────────────────────────────────────────────────────


def main():
    ap = argparse.ArgumentParser(
        description="Generate a packed C header from a ros2_control URDF."
    )
    ap.add_argument("urdf", type=Path, help="Path to .urdf file")
    ap.add_argument(
        "-o", "--output", type=Path, default=None, help="Output file (default: stdout)"
    )
    ap.add_argument("--xacro", action="store_true", help="Pre-process with xacro before parsing")
    ap.add_argument(
        "--dry-run", action="store_true", help="Print layout table instead of generating header"
    )
    args = ap.parse_args()

    if not args.urdf.exists():
        print(f"error: file not found: {args.urdf}", file=sys.stderr)
        sys.exit(1)

    try:
        root = load_xml(args.urdf, args.xacro)
    except subprocess.CalledProcessError as e:
        print(f"xacro error:\n{e.stderr}", file=sys.stderr)
        sys.exit(1)
    except ET.ParseError as e:
        print(f"XML parse error: {e}", file=sys.stderr)
        sys.exit(1)

    layout = parse_urdf(root)

    if not layout.structs:
        print("warning: no ros2_control components found in URDF.", file=sys.stderr)
        print(
            "Make sure your URDF has <ros2_control> tags with <joint>/<sensor>/<gpio> children.",
            file=sys.stderr,
        )

    if args.dry_run:
        print_layout_table(layout)
        return

    header = generate_header(layout, str(args.urdf))

    if args.output:
        args.output.write_text(header + "\n")
        print(
            f"wrote {args.output}  ({layout.payload_byte_size} bytes total payload)",
            file=sys.stderr,
        )
    else:
        print(header)


if __name__ == "__main__":
    main()
