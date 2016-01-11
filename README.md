CiderPress
==========

A Windows utility for managing Apple II file archives and disk images.
Visit the web site at http://a2ciderpress.com/.

CiderPress was initially sold by faddenSoft, LLC as a shareware product,
starting in March 2003.  In March 2007, the program was released as
open source under the BSD license.  A "refresh" to modernize the code was
done in January 2015.

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
just build the entire thing.  The project files have been updated so
that VS2015 Community Edition will accept them, but the new "universal CRT"
causes problems with WinXP, so the build files still require the older
set of tools.

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
Linux.  See the [Linux README](README-linux.md).


Source Notes
------------

Some notes on what you'll find in the various directories.

#### Main Application ####

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

#### MDC Application ####

MDC (Multi-Disk Catalog) was written as a simple demonstration of the
value of having the DiskImg code in a DLL instead of meshed with the main
application.  There's not much to it, and it hasn't changed substantially
since it was first written.

#### DiskImg Library ####

This library provides access to disk images.  It automatically handles
a wide variety of formats.

This library can be built under Linux or Windows. One of my key motivations
for making it work under Linux was the availability of "valgrind". Similar
tools for Windows usually very expensive or inferior (or both).

An overview of the library can be found in the
[DiskImg README](diskimg/README.md).

The library depends on NufxLib and zlib for access to compressed images.

#### Reformat Library ####

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

#### Util Library ####

Miscellaneous utility functions.

#### NufxLib and zlib ####

These are source snapshots from [NufxLib](http://github.com/fadden/nulib2)
and [zlib](http://www.zlib.org).

#### DIST ####

Files used when making a distribution, notably:

- the DeployMaster configuration file
- the license and README files that are included in the installer
- redistributable Windows runtime libraries (only needed on WinXP?)
- NiftyList data file


Future Trouble Spots
--------------------

Microsoft generally does an excellent job of maintaining backward
compatibility, but as Windows and the build tools continue to evolve it is
likely that some things will break.  The original version of CiderPress was
written to work on Win98, using tools of that era, and quite a bit of effort
in the 4.0 release was devoted to bringing CP into the modern era.

In another 15 years things may be broken all over again.  Some areas of
particular concern:

1. File + folder selection.  The dialog that allows you to select a combination
of files and folders is a customized version of the standard file dialog.
There is no standard dialog that works for this.  The original version, based
on the old Win98-era file dialogs, worked fine at first but started to fail
in Vista.  CiderPress v4.0 provided a new implementation, based on the
Win2K "explorer" dialog, which works well in WinXP and Win7/8.  However,
WinVista introduced a new style, and those dialogs have a very different
structure (and don't work on WinXP).  At some point it may be necessary
to replace the dialog again.

2. Help files.  CiderPress initially used the old WinHelp system.  v4.0
switched to the newer HtmlHelp, but judging by the level of support it would
seem that HtmlHelp is on its way out.  The favored approach seems to be to
just pop open a web browser to a web site or a document on disk.  The pop-up
help text, which currently comes out of a special section of the help file,
would instead use MFC tooltip features, with strings coming out of the
resource file.  (This is probably more convenient and definitely more
flexible, so switching the pop-up help messages may happen sooner.)

3. Unicode filenames.  CiderPress cannot open most files with non-ASCII,
non-CP-1252 characters in their names (e.g. kanji).  This is because the
NufxLib and DiskImg libraries use narrow strings for filenames.  The libraries
are expected to build on Linux, so converting them is a bit of a pain.  At
some point it may be necessary to support Unicode fully.  v4.0 did a lot of
code reorganization to make this easier, as did NufxLib v3.0.

4. Windows XP support.  The default Visual Studio 2013 configuration creates
executables that do not work in Windows XP.  CiderPress uses a compatibility
toolset and packs about 5MB of additional DLLs (mfc120u.dll, msvcr120.dll) in
the install package to keep things working.  Visual Studio 2015 shipped with a
new "Universal CRT" that requires more effort and disk space.  At some point
it may not be possible to support WinXP, or building for WinXP will prevent
something from working.  The good news is that, for the current round of
tools, it's possible to build a single binary that works fully on WinXP and
later systems.

5. Installer magic.  Security improvements and changes like the Win8 "Metro"
launcher affect the way apps are installed and launched.  So far the only
impact on CiderPress was to the file association handling (the stuff that
allows you to double-click a file and have CiderPress open it), but it's
likely that future OS changes will require matching app changes.  The use
of DeployMaster is helpful here, as it has been kept up-to-date with changes
in Windows.
