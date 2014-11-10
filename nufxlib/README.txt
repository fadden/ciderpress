NufxLib README, updated 2004/03/18
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
configuration during testing is "-g -O2 -DDEBUG_MSGS", so that verbose
debug output is available when errors occur.

The flags are stuffed into Version.c, so the application program can
examine and display the flags that were used to build the library.


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

This works just like the UNIX version, with the exception that when you link
against nufxlib, your project must also link against the Carbon framework.
This can be done in ProjectBuilder by using the Add Framework option in the
Project menu, or by adding "-framework Carbon" to the gcc command line.

You'll see some warnings due to some namespace collisions between nufxlib and
Carbon, but everything will work fine.  Carbon is used to provide support for
file types and resource forks.


Win32
=====

If you're using an environment that supports "configure" scripts, such as
DJGPP, follow the UNIX instructions.

NufxLib has been tested with Microsoft Visual C++ 6.0.  To build NufxLib,
start up a DOS shell and run vcvars32.bat to set your environment.  Run:
    nmake -f makefile.msc
to build with debugging info, or
    nmake -f makefile.msc nodebug=1
to build optimized.

See the makefile for comments about including zlib or libbz2.  These
need to be enabled at compile time and linked into the sample apps.

Once the library has been built, "cd samples" and run the same command there.
When it finishes, run "test-basic.exe".

If you want to build NufxLib as a DLL, use "makefile.dll" instead.
If you're using zlib or libbz2, these will need to be linked into the DLL.
The makefile currently assumes that you will want to use zlib.dll.


Other Notes
===========

All of the source code is now formatted with spaces instead of tabs.

If you want to use the library in a multithreaded application, you should
define "USE_REENTRANT_CALLS" to tell it to use reentrant versions of
certain library calls.  This defines _REENTRANT, which causes Solaris to
add the appropriate goodies.  (Seems to me you'd always want this on, but
for some reason Solaris makes you take an extra step, so I'm not going to
define it by default.)


Legalese
========

NufxLib, a NuFX archive manipulation library.
Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.

See COPYING for license.

