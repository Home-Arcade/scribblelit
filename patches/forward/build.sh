#!/bin/sh
# cave.s -> forward_cave.h, the bytes both apps put in a game's code.ips
set -e
cd "$(dirname "$0")"
docker run --rm -v "$PWD":/w -w /w devkitpro/devkitarm sh -c 'B=/opt/devkitpro/devkitARM/bin/arm-none-eabi; $B-as -march=armv6k cave.s -o /tmp/c.o && $B-ld -Ttext=0 /tmp/c.o -o /tmp/c.elf && $B-objcopy -O binary /tmp/c.elf cave.bin && $B-nm /tmp/c.elf > cave.sym && chown 0:0 cave.bin cave.sym'
python3 -I - <<'P'
import re
sym={m[2]:int(m[0],16) for m in (l.split() for l in open('cave.sym')) if len(m)==3}
d=open('cave.bin','rb').read()
cut=sym['code_end']
code,table=d[:cut],d[cut:]
rows=lambda b:''.join('    '+', '.join('0x%02X'%x for x in b[i:i+16])+',\n' for i in range(0,len(b),16))
with open('../../app/source/forward_cave.h','w') as f:
    f.write('// SPDX-License-Identifier: GPL-3.0-or-later\n// Made by patches/forward/build.sh from cave.s.\n#pragma once\n')
    f.write('static const unsigned char FORWARD_CODE[] = {\n%s};\nstatic const unsigned char FORWARD_TABLE[] = {\n%s};\n'%(rows(code),rows(table)))
    f.write('#define FORWARD_TABLE_AT 0x%X  // in FORWARD_CODE: where the table is\n'%(sym['table_at']))
    for n,k in (('TID_LO','tid_lo'),('TID_HI','tid_hi'),('NEXT','next_check'),('SPAN','span'),('PARAM_AT','param_at'),('MAGIC','magic'),('GAME_LO','game_lo'),('GAME_HI','game_hi')):
        f.write('#define FORWARD_%s 0x%X  // in FORWARD_TABLE\n'%(n,sym[k]-cut))
print('code',hex(len(code)),'table',hex(len(table)))
P
