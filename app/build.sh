#!/bin/sh
# ScribbleLit.cia (with its updater inside). app.txt tells installed apps about it.
set -e
cd "$(dirname "$0")"
VERSION=${1:-0.2.9}
T=/opt/badgecade/tools-3ds
BT=$T/bannertool/bannertool-1.2.3-linux/bannertool
IFS=. read -r MAJOR MINOR MICRO <<VER
$VERSION
VER
TITLE_VERSION=$(( (MAJOR << 10) | (MINOR << 4) | MICRO ))
mkdir -p build-meta romfs
python3 -I ../tools/build_patches.py >/dev/null
python3 -I tools/make_art.py
python3 -I tools/make_header.py
$BT makebanner -i build-meta/banner.png -a build-meta/silence.wav -o build-meta/banner.bnr
$BT makesmdh -s ScribbleLit -l "Friend notes for your 3DS" -p ScribbleLit -i build-meta/icon.png -f visible,allow3d -o build-meta/icon.icn
$BT makesmdh -s "ScribbleLit updater" -l "Updates ScribbleLit" -p ScribbleLit -i build-meta/icon.png -o build-meta/updater.icn
docker run --rm -v "$PWD":/src -w /src -e APP_VERSION="$VERSION" devkitpro/devkitarm sh -c 'make clean >/dev/null; make -j4 2>&1 | grep -E "error|warning: (impl|unused)|Error " ; chown -R 0:0 .'
# the updater's own RomFs is this folder, so an old copy would get packed inside the new one
rm -f romfs/updater.cia
$T/makerom/makerom -f cia -o romfs/updater.cia -elf SLUpdater.elf -rsf meta/updater.rsf -icon build-meta/updater.icn -banner build-meta/banner.bnr -exefslogo -target t -ver "$TITLE_VERSION"
$T/makerom/makerom -f cia -o ScribbleLit.cia -elf ScribbleLit.elf -rsf meta/app.rsf -icon build-meta/icon.icn -banner build-meta/banner.bnr -exefslogo -target t -ver "$TITLE_VERSION"
python3 -I tools/fix_version.py "$TITLE_VERSION" ScribbleLit.cia
echo "$VERSION $(sha256sum ScribbleLit.cia | cut -d' ' -f1)" > build-meta/app.txt
ls -la ScribbleLit.cia
cat build-meta/app.txt
