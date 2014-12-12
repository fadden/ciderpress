CiderPress
==========

A Windows utility for managing Apple II file archives and disk images.

CiderPress was initially sold by faddenSoft, LLC as a shareware product,
starting in March 2003.  In March 2007, the program was released as
open source under the BSD license.

- - -

**UNDER CONSTRUCTION**

I'm in the process of updating CiderPress to work with newer tools (VS
2013) and operating systems.

Current status:
- Win32 main application properly executes basic functions.  Not well tested.
- MDC application seems to work; not well tested.
- Linux code seems to work; not well tested.
- The web documentation has been moved to github (see the gh-pages branch)
  and is available from http://a2ciderpress.com/.

- - -

Why Bother?
-----------

Back in 2002 I decided it was time to learn how to write an application
for Microsoft Windows. I had been a professional software engineer for
many years -- including 2.5 years at Microsoft! -- but had never written
a Windows program more complex than "Hello, world!".

I decided to write a Windows version of GS/ShrinkIt. I had already written
NufxLib, which handled all of the ShrinkIt stuff, so I could focus on
writing the Windows user interface code.

Somewhere in the early stages of the project, it occurred to me that a
disk image isn't substantially different from a file archive. They're
both just collections of files laid out in a well-defined manner. The
decision to handle disk images as well as ShrinkIt archives seemed like
a simple improvement at the time. The rest is history.

CiderPress has allowed me to explore a variety of interesting
technologies. It has five different ways of reading a block from physical
media, depending on your operating system and what sort of device you're
reading from. I was able to take what I learned from a digital signal
processing textbook and apply it to a real-world problem (decoding Apple
II cassette data). It is also my first Shareware product, not to mention
the initial product of my first small business venture (faddenSoft, LLC).

I could have written other things. No doubt they would have made more
money. CiderPress is something that I find very useful, however, in the
pursuit of my Apple II hobby.

Above all, this has been a labor of love. I have tried to get the details
right, because in the end it's the little things that mean the difference
between "good" and merely "good enough".


Source License
--------------

The source code to CiderPress is available under the BSD license.  See
the file [LICENSE.txt](LICENSE.txt) for details.

CiderPress requires three other libraries, all of which are included as
source code:

- NufxLib, also available under the BSD license.
- Zlib, available under the Zlib license.
- libhfs, available under the GPL license.

The license allows you to do a great many things. For example, you could
take the source code to CiderPress, compile it, and sell it. I'm not sure
why anyone would buy it, but you're legally allowed to do so, as long as
you retain the appropriate copyright notice.

If you retain libhfs, any changes you make to any part of CiderPress must
be made available, due to the "viral" nature of the GPL license. If this
is not acceptable, you can remove HFS disk image support from CiderPress
(look for "EXCISE_GPL_CODE" in DiskImg.h).


Building the Sources
--------------------

The current version of CiderPress is targeted for Visual Studio 2013,
using the WinXP compatibility Platform Toolset to allow installation on
Windows XP systems.  You should be able to select Debug or Release and
just build the entire thing.

If you want to use the static analyzer, you will need to change the
Platform Toolset to straight Visual Studio 2013.

A pre-compiled .CHM file, with the help text and pop-up messages,
is provided.  The source files are all included, but generation of the
.CHM is not part of the build.  If you want to update the help files,
you will need to download the HTML Help Workshop from Microsoft, and use
that to compile the help project in the app/Help directory.

The installer binary is created with [DeployMaster](http://deploymaster.com/).


Building for Linux
------------------

The NuFX archive and disk image manipulation libraries can be used from
Linux.  See the [Linux README](ReadMe-linux.md).


Source Notes
------------

Some notes on what you'll find in the various directories.

### Main Application ###

This is highly Windows-centric.  My goal was to learn how to write a
Windows application, so I made no pretense at portability.  For better
or worse, I avoided the Visual Studio "wizards" for the dialogs.

Much of the user interface text is in the resource file.  Much is not,
especially when it comes to error messages.  This will need to be addressed
if internationalization is attempted.

It may be possible to convert this for use with wxWidgets, which uses an
MFC-like structure, and runs on Mac and Linux as well. The greatest barrier
to entry is probably the heavy reliance on the Rich Edit control. Despite
its bug-ridden history, the Rich Edit control allowed me to let Windows
deal with a lot of text formatting and image display stuff.

### MDC Application ###

MDC (Multi-Disk Catalog) was written as a simple demonstration of the
value of having the DiskImg code in a DLL instead of meshed with the main
application.  There's not much to it, and it hasn't changed substantially
since it was first written.

### DiskImg Library ###

This library provides access to disk images.  It automatically handles
a wide variety of formats.

This library can be built under Linux or Windows. One of my key motivations
for making it work under Linux was the availability of "valgrind". Similar
tools for Windows usually very expensive or inferior (or both).

The basic classes, defined in DiskImg.h, are:

- DiskImg.  This represents a single disk image, which may have sub-images.
Operations on a DiskImg are roughly equivalent to a device driver:
you can read and write blocks, detect image formats, and create new
images.
- DiskFS.  Paired with a DiskImg, this is roughly equivalent to
a GS/OS FST (FileSystem Translator).  You can perform file operations
like rename and delete, format disks, see how much space is available,
and search for sub-volumes.
- A2File.  Represents a file on a DiskFS.  This holds the file's name,
attributes, track/sector or block lists, and provides a call to open
a file.
- A2FileDescr.  Represents an open file.  You can read or write data.
Sub-classes are defined in DiskImgDetail.h.  Most applications won't need
to access this header file.  Each Apple II filesystem defines sub-classes
of DiskFS, A2File, and A2FileDescr.

In an ideal world, the code would mimic the GS/OS file operations.
In practice, CiderPress didn't need the full range of capabilities,
so the functions have some basic limitations:

- On ProDOS and HFS, you can only open one fork at a time. This allowed me to use simpler data structures.
- Files are expected to be written in one large chunk. This reduced the complexity of the task enormously, because there's so much less that can go wrong.

Some things that could be improved:

- The overall structure of the filesystem handlers evolved over time. There is some amount of redundancy that could be factored out.
- The API, especially in DiskImg, could probably be simplified.

The library depends on NufxLib and zlib for access to compressed images.

### Reformat Library ###

This is probably the most "fun" component of CiderPress. It converts
Apple II files to more easily accessible Windows equivalents.

Start in Reformat.h and ReformatBase.h. There are two basic kinds of
reformatter: text and graphics. Everything else is a sub-class of one of
the two basic types.

The general idea is to allow the reformatter to decide whether or
not it is capable of reformatting a file. To this end, the file type
information and file contents are presented to the "examine" function
of each reformatter in turn. The level of confidence is specified in a
range. If it's better than "no", it is presented to the user as an option,
ordered by the strength of its convictions. If chosen, the "process"
function is called to convert the data.

Bear in mind that reformatters may be disabled from the preferences menu.
Also, when extracting files for easy access in Windows, the "best"
reformatter is employed by the extraction code.

Most of the code should be portable, though some of it uses the MFC
CString class.  This could probably be altered to use STL strings or plain.

### Util Library ###

Miscellaneous utility functions.

