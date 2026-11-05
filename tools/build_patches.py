#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Luma code.ips files for the games that won't go online by themselves.

Hand-written bytes at addresses other projects published; no game files are read
or stored.
"""
import struct
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "patches"
GALLERY = b"http://sl.badgecade.com:18100/fs3d"
GALLERY_MAX = 36
SERVER = "sl.badgecade.com:18100"
BOSS_BASE = 0x100000


def ips(records: list[tuple[int, bytes]]) -> bytes:
    out = b"PATCH"
    for offset, data in records:
        out += offset.to_bytes(3, "big") + struct.pack(">H", len(data)) + data
    return out + b"EOF"


def fs3d(nasc_branch: int, gallery_url: int) -> bytes:
    # no NASC check; our gallery address, plain HTTP like the notes, so no certificates are involved
    return ips([(nasc_branch, bytes(4)), (gallery_url, GALLERY.ljust(GALLERY_MAX, b"\0"))])


def boss() -> bytes:
    """System 11.17.0's SpotPass module (TMD 14337). Addresses from swapnote-revival."""
    assert len(SERVER) <= 22
    return ips([(0x10F088 - BOSS_BASE, bytes.fromhex("01E0")),  # always the plain http:// branch
                (0x147E59 - BOSS_BASE, SERVER.encode().ljust(23, b"\0")),  # HPP host suffix
                (0x1074F0 - BOSS_BASE, f"http://{SERVER}".encode().ljust(36, b"\0"))])  # policy list


PATCHES = {
    # System Settings, USA, TMD 11265: its birthday step asks our age server (string found in the real code)
    "settings-USA/0004001000021000": ips([(0x18F88C, f"http://{SERVER}/checkage".encode().ljust(44, b"\0"))]),
    "boss/0004013000003402": boss(),
    # ServiceStatus() returns 1: mov r0, #1 / bx lr (swapnote-revival, v3.3.0, code base 0x100000)
    "swapnote-USA/0004000000051700": ips([(0x1D874C - 0x100000, bytes.fromhex("0100A0E3" "1EFF2FE1"))]),
    # Swapdoodle v4.4.0: its SpotPass files come from us (checked against the game's code)
    "swapdoodle-USA/00040000001a2d00": ips([(0x1726A4, f"http://{SERVER}/p01/nsa/0hFlOFo7pNTU2dyE".encode().ljust(59, b"\0"))]),
    # offsets from KaeruTeam/fs3d-patch-tool; USA checked against the game, EUR and JPN not
    "fs3d-USA/00040000000c6600": fs3d(2133192, 2728724),
    "fs3d-EUR/00040000000c6700": fs3d(2133192, 2728724),
    "fs3d-JPN/0004000000056c00": fs3d(2229332, 2838432),
}

for name, data in PATCHES.items():
    path = OUT / name / "code.ips"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    print(path, len(data))
