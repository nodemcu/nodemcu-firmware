These are MSVC (Visual Studio 2017) project files for the host-side tools,
namely 'luac.cross' and 'spiffsimg'.  Some may find these convenient if they
already have MSVC instead of, say, setting up a Cygwin or MingW build
system.

To build 'luac.cross', you must first edit app/include/user_config.h to make
some choices about the kind of cross-compiler you are generating.

In particular, the definition of
    LUA_FLASH_STORE
should be enabled if you are creating a cross-compiler for generating images
for the Lua File Storage (LFS).  The specific value of this define is not
critical for luac.cross, but it's existence is if you want to be able to
generate appropriate code for LFS.

Be aware that the codebase, as checked in, has LUA_FLASH_STORE undefined.
Since it is expected that most folks wanting a host-side luac.cross is
for LFS use, you will want to first make sure that is changed to be
defined.

Secondly, if you are wanting to generate code that is appropriate for an
integer-only build, you should ensure that
    LUA_NUMBER_INTEGRAL
is defined.

After altering those settings, you can build using the hosttools.sln file in
the Visual Studio UI, or directly on the command line.  x86 and x64 targets
are provisioned, though there isn't anything to be gained with the 64-bit
build.

