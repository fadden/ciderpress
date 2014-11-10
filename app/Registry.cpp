/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Windows Registry operations.
 */
#include "stdafx.h"
#include "Registry.h"
#include "Main.h"
#include "MyApp.h"

#define kRegAppName     L"CiderPress"
#define kRegExeName     L"CiderPress.exe"
#define kCompanyName    L"faddenSoft"

static const WCHAR kRegKeyCPKVersions[] = L"vrs";
static const WCHAR kRegKeyCPKExpire[] = L"epr";

/*
 * Application path.  Add two keys:
 *
 *  (default) = FullPathName
 *      Full pathname of the executable file.
 *  Path = Path
 *      The $PATH that will be in effect when the program starts (but only if
 *      launched from the Windows explorer).
 */
static const WCHAR kAppKeyBase[] =
    L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" kRegExeName;

/*
 * Local settings.  App stuff goes in the per-user key, registration info is
 * in the per-machine key.
 */
static const WCHAR kMachineSettingsBaseKey[] =
    L"HKEY_LOCAL_MACHINE\\SOFTWARE\\" kCompanyName L"\\" kRegAppName;
static const WCHAR kUserSettingsBaseKey[] =
    L"HKEY_CURRENT_USER\\Software\\" kCompanyName L"\\" kRegAppName;

/*
 * Set this key + ".XXX" to (Default)=AppID.  This associates the file
 * type with kRegAppID.
 */
//static const char* kFileExtensionBase = L"HKEY_CLASSES_ROOT";

/*
 * Description of data files.  Set this key + AppID to 40-char string, e.g.
 * (Default)=CompanyName AppName Version DataType
 *
 * Can also set DefaultIcon = Pathname [,Index]
 */
//static const char* kAppIDBase = L"HKEY_CLASSES_ROOT";

/*
 * Put one of these under the AppID to specify the icon for a file type.
 */
static const WCHAR* kDefaultIcon = L"DefaultIcon";

static const WCHAR* kRegKeyCPKStr = L"CPK";

/*
 * Table of file type associations.  They will appear in the UI in the same
 * order that they appear here, so try to maintain alphabetical order.
 */
#define kAppIDNuFX      L"CiderPress.NuFX"
#define kAppIDDiskImage L"CiderPress.DiskImage"
#define kAppIDBinaryII  L"CiderPress.BinaryII"
#define kNoAssociation  L"(no association)"
const MyRegistry::FileTypeAssoc MyRegistry::kFileTypeAssoc[] = {
    { L".2MG",  kAppIDDiskImage },
    { L".APP",  kAppIDDiskImage },
    { L".BNY",  kAppIDBinaryII },
    { L".BQY",  kAppIDBinaryII },
    { L".BSE",  kAppIDNuFX },
    { L".BXY",  kAppIDNuFX },
    { L".D13",  kAppIDDiskImage },
    { L".DDD",  kAppIDDiskImage },
    { L".DO",   kAppIDDiskImage },
    { L".DSK",  kAppIDDiskImage },
    { L".FDI",  kAppIDDiskImage },
    { L".HDV",  kAppIDDiskImage },
    { L".IMG",  kAppIDDiskImage },
    { L".NIB",  kAppIDDiskImage },
    { L".PO",   kAppIDDiskImage },
    { L".SDK",  kAppIDDiskImage },
    { L".SEA",  kAppIDNuFX },
    { L".SHK",  kAppIDNuFX },
//  { L".DC",   kAppIDDiskImage },
//  { L".DC6",  kAppIDDiskImage },
//  { L".GZ",   kAppIDDiskImage },
//  { L".NB2",  kAppIDDiskImage },
//  { L".RAW",  kAppIDDiskImage },
//  { L".ZIP",  kAppIDDiskImage },
};

static const struct {
    const char* user;
    const char* reg;
} gBadKeys[] = {
    { "Nimrod Bonehead", "CP1-68C069-62CC9444" },
    { "Connie Tan", "CP1-877B2C-A428FFD6" },
};


/*
 * ==========================================================================
 *      One-time install/uninstall
 * ==========================================================================
 */

/*
 * This is called immediately after installation finishes.
 *
 * We want to snatch up any unused file type associations.  We define them
 * as "unused" if the entry does not exist in the registry at all.  A more
 * thorough installer would also verify that the appID actually existed
 * and "steal" any apparent orphans, but we can let the user do that manually.
 */
void
MyRegistry::OneTimeInstall(void) const
{
    /* start by stomping on our appIDs */
    WMSG0(" Removing appIDs\n");
    RegDeleteKeyNT(HKEY_CLASSES_ROOT, kAppIDNuFX);
    RegDeleteKeyNT(HKEY_CLASSES_ROOT, kAppIDDiskImage);
    RegDeleteKeyNT(HKEY_CLASSES_ROOT, kAppIDBinaryII);

    /* configure the appIDs */
    FixBasicSettings();

    /* configure extensions */
    int i, res;
    for (i = 0; i < NELEM(kFileTypeAssoc); i++) {
        HKEY hExtKey;
        res = RegOpenKeyEx(HKEY_CLASSES_ROOT, kFileTypeAssoc[i].ext, 0,
                KEY_READ, &hExtKey);
        if (res == ERROR_SUCCESS) {
            WMSG1(" Found existing HKCR\\'%ls', leaving alone\n",
                kFileTypeAssoc[i].ext);
            RegCloseKey(hExtKey);
        } else if (res == ERROR_FILE_NOT_FOUND) {
            OwnExtension(kFileTypeAssoc[i].ext, kFileTypeAssoc[i].appID);
        } else {
            WMSG2(" Got error %ld opening HKCR\\'%ls', leaving alone\n",
                res, kFileTypeAssoc[i].ext);
        }
    }
}

/*
 * Remove things that the standard uninstall script won't.
 *
 * We want to un-set any of our file associations.  We don't really need to
 * clean up the ".xxx" entries, because removing their appID entries is enough
 * to fry their little brains, but it's probably the right thing to do.
 *
 * We definitely want to strip out our appIDs.
 */
void
MyRegistry::OneTimeUninstall(void) const
{
    /* drop any associations we hold */
    int i;
    for (i = 0; i < NELEM(kFileTypeAssoc); i++) {
        CString ext, handler;
        bool ours;

        GetFileAssoc(i, &ext, &handler, &ours);
        if (ours) {
            DisownExtension(ext);
        }
    }

    /* remove our appIDs */
    WMSG0(" Removing appIDs\n");
    RegDeleteKeyNT(HKEY_CLASSES_ROOT, kAppIDNuFX);
    RegDeleteKeyNT(HKEY_CLASSES_ROOT, kAppIDDiskImage);
    RegDeleteKeyNT(HKEY_CLASSES_ROOT, kAppIDBinaryII);
}


/*
 * ==========================================================================
 *      Shareware registration logic
 * ==========================================================================
 */

/* [removed] */

/*
 * ==========================================================================
 *      Windows shell game
 * ==========================================================================
 */

/*
 * Return the application's registry key.  This is used as the argument to
 * CWinApp::SetRegistryKey().  The GetProfile{Int,String} calls combine this
 * (in m_pszRegistryKey) with the app name (in m_pszProfileName) and prepend
 * "HKEY_CURRENT_USER\Software\".
 */
const WCHAR*
MyRegistry::GetAppRegistryKey(void) const
{
    return kCompanyName;
}

/*
 * See if an AppID is one we recognize.
 */
bool
MyRegistry::IsOurAppID(const WCHAR* id) const
{
    return (wcsicmp(id, kAppIDNuFX) == 0 ||
            wcsicmp(id, kAppIDDiskImage) == 0 ||
            wcsicmp(id, kAppIDBinaryII) == 0);
}

/*
 * Fix the basic registry settings, e.g. our AppID classes.
 *
 * We don't overwrite values that already exist.  We want to hold on to the
 * installer's settings, which should get whacked if the program is
 * uninstalled or reinstalled.  This is here for "installer-less" environments
 * and to cope with registry damage.
 */
void
MyRegistry::FixBasicSettings(void) const
{
    const WCHAR* exeName = gMyApp.GetExeFileName();
    ASSERT(exeName != nil && wcslen(exeName) > 0);

    WMSG0("Fixing any missing file type AppID entries in registry\n");

    ConfigureAppID(kAppIDNuFX, L"NuFX Archive (CiderPress)", exeName, 1);
    ConfigureAppID(kAppIDBinaryII, L"Binary II (CiderPress)", exeName, 2);
    ConfigureAppID(kAppIDDiskImage, L"Disk Image (CiderPress)", exeName, 3);
}

/*
 * Set up the registry goodies for one appID.
 */
void
MyRegistry::ConfigureAppID(const WCHAR* appID, const WCHAR* descr,
    const WCHAR* exeName, int iconIdx) const
{
    WMSG2(" Configuring '%ls' for '%ls'\n", appID, exeName);

    HKEY hAppKey = nil;
    HKEY hIconKey = nil;

    DWORD dw;
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT, appID, 0, REG_NONE,
        REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
        &hAppKey, &dw) == ERROR_SUCCESS)
    {
        ConfigureAppIDSubFields(hAppKey, descr, exeName);

        if (RegCreateKeyEx(hAppKey, kDefaultIcon, 0, REG_NONE,
            REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
            &hIconKey, &dw) == ERROR_SUCCESS)
        {
            DWORD type, size;
            unsigned char buf[512];
            long res;

            size = sizeof(buf);     // size in bytes
            res = RegQueryValueEx(hIconKey, L"", nil, &type, buf, &size);
            if (res == ERROR_SUCCESS && size > 1) {
                WMSG1("  Icon for '%ls' already exists, not altering\n", appID);
            } else {
                CString iconStr;
                iconStr.Format(L"%ls,%d", exeName, iconIdx);

                if (RegSetValueEx(hIconKey, L"", 0, REG_SZ,
                    (const BYTE*)(LPCTSTR) iconStr,
                    wcslen(iconStr) * sizeof(WCHAR)) == ERROR_SUCCESS)
                {
                    WMSG2("  Set icon for '%ls' to '%ls'\n", appID,
                        (LPCWSTR) iconStr);
                } else {
                    WMSG2("  WARNING: unable to set DefaultIcon  for '%ls' to '%ls'\n",
                        appID, (LPCWSTR) iconStr);
                }
            }
        } else {
            WMSG1("WARNING: couldn't set up DefaultIcon for '%ls'\n", appID);
        }
    } else {
        WMSG1("WARNING: couldn't create AppID='%ls'\n", appID);
    }

    RegCloseKey(hIconKey);
    RegCloseKey(hAppKey);
}

/*
 * Set up the current key's default (which is used as the explorer
 * description) and put the "Open" command in "...\shell\open\command".
 */
void
MyRegistry::ConfigureAppIDSubFields(HKEY hAppKey, const WCHAR* descr,
    const WCHAR* exeName) const
{
    HKEY hShellKey, hOpenKey, hCommandKey;
    DWORD dw;

    ASSERT(hAppKey != nil);
    ASSERT(descr != nil);
    ASSERT(exeName != nil);
    hShellKey = hOpenKey = hCommandKey = nil;
    
    if (RegSetValueEx(hAppKey, L"", 0, REG_SZ, (const BYTE*) descr,
        wcslen(descr) * sizeof(WCHAR)) != ERROR_SUCCESS)
    {
        WMSG1("  WARNING: unable to set description to '%ls'\n", descr);
    }

    if (RegCreateKeyEx(hAppKey, L"shell", 0, REG_NONE,
        REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
        &hShellKey, &dw) == ERROR_SUCCESS)
    {
        if (RegCreateKeyEx(hShellKey, L"open", 0, REG_NONE,
            REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
            &hOpenKey, &dw) == ERROR_SUCCESS)
        {
            if (RegCreateKeyEx(hOpenKey, L"command", 0, REG_NONE,
                REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
                &hCommandKey, &dw) == ERROR_SUCCESS)
            {
                DWORD type, size;
                WCHAR buf[MAX_PATH+8];
                long res;

                size = sizeof(buf);     // size in bytes
                res = RegQueryValueEx(hCommandKey, L"", nil, &type, (LPBYTE) buf,
                    &size);
                if (res == ERROR_SUCCESS && size > 1) {
                    WMSG1("  Command already exists, not altering ('%ls')\n", buf);
                } else {
                    CString openCmd;

                    openCmd.Format(L"\"%ls\" \"%%1\"", exeName);
                    if (RegSetValueEx(hCommandKey, L"", 0, REG_SZ,
                        (LPBYTE)(LPCWSTR) openCmd,
                        wcslen(openCmd) * sizeof(WCHAR)) == ERROR_SUCCESS)
                    {
                        WMSG1("  Set command to '%ls'\n", openCmd);
                    } else {
                        WMSG1("  WARNING: unable to set open cmd '%ls'\n", openCmd);
                    }
                }
            }
        }
    }

    RegCloseKey(hCommandKey);
    RegCloseKey(hOpenKey);
    RegCloseKey(hShellKey);
}


/*
 * Return the number of file type associations.
 */
int
MyRegistry::GetNumFileAssocs(void) const
{
    return NELEM(kFileTypeAssoc);
}

#if 0
/*
 * Return information on a file association.
 *
 * Check to see if we're the application that will be launched.
 *
 * Problem: the file must *actually exist* for this to work.
 */
void
MyRegistry::GetFileAssoc(int idx, CString* pExt, CString* pHandler,
    bool* pOurs) const
{
    char buf[MAX_PATH];

    *pExt = kFileTypeAssoc[idx].ext;
    *pHandler = "";
    *pOurs = false;

    HINSTANCE res = FindExecutable(*pExt, "\\", buf);
    if ((long) res > 32) {
        WMSG1("Executable is '%s'\n", buf);
        *pHandler = buf;
    } else {
        WMSG1("FindExecutable failed (err=%d)\n", res);
        *pHandler = kNoAssociation;
    }
}
#endif


/*
 * Return information on a file association.
 *
 * We check to see if the file extension is associated with one of our
 * application ID strings.  We don't bother to check whether the appID
 * strings are still associated with CiderPress, since nobody should be
 * messing with those.
 *
 * BUG: we should be checking to see what the shell actually does to
 * take into account the overrides that users can set.
 */
void
MyRegistry::GetFileAssoc(int idx, CString* pExt, CString* pHandler,
    bool* pOurs) const
{
    ASSERT(idx >= 0 && idx < NELEM(kFileTypeAssoc));
    long res;

    *pExt = kFileTypeAssoc[idx].ext;
    *pHandler = "";

    CString appID;
    HKEY hExtKey = nil;

    res = RegOpenKeyEx(HKEY_CLASSES_ROOT, *pExt, 0, KEY_READ, &hExtKey);
    if (res == ERROR_SUCCESS) {
        WCHAR buf[260];
        DWORD type;
        DWORD size = sizeof(buf);   // size in bytes

        res = RegQueryValueEx(hExtKey, L"", nil, &type, (LPBYTE)buf, &size);
        if (res == ERROR_SUCCESS) {
            WMSG1("  Got '%ls'\n", buf);
            appID = buf;

            if (GetAssocAppName(appID, pHandler) != 0)
                *pHandler = appID;
        } else {
            WMSG1("RegQueryValueEx failed on '%ls'\n", (LPCWSTR) *pExt);
        }
    } else {
        WMSG1("  RegOpenKeyEx failed on '%ls'\n", *pExt);
    }

    *pOurs = false;
    if (pHandler->IsEmpty()) {
        *pHandler = kNoAssociation;
    } else {
        *pOurs = IsOurAppID(appID);
    }

    RegCloseKey(hExtKey);
}

/*
 * Given an application ID, determine the application's name.
 *
 * This requires burrowing down into HKEY_CLASSES_ROOT\<appID>\shell\open\.
 */
int
MyRegistry::GetAssocAppName(const CString& appID, CString* pCmd) const
{
    CString keyName;
    WCHAR buf[260];
    HKEY hAppKey = nil;
    long res;
    int result = -1;

    keyName = appID + L"\\shell\\open\\command";

    res = RegOpenKeyEx(HKEY_CLASSES_ROOT, keyName, 0, KEY_READ, &hAppKey);
    if (res == ERROR_SUCCESS) {
        DWORD type;
        DWORD size = sizeof(buf);       // size in bytes

        res = RegQueryValueEx(hAppKey, L"", nil, &type, (LPBYTE) buf, &size);
        if (res == ERROR_SUCCESS) {
            CString cmd(buf);
            int pos;

            /* cut it down to just the EXE name */
            ReduceToToken(&cmd);

            pos = cmd.ReverseFind('\\');
            if (pos != -1 && pos != cmd.GetLength()-1) {
                cmd = cmd.Right(cmd.GetLength() - pos -1);
            }

            *pCmd = cmd;
            result = 0;
        } else {
            WMSG1("Unable to open shell\\open\\command for '%ls'\n", appID);
        }
    } else {
        CString errBuf;
        GetWin32ErrorString(res, &errBuf);

        WMSG2("Unable to open AppID key '%ls' (%ls)\n",
            keyName, (LPCWSTR) errBuf);
    }

    RegCloseKey(hAppKey);
    return result;
}

/*
 * Reduce a compound string to just its first token.
 */
void
MyRegistry::ReduceToToken(CString* pStr) const
{
    WCHAR* argv[1];
    int argc = 1;
    WCHAR* mangle = wcsdup(*pStr);

    VectorizeString(mangle, argv, &argc);

    if (argc == 1)
        *pStr = argv[0];

    free(mangle);
}

/*
 * Set the state of a file association.  There are four possibilities:
 *
 *  - We own it, we want to keep owning it: do nothing.
 *  - We don't own it, we want to keep not owning it: do nothing.
 *  - We own it, we don't want it anymore: remove ".xxx" entry.
 *  - We don't own it, we want to own it: remove ".xxx" entry and replace it.
 *
 * Returns 0 on success, nonzero on failure.
 */
int
MyRegistry::SetFileAssoc(int idx, bool wantIt) const
{
    const WCHAR* ext;
    bool weOwnIt;
    int result = 0;

    ASSERT(idx >= 0 && idx < NELEM(kFileTypeAssoc));

    ext = kFileTypeAssoc[idx].ext;
    weOwnIt = GetAssocState(ext);
    WMSG3("SetFileAssoc: ext='%ls' own=%d want=%d\n", ext, weOwnIt, wantIt);

    if (weOwnIt && !wantIt) {
        /* reset it */
        WMSG1(" SetFileAssoc: clearing '%ls'\n", ext);
        result = DisownExtension(ext);
    } else if (!weOwnIt && wantIt) {
        /* take it */
        WMSG1(" SetFileAssoc: taking '%ls'\n", ext);
        result = OwnExtension(ext, kFileTypeAssoc[idx].appID);
    } else {
        WMSG1(" SetFileAssoc: do nothing with '%ls'\n", ext);
        /* do nothing */
    }

    return 0;
}

/*
 * Determine whether or not the filetype described by "ext" is one that we
 * currently manage.
 *
 * Returns "true" if so, "false" if not.  Returns "false" on any errors
 * encountered.
 */
bool
MyRegistry::GetAssocState(const WCHAR* ext) const
{
    WCHAR buf[260];
    HKEY hExtKey = nil;
    int res;
    bool result = false;

    res = RegOpenKeyEx(HKEY_CLASSES_ROOT, ext, 0, KEY_READ, &hExtKey);
    if (res == ERROR_SUCCESS) {
        DWORD type;
        DWORD size = sizeof(buf);       // size in bytes
        res = RegQueryValueEx(hExtKey, L"", nil, &type, (LPBYTE) buf, &size);
        if (res == ERROR_SUCCESS && type == REG_SZ) {
            /* compare it to known appID values */
            WMSG2("  Found '%ls', testing '%ls'\n", ext, buf);
            if (IsOurAppID((WCHAR*)buf))
                result = true;
        }
    }

    RegCloseKey(hExtKey);
    return result;
}

/*
 * Drop ownership of a file extension.
 *
 * We assume we own it.
 *
 * Returns 0 on success, -1 on error.
 */
int
MyRegistry::DisownExtension(const WCHAR* ext) const
{
    ASSERT(ext != nil);
    ASSERT(ext[0] == '.');
    if (ext == nil || wcslen(ext) < 2)
        return -1;

    if (RegDeleteKeyNT(HKEY_CLASSES_ROOT, ext) == ERROR_SUCCESS) {
        WMSG1("    HKCR\\%ls subtree deleted\n", ext);
    } else {
        WMSG1("    Failed deleting HKCR\\'%ls'\n", ext);
        return -1;
    }

    return 0;
}

/*
 * Take ownership of a file extension.
 *
 * Returns 0 on success, -1 on error.
 */
int
MyRegistry::OwnExtension(const WCHAR* ext, const WCHAR* appID) const
{
    ASSERT(ext != nil);
    ASSERT(ext[0] == '.');
    if (ext == nil || wcslen(ext) < 2)
        return -1;

    HKEY hExtKey = nil;
    DWORD dw;
    int res, result = -1;

    /* delete the old key (which might be a hierarchy) */
    res = RegDeleteKeyNT(HKEY_CLASSES_ROOT, ext);
    if (res == ERROR_SUCCESS) {
        WMSG1("    HKCR\\%ls subtree deleted\n", ext);
    } else if (res == ERROR_FILE_NOT_FOUND) {
        WMSG1("    No HKCR\\%ls subtree to delete\n", ext);
    } else {
        WMSG1("    Failed deleting HKCR\\'%ls'\n", ext);
        goto bail;
    }

    /* set the new key */
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT, ext, 0, REG_NONE,
        REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
        &hExtKey, &dw) == ERROR_SUCCESS)
    {
        res = RegSetValueEx(hExtKey, L"", 0, REG_SZ,
                (LPBYTE) appID, wcslen(appID) * sizeof(WCHAR));
        if (res == ERROR_SUCCESS) {
            WMSG2("    Set '%ls' to '%ls'\n", ext, appID);
            result = 0;
        } else {
            WMSG3("Failed setting '%ls' to '%ls' (res=%d)\n", ext, appID, res);
            goto bail;
        }
    }

bail:
    RegCloseKey(hExtKey);
    return result;
}


// (This comes from the MSDN sample sources.)
//
// The sample code makes no attempt to check or recover from partial
// deletions.
//
// A registry key that is opened by an application can be deleted
// without error by another application in both Windows 95 and
// Windows NT. This is by design.
//
#define MAX_KEY_LENGTH  256     // not in any header I can find ++ATM
DWORD
MyRegistry::RegDeleteKeyNT(HKEY hStartKey, LPCTSTR pKeyName) const
{
    DWORD   dwRtn, dwSubKeyLength;
    LPTSTR  pSubKey = NULL;
    TCHAR   szSubKey[MAX_KEY_LENGTH]; // (256) this should be dynamic.
    HKEY    hKey;
    
    // Do not allow NULL or empty key name
    if ( pKeyName &&  lstrlen(pKeyName))
    {
        if( (dwRtn=RegOpenKeyEx(hStartKey,pKeyName,
            0, KEY_ENUMERATE_SUB_KEYS | DELETE, &hKey )) == ERROR_SUCCESS)
        {
            while (dwRtn == ERROR_SUCCESS )
            {
                dwSubKeyLength = MAX_KEY_LENGTH;
                dwRtn=RegEnumKeyEx(
                    hKey,
                    0,       // always index zero, because we're deleting it
                    szSubKey,
                    &dwSubKeyLength,
                    NULL,
                    NULL,
                    NULL,
                    NULL
                    );
                
                if(dwRtn == ERROR_NO_MORE_ITEMS)
                {
                    dwRtn = RegDeleteKey(hStartKey, pKeyName);
                    break;
                }
                else if(dwRtn == ERROR_SUCCESS)
                    dwRtn=RegDeleteKeyNT(hKey, szSubKey);
            }
            RegCloseKey(hKey);
            // Do not save return code because error
            // has already occurred
        }
    }
    else
        dwRtn = ERROR_BADKEY;
    
    return dwRtn;
}
