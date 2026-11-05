#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Makerom ignores the minor part of -ver: write the real title version into the CIA's TMD."""
import struct
import sys

version = int(sys.argv[1])
for path in sys.argv[2:]:
    data = bytearray(open(path, "rb").read())
    header, = struct.unpack_from("<I", data, 0)
    cert, ticket = struct.unpack_from("<II", data, 8)
    align = lambda n: (n + 0x3f) & ~0x3f
    tmd = align(header) + align(cert) + align(ticket)
    signature = {0x10000: 0x240, 0x10001: 0x140, 0x10002: 0x80, 0x10003: 0x240, 0x10004: 0x140, 0x10005: 0x80}[struct.unpack_from(">I", data, tmd)[0]]
    struct.pack_into(">H", data, tmd + signature + 0x9C, version)
    open(path, "wb").write(data)
