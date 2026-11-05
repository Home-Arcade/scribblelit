# ScribbleLit

The 3DS app and Luma patches for ScribbleLit, which brings back friend notes in
Swapdoodle and Swapnote and a gallery for Flipnote Studio 3D. To use it, follow
the steps at https://sl.badgecade.com.

The servers that carry the notes and run the gallery aren't in this repo.
No game files are either. You need your own copy of each game.

    app/        the 3DS app, built with devkitARM
    patches/    the Luma IPS patches
    tools/      build_patches.py writes most of those patches

To build, you need Docker for the devkitpro/devkitarm image, plus makerom and
bannertool in /opt/badgecade/tools-3ds (edit `T` in app/build.sh):

    app/build.sh 0.2.9

## Thanks

The formats come from other people's work: swapnote-revival (NickOveracker), the
SwapdoodleRevival group and libdoodle, Pretendo Network, the Flipnote Collective's
docs and fs3d-patch-tool.

## License

GPL-3.0-or-later, with the extra terms in NOTICE. A fan project, not connected to Nintendo.
