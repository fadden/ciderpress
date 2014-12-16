CiderPress Disk Image Library
=============================

This library provides access to files stored in Apple II disk images.  It
was developed as part of CiderPress, but can be used independently.  It
builds on Windows and Linux.

The MDC (Multi-Disk Catalog) application uses the DiskImg DLL (on Windows)
or library (on Linux) to examine disk image files.


Disk Image Structure
--------------------

The Apple II supported a number of different filesystems and physical
formats.  There are several different disk image formats, some supported
by Apple II software, some only usable by emulators.  This section
provides a quick summary.

#### Filesystems ####

- DOS 3.2/3.3.  The classic Apple II filesystems.  The typical DOS 3.3
disk has 35 tracks, 16 sectors per track, 256 bytes per sector, for a
total of 140K.  The format allows up to 32 sectors and 50 tracks (400K).
It worked very well on 5.25" floppy disks, but was awkward to use on
larger volumes.  Filenames could be up to 30 characters, and sometimes
embedded control characters or used flashing/inverse character values.

- ProDOS.  Designed to work on larger media, ProDOS addresses data as
512-byte blocks, and leaves any track/sector mapping to device-specific
code.  Block numbers were stored as unsigned 16-bit values, allowing
volumes up to 32MB.  Filenames were limited to 15 upper-case or numeric
ASCII values.  Later versions added support for forked files and
case-preserved (but still case-insensitive) filenames.

- UCSD Pascal.  Another block-oriented filesystem, used with the UCSD
Pascal operating system.  Very simple, and very efficient when reading,
but required explicit defragmentation from time to time.

- HFS.  Originally developed for the Macintosh, it was often used on
the Apple IIgs as hard drive sizes increased.  HFS supports forked files,
and "MacRoman" filenames up to 31 characters.

- CP/M.  Z-80 cards allowed Apple II users to run CP/M software, using
the established CP/M filesystem layout.  It featured 1K blocks and 8.3
filenames.

- SSI RDOS.  A custom format developed by Strategic Simulations, Inc. for
use with their games.  This was used on 13-sector and 16-sector 5.25"
disks.  The operating system used Applesoft ampersand commands, and was
ported to ProDOS.

- Gutenberg.  A custom format developed by Micromation Limited for use
with the Gutenberg word processor.

DOS, ProDOS, HFS, and UCSD Pascal are fully supported by the DiskImg
library.  CP/M, RDOS, and Gutenberg are treated as read-only.


#### Disk Image Formats ####

Disk image files can be "unadorned", meaning they're just a series of
blocks or sectors, or they can have fancy headers and compressed contents.
Block-oriented images are easy to deal with, as they're generally just
the blocks in sequential order.  Sector-oriented images can be tricky,
because the sector order is subject to interpretation.

The most common sector orderings are "DOS" and "ProDOS".  If you read
a 5.25" disk sequentially from DOS, starting with track 0 sector 0, and
wrote the contents to a file, you would end up with a DOS-ordered image.
If you read that same disk from ProDOS, starting with block 0, you would
end up with a ProDOS-ordered image.  The difference occurs because ProDOS
blocks are 512 bytes -- two DOS sectors -- and ProDOS interleaves the
sectors as an optimization.  While you might expect ProDOS block 0 to be
comprised of DOS T0 S0 and T0 S1, it's actually T0 S0 and T0 S2.

There have been various attempts at defining storage formats for disk
images over the years.  The library handles most of them.

- Unadorned block/sector files (.po, .do, .d13, .raw, .hdv, .iso, most .dc6).
The image file holds data from the file and nothing else.

- Unadorned nibble-format files (.nib, .nb2).  Some 5.25" disks were a
bit "creative" with their physical format, so some image formats allow
for extraction of the data as bits directly off the disk.  Such formats
are unusual in that it's possible to have "bad sectors" in a disk image.

- Universal Disk Image (.2mg, .2img).  The format was designed specifically
for Apple II emulators.  It supports DOS-order, ProDOS-order, and nibble
images.

- Copy ][ Plus (.img).  Certain versions of the Copy ][ Plus Apple II
utility had the ability to create disk images.  The format was simple
unadorned sectors, but with a twist: the sectors were in physical order,
which is different from DOS and ProDOS.

- Dalton's Disk Disintegrator (.ddd).  DDD was developed as an alternative
to "disk slicer" programs for uploading disk images to BBS systems.
Because it used compression, 5.25" disk images could be held on 5.25" disks.
DOS and ProDOS versions of the program were available.  A fancier version,
called DDD Deluxe, was developed later.

- ShrinkIt (.shk, .sdk).  ShrinkIt was initially developed as an improved
version of DDD, incorporating LZW compression and CRC error checking.  It
grew into a general-purpose file archiver.

- DiskCopy 4.2 (.dsk).  The format used by the Mac DiskCopy program for
making images of 800K floppies.  Includes a checksum, but no compression.

- TrackStar (.app).  The TrackStar was essentially an Apple II built
into a PC ISA expansion card.  The 5.25" disk images use a variable-length
nibble format with 40 tracks.

- Formatted Disk Image (.fdi).  Files generated by the Disk2FDI program.
These contain raw signal data obtained from a PC floppy drive.  It was
long said that reading an Apple II floppy from a PC drive was impossible;
this program proved otherwise.

- Sim //e HDV (.hdv).  Used for images of ProDOS drives for use with a
specific emulator, this is just a ProDOS block image with a short header.

All of these, with the exception of DDD Deluxe, are fully supported by
the DiskImg library.


#### Meta-Formats ####

As disks got larger, older filesystems could no longer use all of the
available space with a single volume.  Some "meta-formats" were developed.
These allow a single disk image to hold multiple filesystems.

- UNIDOS / AmDOS / OzDOS.  These allow two 400K DOS 3.3 volumes to
exist on a single 800K disk.

- ProSel Uni-DOS / DOS Master.  These allow multiple 140K DOS 3.3
volumes to reside on a ProDOS volume.  You can, on a single 800K disk,
provide a small ProDOS launcher that will boot into DOS 3.3 and launch a
specific file from one of five 140K DOS volumes.

- Macintosh-style disk partitioning.  This was widely used on hard
drives, CD-ROMs, and other large disks.

- CFFA-style disk partitioning.  The CFFA card for the Apple II allows
the use of Compact Flash cards for storage.  There is no partitioning
done on the CF card itself -- the CFFA card has a fixed arrangement.

- ///SHH Systeme MicroDrive partitioning.  Another disk partition format,
developed for use with the MicroDrive.  Allows up to 16 partitions on
an IDE hard drive.

- Parsons Engineering FocusDrive partitioning.  Developed for use with
the FocusDrive.  Allows up to 30 partitions.

It's also possible to create a "hybrid" DOS / ProDOS disk, because the
essential file catalog areas don't overlap.  (See HYBRID.CREATE on the
Beagle Bros "Extra K" disk.)

All of these are fully supported by the DiskImg library.


#### Wrapper Formats ####

While 800K may not seem like a lot these days, it used to be a decent
chunk of data, so it was common for disk images that didn't use compression
to be compressed with another program.  The most common are ZIP and gzip.


#### Apple II File Formats ####

For filesystems like ProDOS, the body of the file contains just the file
contents.  The directory entry holds the file type, auxiliary type, and
the file's length in bytes.

For DOS 3.2/3.3, the first few bytes of the file specified details like
the load address, and for text files there is no reliable indication of
the file length.  The DiskImg library does what it can to conceal
filesystem quirks.



DiskImg Library Classes
-----------------------

The library provides several C++ classes.  This section gives an overview
of what each does.

The basic classes are defined in DiskImg.h.  Specialized sub-classes are
declared in DiskImgDetail.h.

The API is a bit more complicated than it could be, and there's a bit of
redundancy in the filesystem code.  Some of this is due to the way the
library evolved -- disk image access was originally intended to be read-only,
and that didn't change until CiderPress 2.0.


#### DiskImg ####

The `DiskImg` class represents a single disk image.  It may have sub-images,
each of which is its own instance of DiskImg.  Operations on a DiskImg
are similar to what you'd expect from a device driver, e.g. reading and
writing individual blocks.

The DiskImg has several characteristics:

- OuterFormat.  This is the "outer wrapper", which may be ZIP (.zip) or
gzip (.gz).  The wrapper is handled transparently -- the contents are
uncompressed when opened, and recompressed if necessary when closed.

- FileFormat.  The disk image file's format, once the outer wrapper has
been stripped away.  "Unadorned", 2MG, and ShrinkIt are examples.

- PhysicalFormat.  Identifies whether the data is "raw" or "cooked", i.e.
if it's a series of blocks/sectors or raw nibbles.

- SectorOrder.  ProDOS, DOS, Physical.

- FSFormat.  What type of filesystem is in this image.  This includes
common formats, like DOS 3.3 and ProDOS, as well as meta-formats like
UNIDOS and Macintosh partition.


#### DiskFS ####

A `DiskFS` instance is typically paired with a DiskImg.  It represents
the filesystem, operating at a level roughly equivalent to a GS/OS
File System Translator (FST).

Using DiskFS, you can read, write, and rename files, format disks,
check how much free space is available, and search for sub-volumes.


#### A2File ####

One instance of `A2File` represents one file on disk.  The object holds
the filename and attributes, and provides a call to open the file.


#### A2FileDescr ####

This is essentially a file descriptor for an Apple II file.  You can
read or write data.

To make the implementation easier, files must be written with a single
write call, and only one fork of a forked file may be open.

