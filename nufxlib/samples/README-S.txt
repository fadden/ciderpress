NufxLib "samples" README

This directory contains some test programs and useful sample code.


test-basic
==========

Basic tests.  Run this to verify that things are working.

On Win32 there will be a second executable, test-basic-d, that links against
the DLL rather than the static library.


exerciser
=========

This program allows you to exercise all of NufxLib's basic functions.
Run it without arguments and hit "?" for a list of commands.

If you think you have found a bug in NufxLib, you can use this to narrow
it down to a repeatable case.


imgconv
=======

A 2IMG disk image converter.  You can convert ".2MG" files to ShrinkIt
disk archives, and ShrinkIt disk archives to 2IMG format.  imgconv uses
a creator type of "NFXL".

You can use it like this:

  % imgconv file.shk file.2mg
or
  % imgconv file.2mg file.shk

It figures out what to do based on the filename.  It will recognize ".sdk"
as a ShrinkIt archive.

Limitations: works for DOS-ordered and ProDOS-ordered 2MG images, but
not for raw nibble images.  Converting from .shk only works if the first
record in the archive is a disk image; you don't get to pick the one you
want from an archive with several in it.


launder
=======

Run an archive through the laundry.  This copies the entire contents of
an archive thread-by-thread, reconstructing it such that the data
matches the original even if the archive contents don't (e.g. records
are updated to version 3, files may be recompressed with LZW/2, option
lists are stripped out, etc).

The basic usage is:

  % launder [-crfa] [-m method] infile.shk outfile.shk

The flags are:

  -c  Just copy data threads rather than recompressing them
  -r  Add threads in reverse order
  -f  Call NuFlush after every record
  -a  Call NuAbort after every record, then re-do the record and call NuFlush
  -t  Write to temp file, instead of writing directly into outfile.shk

The "-m method" flag allows you to specify the compression method.  Valid
values are sq (SQueeze), lzw1 (ShrinkIt LZW/1), lzw2 (ShrinkIt LZW/2),
lzc12 (12-bit UNIX "compress"), lzc16 (16-bit UNIX "compress"), deflate
(zlib deflate), and bzip2 (libbz2 compression).  The default is lzw2.

If you use the "-c" flag with an archive created by P8 ShrinkIt or NuLib,
the laundered archive may have CRC failures when you try to extract
from it.  This is because "launder" creates version 3 records, which
are expected to have a valid CRC in the thread header.  The only way
to compute the CRC is to uncompress the data, which "launder" doesn't
do when "-c" is set.  The data itself is fine, it's just the thread CRC
that's wrong (if the data were hosed, the LZW/1 CRC would be bad too).
"launder" will issue a warning when it detects this situation.

By default, launder will try to keep the entire archive in memory and flush
all of the operations at the end.  If you find that you're running out
of memory on very large archives, you can reduce the memory requirements
by specifying the "-f" flag.


test-names
==========

Tests Unicode filename handling.  Run without arguments.

(This currently fails on Win32 because the Unicode filename support is
incomplete there.)


test-simple
===========

Simple test program.  Give it the name of an archive, and it will display
the contents.


test-extract
============

Simple test program.  Give it the name of an archive, and it will write
all filename threads into "out.buf", "out.fp", and "out.file" using three
different kinds of NuDataSinks.


test-twirl
==========

Like "launder", but not meant to be useful.  This recompresses the file "in
place", deleting and adding threads within existing records several times.
The changes are periodically flushed, but the archive is never closed.
The goal is to test repeated updates on an open archive.

The CRC verification mechanism will fail on archives created with ProDOS
8 ShrinkIt.  The older "version 1" records didn't have CRCs in the thread
headers, so you will get a series of messages that look like this:

ERROR: CRC mismatch:     0 old=0x0000 new=0x681b
ERROR: CRC mismatch:     1 old=0x0000 new=0x5570
ERROR: CRC mismatch:     2 old=0x0000 new=0x4ec5

This will leave the original archive alone, making a copy of it named
"TwirlCopy678" in the current directory.  It overwrites its temp file,
"TwirlTmp789", without prompting.

