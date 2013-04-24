Jedi Outcast with various changes to make it build/run on
more platforms including amd64/x86_64.

Currently only the single player code is built.

The game needs to be patched to 1.04 to work, the data in the
steam version is already patched.

The single player demo data also seems to be compatible
and runs seemingly fine.

	How to build:

mkdir build-sp && cd build-sp
cmake ../code/
make

copy jk2sp and jk2game*.so to your game data directory

	Known issues:

When running windowed the mouse does not work in the menus.

With i386 linux with i965 graphics, levels seem to be zoomed in
to point where only textures are seen.  Does not seem to
affect GL rendered cutscenes or OpenBSD/i386 with slightly older mesa.

Save games do not yet work on amd64.
