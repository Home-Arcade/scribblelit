#!/bin/sh
# The BadgeCade helper carries ScribbleLit's menu: same code, drawn with its own screens.
cd "$(dirname "$0")/../source"
cp sl_core.c sl_core.h sl_find.c sl_find.h sl_forward.c sl_forward.h forward_cave.h sl_patches.h letter_art.h /opt/badgecade/badge-arcade-server/client-app/badgecade-setup/source/
