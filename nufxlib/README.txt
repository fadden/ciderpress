NufxLib README, updated 2014/12/23
http://www.nulib.com/

See "COPYING-LIB" for distribution restrictions.


UNIX
====

Run the "configure" script.  Read through "INSTALL" if you haven't used
one of these before, especially if you want to use a specific compiler
or a particular set of compiler flags.

You can disable specific compression methods with "--disable-METHOD"
(run "sh ./configure --help" to see the possible options).  By default,
all methods are enabled except bzip2.

Run "make depend" if you have makedepend, and then type "make".  This will
build the library and all of the programs in the "samples" directory.
There are some useful programs in "samples", described in a README.txt
file there.  In particular, you should run samples/test-basic to verify
that things are more or less working.

If you want to install the library and header file into standard system
locations (usually /usr/local), run "make install".  To learn how to
specify different locations, read the INSTALL document.

There are some flags in "OPT" you may want to use.  The "autoconf" default
for @CFLAGS@ is "-g -O2".

-DNDEBUG
  Disable assert() calls and extra tests.  This will speed things up,
  but errors won't get caught until later on, making the root cause
  harder to locate.

-DDEBUG_MSGS
  Enable debug messages.  This increases the size of the executable,
  but shouldn't affect performance.  When errors occur, more output is
  produced.  The "debug dump" feature is enabled by this flag.

-DDEBUG_VERBOSE
  (Implicitly sets DEBUG_MSGS.)  Spray lots of debugging output.

If you want to do benchmarks, use "-O2 -DNDEBUG".  The recommended
configuration is "-g -O2 -DDEBUG_MSGS", so that verbose debug output is
available when errors occur.


BeOS
====

This works just like the UNIX version, but certain defaults have been
changed.  Running configure without arguments under BeOS is equivalent to:

    ./configure --prefix=/boot --includedir='${prefix}/develop/headers'
      --libdir='${exec_prefix}/home/config/lib' --mandir='/tmp'
      --bindir='${exec_prefix}/home/config/bin'

If you're using BeOS/PPC, it will also do:

    CC=cc CFLAGS='-proc 603 -opt full'


Mac OS X
========

This works just like the UNIX version, but includes support for resource
forks and Finder file/aux types.

Tested with Xcode v5.1.1 and Mac OS 10.8.5.


Win32
=====

If you're using an environment that supports "configure" scripts, such as
DJGPP, follow the UNIX instructions.

NufxLib has been tested with Microsoft Visual C++ 12 (Visual Studio 2013).
To build NufxLib, run the "Visual Studio 2013 x86 Native Tools Command
Prompt" shortcut to get a shell.  Change to the nufxlib directory, then:

    nmake -f makefile.msc

When the build finishes, run "test-basic.exe" to confirm things are working.

If you want to have zlib support enabled, you will need to have zlib.h,
zconf.h, and zlib.lib copied into the directory.  See "makefile.msc" for
more details.

The makefile builds NufxLib as a static library and as a DLL.


Other Notes
===========

If you want to use the library in a multithreaded application, you should
define "USE_REENTRANT_CALLS" to tell it to use reentrant versions of
certain library calls.  This defines _REENTRANT, which causes Solaris to
add the appropriate goodies.  (Seems to me you'd always want this on, but
for some reason Solaris makes you take an extra step, so I'm not going to
define it by default.)

Originally, NufxLib / NuLib2 were intended to be usable natively on the
Apple IIgs, so some of the design decisions were influenced by the need
to minimize memory usage (e.g. being able to get a directory listing
without holding the entire directory in memory) and interact with GS/OS
(forked files have a single filename, files have type/auxtype).  The IIgs
port was never started.


Legalese
========

NufxLib, a NuFX archive manipulation library.
Copyright (C) 2000-2014 by Andy McFadden, All Rights Reserved.

See COPYING for license.

