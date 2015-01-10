NufxLib NOTES
=============
Last revised: 2015/01/04


The interface is documented in "nufxlibapi.html", available from the
http://www.nulib.com/ web site.  This discusses some of the internal
design that may be of interest.

Some familiarity with the NuFX file format is assumed.

- - -

### Read-Write Data Structures ###

For both read-only and read-write files (but not streaming read-only files),
the archive is represented internally as a linked list of Records, each
of which has an array of Threads attached.  No attempt is made to
optimize searches by filename, so use of the "replace existing entry when
filenames match" option should be restricted to situations where it is
necessary.  Otherwise, O(N^2) behavior can result.

Modifications, such as deletions, changes to filename threads, and
additions of new records, are queued up in a separate list until a NuFlush
call is issued.  The list works much the same way as the temporary file:
when the operation completes, the "new" list becomes the "original" list.
If the operation is aborted, the "new" list is scrubbed, and the "original"
list remains unmodified.

Just as it is inefficient to write data to the temp file when it's not
necessary to do so, it is inefficient to allocate a complete copy of the
records from the original list if none are changed.  As a result, there are
actually two "new" lists, one with a copy of the original record list, and
one with new additions.  The "copy" list starts out uninitialized, and
remains that way until one of the entries from the original list is
modified.  When that happens, the entire original list is duplicated, and
the changes are made directly to members of the "copy" list.  (This is
important for really large archives, like a by-file archive with the
entire contents of a hard drive, where the record index could be several
megabytes in size.)

It would be more *memory* efficient to simply maintain a list of what
has changed.  However, we can't disturb the "original" list in any way or
we lose the ability to roll back quickly if the operation is aborted.
Consequently, we need to create a new list of records that reflects
the state of the new archive, so that when we rename the temp file over
the original, we can simply "rename" the new record list over the original.
Since we're going to need the new list eventually, we might as well create
it as soon as it is needed, and deal with memory allocation failures up
front rather than during the update process.  (Some items, such as the
record's file offset in the archive, have to be updated even for records
that aren't themselves changing... which means we potentially need to
modify all existing record structures, so we need a complete copy of the
record list regardless of how little or how much has changed.)

This also ties into the "modify original archive file directly if possible"
option, which avoids the need for creating and renaming a temp file.  If
the only changes are updates to pre-sized records (e.g. renaming a file
inside the archive, or updating a comment), or adding new records onto the
end, there is little risk and possibly a huge efficiency gain in just
modifying the archive in place.  If none of the operations caused the
"copy" list to be initialized, then clearly there's no need to write to a
temp file.  (It's not actually this simple, because updates to pre-sized
threads are annotated in the "copy" list.)

One of the goals was to be able to execute a sequence of operations like:

    open original archive
    read original archive
    modify archive
    flush (success)
    modify archive
    flush (failure, rollback)
    modify archive
    flush (success)
    close archive

The archive is opened at the start and held open across many operations.
There is never a need to re-read the entire archive.  We could avoid the
need to allocate two complete Record lists by requiring that the archive be
re-scanned after changes are aborted; if we did that, we could just modify
the original record list in place, and let the changes become "permanent"
after a successful write.  In many ways, though, its cleaner to have two
lists.

Archives with several thousand entries should be sufficiently rare, and
virtual memory should be sufficiently plentiful, that this won't be a
problem for anyone.  Scanning repeatedly through a 15MB archive stored on a
CD-ROM is likely to be very annoying though, so the design makes every
attempt to avoid repeated scans of the archive.  And in any event, this
only applies to archive updates.  The memory requirements for simple file
extraction are minimal.

In summary:

  - "orig" list has original set of records, and is not disturbed until
    the changes are committed.
  - "copy" list is created on first add/update/delete operation, and
    initially contains a complete copy of "orig".
  - "new" list contains all new additions to the archive, including
    new additions that replace existing entries (the existing entry
    is deleted from "copy" and then added to "new").


Each Record in the list has a "thread modification" list attached to it.
Any changes to the record header or additions to the thread mod list are
made in the "copy" set; the "original" set remains untouched.  The thread
mod list can have the following items in it:

  - delete thread (NuThreadIdx)
  - add thread (type, otherSize, format, +contents)
  - update pre-sized thread (NuThreadIdx, +contents)

Contents are specified with a NuDataSource, which allows the application
to indicate that the data is already compressed.  This is useful for
copying parts of records between archives without having to expand and
recompress the data.

Some interactions and concepts that are important to understand:

  When a file is added, the file type information will be placed in the
  "new" Record immediately (subject to some restrictions: adding a data
  fork always causes the type info to be updated, adding a rsrc fork only
  updates the type info if a data fork is not already present).

  Deleting a record results in the Record being removed from the "copy"
  list immediately.  Future modify operations on that NuRecordIdx will
  fail.  Future read operations will work just fine until the next
  NuFlush is issued, because read operations use the "original" list.

  Deleting all threads from a record results in the record being
  deleted, but not until the NuFlush call is issued.  It is possible to
  delete all the existing threads and then add new ones.

  It is *not* allowed to delete a modified thread, modify a deleted thread,
  or delete a record that has been modified.  This limitation was added to
  keep the system simple.  Note this does not mean you can't delete a data
  fork and add a data fork; doing so results in operations on two threads
  with different NuThreadIdx values.  What you can't do is update the
  filename thread and then delete it, or vice-versa.  (If anyone can think
  of a reason why you'd want to rename a file and then delete it with the
  same NuFlush call, I'll figure out a way to support it.)

  Updating a filename thread is intercepted, and causes the Record's
  filename cache to be updated as well.  Adding a filename thread for
  records where the filename is stored in the record itself cause the
  "in-record" filename to be zeroed.  Adding a filename thread to a
  record that already has one isn't allowed; nufxlib restricts you to
  a single filename thread per record.

  Some actions on an archive are allowed but strongly discouraged.  For
  example, deleting a filename thread but leaving the data threads behind
  is a valid thing to do, but leaves most archivers in a state of mild
  confusion.  Deleting the data threads but leaving the filename thread is
  similarly perplexing.

  You can't call "update thread" on a thread that doesn't yet exist,
  even if an "add thread" call has been made.  You can, however, call
  "add thread" on a newly created Record.

When a new record is created because of a "create record" call, a filename
thread is created automatically.  It is not necessary to explicitly add the
filename.

Failures encountered while committing changes to a record cause all
operations on that record to be rolled back.  If, during a NuFlush, a
file add fails, the user is given the option of aborting the entire
operation or skipping the file in question (and perhaps retrying or other
options as well).  Aborting the flush causes a complete rollback.  If only
the thread mod operation is canceled, then all thread mods for that record
are ignored.  The temp file (or archive file) will have its file pointer
reset to the original start of the record, and if the record already
existed in the original archive, the full original record will be copied
over.  This may seem drastic, but it helps ensure that you don't end up
with a record in a partially created state.

If a failure occurs during an "update in place", it isn't possible to
roll back all changes.  If the failure was due to a bug in NufxLib, it
is possible that the archive could be unrecoverably damaged.  NufxLib
tries to identify such situations, and will leave the archive open in
read-only mode after rolling back any new file additions.

- - -

### Updating Filenames ###

Updating filenames is a small nightmare, because the filename can be
either in the record header or in a filename thread.  It's possible,
but illogical, to have a single record with a filename in the record
header and two or more filenames in threads.

NufxLib will not automatically "fix" broken records, but it will prevent
applications from creating situations that should not exist.

  - When reading an archive, NufxLib will use the filename from the
  first filename thread found.  If no filename threads are found, the
  filename from the record header will be used.

  - If you add a filename thread to a record that has a filename in the
  record header, the header name will be removed.

  - If you update a filename thread in a record that has a filename in
  the record header, the header name will be left untouched.

  - Adding a filename thread is only allowed if no filename thread exists,
  or all existing filename threads have been deleted.


- - -

### Unicode Filenames ###

Modern operating systems support filenames with a broader range of
characters than the Apple II did.  This presents problems and opportunities.

#### Background ####

The Apple IIgs and old Macintoshes use the Mac OS Roman ("MOR") character
set.  This defines a set of characters outside the ASCII range, i.e.
byte values with the high bit set.  In addition to the usual collection
of vowels with accents and umlauts, MOR has some less-common characters,
including the Apple logo.

On Windows, the high-ASCII values are generally interpreted according
to Windows Code Page 1252 ("CP-1252"), which defines a similar set
of vowels with accents and miscellaneous symbols.  MOR and CP-1252
have some overlap, but you can't really translate one into the other.
The standards-approved equivalent of CP-1252 is ISO-8859-1, though
according to [wikipedia](http://en.wikipedia.org/wiki/Windows-1252)
there was some confusion between the two.

Modern operating systems support the Unicode Universal Character Set.
This system allows for a very large number of characters (over a million),
and includes definitions for all of the symbols in MOR and CP-1252.
Each character is assigned a "code point", which is a numeric value between
zero and 0x10FFFF.  Most of the characters used in modern languages can
be found in the Basic Multilingual Plane (BMP), which uses code points
between zero and 0xFFFF (requiring only 16 bits).

There are different ways of encoding code points.  Consider, for example,
Unicode LATIN SMALL LETTER A WITH ACUTE:

    MOR: 0x87
    CP-1252: 0xE1
    Unicode: U+00E1
    UTF-16: 0x00E1
    UTF-8: 0xC3 0xA1

Or the humble TRADE MARK SIGN:

    MOR: 0xAA
    CP-1252: 0x99
    Unicode: U+2122
    UTF-16: 0x2122
    UTF-8: 0xE2 0x84 0xA2

Modern Linux and Mac OS X use UTF-8 encoding in filenames.  Because it's a
byte-oriented encoding, and 7-bit ASCII values are trivially represented
as 7-bit ASCII values, all of the existing system and library calls work
as they did before (i.e. if they took a `char*`, they still do).

Windows uses UTF-16, which requires at least 16 bits per code point.
Filenames are now "wide" strings, based on `wchar_t*`.  Windows includes
an elaborate system of defines based around the `TCHAR` type, which can
be either `char` or `wchar_t` depending on whether a program is compiled
with `_MBCS` (Multi-Byte Character System) or `_UNICODE`.  A set of
preprocessor definitions is provided that will map I/O function names,
so you can call `_tfopen(TCHAR* ...)`, and the compiler will turn it into
either `fopen(char* ...)` or `_wfopen(wchar_t* ...)`.  MBCS is deprecated
in favor of Unicode, so any new code should be strictly UTF-16 based.

This means that, for code to work on both Linux and Windows, it has to
work with incompatible filename string types and different I/O functions.

#### Opening Archive Files ####

On Linux and Mac OS X, NuLib2 can open any file named on the command line.
On Windows, it's a bit trickier.

The problem is that NuLib2 provides a `main()` function that is passed a
vector of "narrow" strings.  The filenames provided on the command line
will be converted from wide to narrow, so unless the filename is entirely
composed of ASCII or CP-1252 characters, some information will be lost
and it will be impossible to open the file.

NuLib2 must instead provide a `wmain()` function that takes wide strings.
The strings must be stored and passed around as wide throughout the
program, and passed into NufxLib this way (because NufxLib issues the
actual _wopen call).  This means that NufxLib API must take narrow strings
when built for Linux, and wide strings when built for Windows.

#### Adding/Extracting Mac OS Roman Files ####

GS/ShrinkIt was designed to handle GS/OS files from HFS volumes, so NuFX
archive filenames use the MOR character set.  To preserve the encoding
we could simply extract the values as-is and let them appear as whatever
values happen to line up in CP-1252, which is what pre-3.0 NuLib2 did.
It's much nicer to translate from MOR to Unicode when extracting, and
convert back from Unicode to MOR when adding files to an archive.

The key consideration is that the character set associated with a
filename must be tracked.  The code can't simply extract a filename from
the archive and pass it to a 'creat()` call.  Character set conversions
must take place at appropriate times.

With Windows it's a bit harder to confuse MOR and Unicode names, because
one uses 8-bit characters and the other uses UTF-16, but the compiler
doesn't catch everything.

#### Current State ####

NufxLib defines the UNICHAR type, which has a role very like TCHAR:
it can be `char*` or `wchar_t*`, and can be accompanied by a set of
preprocessor mappings that switch between I/O functions.  The UNICHAR
type will be determined based on a define provided from the compiler
command line (perhaps `-DUSE_UTF16_FILENAMES`).

The current version of NufxLib (v3.0.0) takes the first step, defining
all filename strings as either UNICHAR or MOR, and converting between them
as necessary.  This, plus a few minor tweaks to NuLib2, was enough to
get Unicode filename support working on Linux and Mac OS X.

None of the work needed to make Windows work properly has been done.
The string conversion functions are no-ops for Win32.  As a result,
NuLib2 for Windows treats filenames the same way in 3.x as it did in 2.x.

There are some situations where things can go awry even with UNICHAR,
most notably printf-style arguments.  These are checked by gcc, but
not by Visual Studio unless you run the static analyzer.  A simple
`printf("filename=%s\n", filename)` would be correct for narrow strings
but wrong for wide strings.  It will likely be necessary to define a
filename format string (similar to `PRI64d` for 64-bit values) and switch
between "%s" and "%ls".

This is a fair bit of work and requires some amount of uglification to
NuLib2 and NufxLib.  Since Windows users can use CiderPress, and the
vast majority of NuFX archives use ASCII-only ProDOS file names, it's
not clear that the effort would be worthwhile.

