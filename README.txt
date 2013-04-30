Jedi Outcast with various changes to make it build/run on
more platforms including amd64/x86_64.

Currently only the single player code runs on amd64.

The game needs to be patched to 1.04 to work, the data in the
steam version is already patched.

The single player demo data also seems to be compatible
and runs seemingly fine.

	How to build single player:

mkdir build-sp && cd build-sp
cmake ../code/
make

copy jk2sp and jk2game*.so to the directory containing base or demo

	How to build multiplayer:

mkdir build-mp && cd build-mp
cmake ../CODE-mp/
make

copy jk2mp and jk2mpded to the directory containing base
copy *.so to your base directory

	Known issues:

With i386 linux with i965 graphics, levels seem to be zoomed in
to point where only textures are seen.  Does not seem to
affect GL rendered cutscenes or OpenBSD/i386 with slightly older mesa.

Save games do not yet work on amd64.
