/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Pathname utilities.
 */
#include "stdafx.h"
#include "PathName.h"
#include "Util.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <errno.h>

#ifndef S_ISREG
# define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

# ifndef F_OK
#  define F_OK  00
# endif
# ifndef R_OK
#  define R_OK  04
# endif


/*
 * ===========================================================================
 *		Filename utils
 * ===========================================================================
 */

#define kFilenameExtDelim	'.' 	/* separates extension from filename */

/*
 * Find the filename component of a local pathname.  Uses the fssep passed
 * in.  If the fssep is '\0' (as is the case for DOS 3.3), then the entire
 * pathname is returned.
 *
 * Always returns a pointer to a string; never returns nil.
 */
const char*
FilenameOnly(const char* pathname, char fssep)
{
	const char* retstr;
	const char* pSlash;
	char* tmpStr = nil;

	ASSERT(pathname != nil);
	if (fssep == '\0') {
		retstr = pathname;
		goto bail;
	}

	pSlash = strrchr(pathname, fssep);
	if (pSlash == nil) {
		retstr = pathname;		/* whole thing is the filename */
		goto bail;
	}

	pSlash++;
	if (*pSlash == '\0') {
		if (strlen(pathname) < 2) {
			retstr = pathname;	/* the pathname is just "/"?  Whatever */
			goto bail;
		}

		/* some bonehead put an fssep on the very end; back up before it */
		/* (not efficient, but this should be rare, and I'm feeling lazy) */
		tmpStr = strdup(pathname);
		tmpStr[strlen(pathname)-1] = '\0';
		pSlash = strrchr(tmpStr, fssep);

		if (pSlash == nil) {
			retstr = pathname;	/* just a filename with a '/' after it */
			goto bail;
		}

		pSlash++;
		if (*pSlash == '\0') {
			retstr = pathname;	/* I give up! */
			goto bail;
		}

		retstr = pathname + (pSlash - tmpStr);

	} else {
		retstr = pSlash;
	}

bail:
	free(tmpStr);
	return retstr;
}

/*
 * Return the filename extension found in a full pathname.
 *
 * An extension is the stuff following the last '.' in the filename.  If
 * there is nothing following the last '.', then there is no extension.
 *
 * Returns a pointer to the '.' preceding the extension, or nil if no
 * extension was found.
 *
 * We guarantee that there is at least one character after the '.'.
 */
const char*
FindExtension(const char* pathname, char fssep)
{
	const char* pFilename;
	const char* pExt;

	/*
	 * We have to isolate the filename so that we don't get excited
	 * about "/foo.bar/file".
	 */
	pFilename = FilenameOnly(pathname, fssep);
	ASSERT(pFilename != nil);
	pExt = strrchr(pFilename, kFilenameExtDelim);

	/* also check for "/blah/foo.", which doesn't count */
	if (pExt != nil && *(pExt+1) != '\0')
		return pExt;

	return nil;
}



/*
 * Get just the file name.
 */
CString
PathName::GetFileName(void)
{
	CString str;

	SplitIFN();
	str = fFileName;
	str += fExt;

	{
		const char* ccp;
		ccp = FilenameOnly(fPathName, '\\');
		if (strcmp(ccp, str) != 0) {
			WMSG2("NOTE: got different filenames '%s' vs '%s'\n",
				ccp, (LPCTSTR) str);
		}
	}

	return str;
}

/*
 * Get just the drive name.
 */
CString
PathName::GetDriveOnly(void)
{
	SplitIFN();

	return fDrive;
}

/*
 * Get directory names, prefixed with the drive.
 */
CString
PathName::GetDriveAndPath(void)
{
	CString str;

	SplitIFN();
	str = fDrive;
	str += fDir;

	return str;
}

/*
 * Get just the directory names.
 */
CString
PathName::GetPathOnly(void)
{
	SplitIFN();

	return fDir;
}

/*
 * Get just the extension.
 */
CString
PathName::GetExtension(void)
{
	SplitIFN();

	{
		const char* ccp;
		ccp = FindExtension(fPathName, '\\');
		if ((ccp == nil && strlen(fExt) > 0) ||
			(ccp != nil && strcmp(ccp, fExt) != 0))
		{
			WMSG2("NOTE: got different extensions '%s' vs '%s'\n",
				ccp, (LPCTSTR) fExt);
		}
	}

	return fExt;
}

/*
 * Expand the short file name of an existing file into its long form.
 *
 * Returns 0 on success, -1 on failure.
 */
int
PathName::SFNToLFN(void)
{
	char buf[MAX_PATH];
	WIN32_FIND_DATA findFileData;
	HANDLE hFind;
	char* cp;
	DWORD len;
	CString lfn;
	bool hadEndingSlash = false;

	lfn = "";
	if (fPathName.IsEmpty())
		return 0;

	/* fully expand it */
	len = ::GetFullPathName(fPathName, sizeof(buf), buf, &cp);
	if (len == 0 || len >= sizeof(buf))
		return -1;
	//WMSG1("  FullPathName='%s'\n", buf);

	if (buf[len-1] == '\\') {
		hadEndingSlash = true;
		buf[len-1] = '\0';
		len--;
	}

	/*
	 * Walk forward in the buffer, passing increasingly-long filenames into
	 * FindFirstFile.
	 */
	cp = buf;
	while (cp != buf + len) {
		if (*cp == '\\') {
			if (cp == buf) {
				/* just the leading '\'; shouldn't happen after GetFPN? */
				lfn += "\\";
			} else if (cp == buf+2 && *buf != '\\') {
				/* this is probably "C:\", which FindFF doesn't handle */
				*cp = '\0';
				lfn += buf;
				lfn += "\\";
				*cp = '\\';
			} else {
				*cp = '\0';
				hFind = ::FindFirstFile(buf, &findFileData);
				if (hFind == INVALID_HANDLE_VALUE) {
					DWORD err = ::GetLastError();
					WMSG2("FindFirstFile '%s' failed, err=%d\n", buf, err);
					return -1;
				} else {
					FindClose(hFind);
				}
				//WMSG2("  COMPONENT '%s' [%s]\n", findFileData.cFileName,
				//	findFileData.cAlternateFileName);
				lfn += findFileData.cFileName;
				lfn += "\\";
				*cp = '\\';
			}
		}

		cp++;
	}
	//WMSG1("  Interim name = '%s'\n", (LPCTSTR) lfn);

	if (*(cp-1) != '\\') {
		/* there was some stuff after the last '\\'; handle it */
		hFind = ::FindFirstFile(buf, &findFileData);
		if (hFind == INVALID_HANDLE_VALUE) {
			DWORD err = ::GetLastError();
			WMSG2("FindFirstFile '%s' failed, err=%d\n", buf, err);
			return -1;
		} else {
			FindClose(hFind);
		}
		//WMSG2("  COMPONENT2 '%s' [%s]\n", findFileData.cFileName,
		//	findFileData.cAlternateFileName);
		lfn += findFileData.cFileName;
	}

	//WMSG1("  Almost done = '%s'\n", (LPCTSTR) lfn);
	if (hadEndingSlash)
		lfn += "\\";

	fPathName = lfn;

	return 0;
}

/*
 * Return the description of the file type.
 */
CString
PathName::GetDescription()
{
	CString		szTypeName;
	SHFILEINFO	sfi;

	SHGetFileInfo(fPathName, 0, &sfi, sizeof(SHFILEINFO), SHGFI_TYPENAME);

	szTypeName = sfi.szTypeName;

	return szTypeName;
}

/*
 * Check to see if the file exists.
 *
 * If we use something simple like access(), we will catch all files including
 * the ones in Network Neighborhood.  Using the FindFirstFile stuff avoids
 * the problem, but raises the difficulty of being unable to find simple
 * things like "D:\".
 */
bool
PathName::Exists(void)
{
//	if (strncmp(fPathName, "\\\\", 2) == 0) {
//		WMSG1("Refusing to check for network path '%s'\n", fPathName);
//		return false;
//	}

	return (::access(fPathName, 0) != -1);

#if 0
	WIN32_FIND_DATA fd;
	bool result;

	CString	szFindPath = fPathName;
	int nSlash = szFindPath.ReverseFind('\\');

	if( nSlash == szFindPath.GetLength()-1)
	{
		szFindPath = szFindPath.Left(nSlash);
	}

	HANDLE hFind = FindFirstFile( szFindPath, &fd );

	if ( hFind != INVALID_HANDLE_VALUE )
	{
		FindClose( hFind );
	}

	result = (hFind != INVALID_HANDLE_VALUE);
#endif

#if 0
	if (::access(fPathName, 0) != -1) {
		/* exists */
		if (!result) {
			ASSERT(false);
		}
	} else {
		/* doesn't exist */
		if (result) {
			ASSERT(false);
		}
	}

	return result;
#endif
}

/*
Problem:
FindFirstFile returns INVALID_HANDLE_VALUE while seeking "C:"

my workaround:
instead of removing the backshash at the end of the path, i add a * to find any file in this folder.

FileName.cpp line 135:
replace
----------------------
	if( nSlash == szFindPath.GetLength()-1)
	{
		szFindPath = szFindPath.Left(nSlash);
	}
----------------------
with
----------------------
	if( nSlash == szFindPath.GetLength()-1)
	{
		szFindPath += "*";
	}
	else
		szFindPath += "\\*";
----------------------
*/

/*
 * Invoke the system-dependent directory creation function.
 */
int
PathName::Mkdir(const char* dir)
{
	int err = 0;

	ASSERT(dir != nil);

	if (mkdir(dir) < 0)
		err = errno ? errno : -1;

	return err;
}

/*
 * Determine if a file exists, and if so whether or not it's a directory.
 *
 * Set fields you're not interested in to nil.
 *
 * On success, returns 0 and fields are set appropriately.  On failure,
 * returns nonzero and result values are undefined.
 */
int
PathName::GetFileInfo(const char* pathname, struct stat* psb,
	time_t* pModWhen, bool* pExists, bool* pIsReadable, bool* pIsDirectory)
{
	struct stat sbuf;
	int cc;

	/*
	 * On base network path, e.g. \\webby\fadden, the stat() call fails
	 * with ENOENT, but the access() call succeeds.  Not sure if this
	 * can happen in other circumstances, so I'm not messing with it
	 * for now.
	 */
	//{
	//	int cc2 = access(pathname, 0);
	//}

	if (pModWhen != nil)
		*pModWhen = (time_t) -1;
	if (pExists != nil)
		*pExists = false;
	if (pIsReadable != nil)
		*pIsReadable = false;
	if (pIsDirectory != nil)
		*pIsDirectory = false;

	cc = stat(pathname, &sbuf);
	if (psb != nil)
		*psb = sbuf;
	if (cc != 0) {
		if (errno == ENOENT) {
			if (pExists != nil)
				*pExists = false;
			return 0;
		} else
			return errno;
	}

	if (pExists != nil)
		*pExists = true;

	if (pIsDirectory != nil && S_ISDIR(sbuf.st_mode))
		*pIsDirectory = true;
	if (pModWhen != nil)
		*pModWhen = sbuf.st_mtime;

	if (pIsReadable != nil) {
		/*
		 * Test if we can read this file.  How do we do that?  The easy but
		 * slow way is to call access(2), the harder way is to figure out
		 * what user/group we are and compare the appropriate file mode.
		 */
		if (access(pathname, R_OK) < 0)
			*pIsReadable = false;
		else
			*pIsReadable = true;
	}

	return 0;
}

/*
 * Check the status of a file.
 */
int
PathName::CheckFileStatus(struct stat* psb, bool* pExists, bool* pIsReadable,
	bool* pIsDir)
{
	return GetFileInfo(fPathName, psb, nil, pExists, pIsReadable, pIsDir);
}

/*
 * Get the modification date of a file.
 */
time_t
PathName::GetModWhen(void)
{
	time_t when;

	if (GetFileInfo(fPathName, nil, &when, nil, nil, nil) != 0)
		return (time_t) -1;

	return when;
}

/*
 * Set the modification date on a file.
 */
int
PathName::SetModWhen(time_t when)
{
	struct utimbuf utbuf;

	if (when == (time_t) -1 || when == kDateNone || when == kDateInvalid) {
		WMSG1("NOTE: not setting invalid date (%ld)\n", when);
		return 0;
	}

	utbuf.actime = utbuf.modtime = when;

	if (utime(fPathName, &utbuf) < 0)
		return errno;

	return 0;
}

/*
 * Create a single subdirectory if it doesn't exist.  If the next-highest
 * subdirectory level doesn't exist either, cut down the pathname and
 * recurse.
 *
 * "pathEnd" points at the last valid character.  The length of the valid
 * path component is therefore (pathEnd-pathStart+1).
 */
int
PathName::CreateSubdirIFN(const char* pathStart, const char* pathEnd,
	char fssep)
{
	int err = 0;
	char* tmpBuf = nil;
	bool isDirectory;
	bool exists;

	ASSERT(pathStart != nil);
	ASSERT(pathEnd != nil);
	ASSERT(fssep != '\0');

	/* pathStart might have whole path, but we only want up to "pathEnd" */
	tmpBuf = strdup(pathStart);
	tmpBuf[pathEnd - pathStart +1] = '\0';

	err = GetFileInfo(tmpBuf, nil, nil, &exists, nil, &isDirectory);
	if (err != 0) {
		WMSG1("  Could not get file info for '%s'\n", tmpBuf);
		goto bail;
	} else if (!exists) {
		/* dir doesn't exist; move up a level and check parent */
		pathEnd = strrchr(tmpBuf, fssep);
		if (pathEnd != nil) {
			pathEnd--;
			ASSERT(pathEnd >= tmpBuf);
			err = CreateSubdirIFN(tmpBuf, pathEnd, fssep);
			if (err != 0)
				goto bail;
		}

		/* parent is taken care of; create this one */
		err = Mkdir(tmpBuf);
		if (err != 0)
			goto bail;
	} else {
		/* file does exist, make sure it's a directory */
		if (!isDirectory) {
			WMSG1("Existing file '%s' is not a directory\n", tmpBuf);
			err = ENOTDIR;
			goto bail;
		}
	}

bail:
	free(tmpBuf);
	return err;
}

/*
 * Create subdirectories, if needed.  The paths leading up to the filename
 * in "pathname" will be created.
 *
 * If "pathname" is just a filename, or the set of directories matches
 * the last directory we created, we don't do anything.
 *
 * Returns 0 on success, or a Windows error code on failure.
 */
int
PathName::CreatePathIFN(void)
{
	int err = 0;
	CString pathName(fPathName);
	char* pathStart;
	const char* pathEnd;

	ASSERT(fFssep != '\0');

	pathStart = pathName.GetBuffer(0);
	/* BAD: network paths begin with "\\", not a drive letter */
//	if (pathStart[0] == fFssep)
//		pathStart++;

	/* remove trailing fssep */
	if (pathStart[strlen(pathStart)-1] == fFssep)
		pathStart[strlen(pathStart)-1] = '\0';

	/* work around bug in Win32 strrchr */
	if (pathStart[0] == '\0' || pathStart[1] == '\0') {
		err = EINVAL;
		goto bail;
	}

	pathEnd = strrchr(pathStart, fFssep);
	if (pathEnd == nil) {
		/* no subdirectory components found */
		goto bail;
	}
	pathEnd--;	// back up past the fssep

	ASSERT(pathEnd >= pathStart);
	if (pathEnd - pathStart < 0) {
		err = EINVAL;
		goto bail;
	}

	/*
	 * Special-case the root directory, e.g. "C:\", which needs the final
	 * slash to be recognized by Windows file calls.
	 */
	if (pathEnd - pathStart == 1 && pathStart[1] == ':' &&
		(toupper(pathStart[0]) >= 'A' && toupper(pathStart[0]) <= 'Z'))
	{
		pathEnd++;	// put it back on
	}

	/*
	 * Test to determine which directories exist.  The most likely case
	 * is that some or all of the components have already been created,
	 * so we start with the last one and work backward.
	 */
	err = CreateSubdirIFN(pathStart, pathEnd, fFssep);
	/* fall through with err */

bail:
	pathName.ReleaseBuffer();
	return err;
}
