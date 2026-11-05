#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""source/patches.h: the patches and the certificate the app carries."""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PATCHES = ROOT.parent / "patches"


def array(name, data):
    return f"static const unsigned char {name}[] = {{{','.join(map(str, data))}}};\n"


pem = Path("/etc/ssl/certs/ISRG_Root_X1.pem").read_bytes()
(ROOT / "source/letter_art.h").write_text(array("LETTER_JPEG", (ROOT / "build-meta/letter.jpg").read_bytes()))
(ROOT / "source/sl_patches.h").write_text(
    array("BOSS_IPS", (PATCHES / "boss/0004013000003402/code.ips").read_bytes())
    + array("SWAPNOTE_IPS", (PATCHES / "swapnote-USA/0004000000051700/code.ips").read_bytes())
    + array("SWAPDOODLE_IPS", (PATCHES / "swapdoodle-USA/00040000001a2d00/code.ips").read_bytes())
    + array("FS3D_IPS", (PATCHES / "fs3d-USA/00040000000c6600/code.ips").read_bytes())
    + array("SETTINGS_IPS", (PATCHES / "settings-USA/0004001000021000/code.ips").read_bytes())
    + array("ISRG_ROOT_X1", pem + b"\0"))
