CiderPress Linux Utilities
==========================

CiderPress is a Windows app, but the code for accessing NuFX (ShrinkIt)
archives and disk images can be built as libraries on Linux and used
from applications.  A few samples are provided.  Most are really just
API demos, but they may come in handy.

You need to build NufxLib, the diskimg library, and then the samples.

Build Instructions
------------------

1. Configure and build NufxLib library

        cd nufxlib
        ./configure
        make

2. build diskimg library

        cd ../diskimg
        make

3. build libhfs library

        cd libhfs
        make

4. build samples

        cd ../../linux
        make

The build system could use some work.


Sample Programs
---------------

The current sample programs are:

`getfile disk-image filename` --
Extract a file from a disk image The file is written to stdout.

`makedisk {dos|prodos|pascal} size image-filename.po file1 ...` --
Create a new disk image, with the specified size and format, and copy the
specified files onto it.  The NON file type is used.

`mdc file1 ...` --
This is a Linux port of the MDC utility that ships with CiderPress.
It recursively scans all files and directories specified, displaying
the contents of any disk images it finds.


### Bonus Programs ###

`iconv infile outfile` --
Convert an image from one format to another.  This was used for testing.

`packddd infile outfile` --
The DDD code was originally developed under Linux.  This code is here
for historical reasons.

`sstasm part1 part2` --
The SST re-assembly code was originally developed under Linux.  The code
is here for historical reasons.

