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

#define kRegAppName     "CiderPress"
#define kRegExeName     "CiderPress.exe"
#define kCompanyName    "faddenSoft"

static const char* kRegKeyCPKVersions = _T("vrs");
static const char* kRegKeyCPKExpire = _T("epr");

/*
 * Application path.  Add two keys:
 *
 *  (default) = FullPathName
 *      Full pathname of the executable file.
 *  Path = Path
 *      The $PATH that will be in effect when the program starts (but only if
 *      launched from the Windows explorer).
 */
static const char* kAppKeyBase =
    _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" kRegExeName);

/*
 * Local settings.  App stuff goes in the per-user key, registration info is
 * in the per-machine key.
 */
static const char* kMachineSettingsBaseKey =
    _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\" kCompanyName "\\" kRegAppName);
static const char* kUserSettingsBaseKey =
    _T("HKEY_CURRENT_USER\\Software\\" kCompanyName "\\" kRegAppName);

/*
 * Set this key + ".XXX" to (Default)=AppID.  This associates the file
 * type with kRegAppID.
 */
//static const char* kFileExtensionBase = _T("HKEY_CLASSES_ROOT");

/*
 * Description of data files.  Set this key + AppID to 40-char string, e.g.
 * (Default)=CompanyName AppName Version DataType
 *
 * Can also set DefaultIcon = Pathname [,Index]
 */
//static const char* kAppIDBase = _T("HKEY_CLASSES_ROOT");

/*
 * Put one of these under the AppID to specify the icon for a file type.
 */
static const char* kDefaultIcon = _T("DefaultIcon");

static const char* kRegKeyCPKStr = "CPK";

/*
 * Table of file type associations.  They will appear in the UI in the same
 * order that they appear here, so try to maintain alphabetical order.
 */
#define kAppIDNuFX      _T("CiderPress.NuFX")
#define kAppIDDiskImage _T("CiderPress.DiskImage")
#define kAppIDBinaryII  _T("CiderPress.BinaryII")
#define kNoAssociation  _T("(no association)")
const MyRegistry::FileTypeAssoc MyRegistry::kFileTypeAssoc[] = {
    { _T(".2MG"),   kAppIDDiskImage },
    { _T(".APP"),   kAppIDDiskImage },
    { _T(".BNY"),   kAppIDBinaryII },
    { _T(".BQY"),   kAppIDBinaryII },
    { _T(".BSE"),   kAppIDNuFX },
    { _T(".BXY"),   kAppIDNuFX },
    { _T(".D13"),   kAppIDDiskImage },
    { _T(".DDD"),   kAppIDDiskImage },
    { _T(".DO"),    kAppIDDiskImage },
    { _T(".DSK"),   kAppIDDiskImage },
    { _T(".FDI"),   kAppIDDiskImage },
    { _T(".HDV"),   kAppIDDiskImage },
    { _T(".IMG"),   kAppIDDiskImage },
    { _T(".NIB"),   kAppIDDiskImage },
    { _T(".PO"),    kAppIDDiskImage },
    { _T(".SDK"),   kAppIDDiskImage },
    { _T(".SEA"),   kAppIDNuFX },
    { _T(".SHK"),   kAppIDNuFX },
//  { _T(".DC"),    kAppIDDiskImage },
//  { _T(".DC6"),   kAppIDDiskImage },
//  { _T(".GZ"),    kAppIDDiskImage },
//  { _T(".NB2"),   kAppIDDiskImage },
//  { _T(".RAW"),   kAppIDDiskImage },
//  { _T(".ZIP"),   kAppIDDiskImage },
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
            WMSG1(" Found existing HKCR\\'%s', leaving alone\n",
                kFileTypeAssoc[i].ext);
            RegCloseKey(hExtKey);
        } else if (res == ERROR_FILE_NOT_FOUND) {
            OwnExtension(kFileTypeAssoc[i].ext, kFileTypeAssoc[i].appID);
        } else {
            WMSG2(" Got error %ld opening HKCR\\'%s', leaving alone\n",
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
const char*
MyRegistry::GetAppRegistryKey(void) const
{
    return kCompanyName;
}

/*
 * See if an AppID is one we recognize.
 */
bool
MyRegistry::IsOurAppID(const char* id) const
{
    return (strcasecmp(id, kAppIDNuFX) == 0 ||
            strcasecmp(id, kAppIDDiskImage) == 0 ||
            strcasecmp(id, kAppIDBinaryII) == 0);
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
    const char* exeName = gMyApp.GetExeFileName();
    ASSERT(exeName != nil && strlen(exeName) > 0);

    WMSG0("Fixing any missing file type AppID entries in registry\n");

    ConfigureAppID(kAppIDNuFX, "NuFX Archive (CiderPress)", exeName, 1);
    ConfigureAppID(kAppIDBinaryII, "Binary II (CiderPress)", exeName, 2);
    ConfigureAppID(kAppIDDiskImage, "Disk Image (CiderPress)", exeName, 3);
}

/*
 * Set up the registry goodies for one appID.
 */
void
MyRegistry::ConfigureAppID(const char* appID, const char* descr,
    const char* exeName, int iconIdx) const
{
    WMSG2(" Configuring '%s' for '%s'\n", appID, exeName);

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
            unsigned char buf[256];
            long res;

            size = sizeof(buf);
            res = RegQueryValueEx(hIconKey, "", nil, &type, buf, &size);
            if (res == ERROR_SUCCESS && size > 1) {
                WMSG1("  Icon for '%s' already exists, not altering\n", appID);
            } else {
                CString iconStr;
                iconStr.Format("%s,%d", exeName, iconIdx);

                if (RegSetValueEx(hIconKey, "", 0, REG_SZ, (const unsigned char*)
                    (const char*) iconStr, strlen(iconStr)) == ERROR_SUCCESS)
                {
                    WMSG2("  Set icon for '%s' to '%s'\n", appID, (LPCTSTR) iconStr);
                } else {
                    WMSG2("  WARNING: unable to set DefaultIcon  for '%s' to '%s'\n",
                        appID, (LPCTSTR) iconStr);
                }
            }
        } else {
            WMSG1("WARNING: couldn't set up DefaultIcon for '%s'\n", appID);
        }
    } else {
        WMSG1("WARNING: couldn't create AppID='%s'\n", appID);
    }

    RegCloseKey(hIconKey);
    RegCloseKey(hAppKey);
}

/*
 * Set up the current key's default (which is used as the explorer
 * description) and put the "Open" command in "...\shell\open\command".
 */
void
MyRegistry::ConfigureAppIDSubFields(HKEY hAppKey, const char* descr,
    const char* exeName) const
{
    HKEY hShellKey, hOpenKey, hCommandKey;
    DWORD dw;

    ASSERT(hAppKey != nil);
    ASSERT(descr != nil);
    ASSERT(exeName != nil);
    hShellKey = hOpenKey = hCommandKey = nil;
    
    if (RegSetValueEx(hAppKey, "", 0, REG_SZ, (const unsigned char*) descr,
        strlen(descr)) != ERROR_SUCCESS)
    {
        WMSG1("  WARNING: unable to set description to '%s'\n", descr);
    }

    if (RegCreateKeyEx(hAppKey, _T("shell"), 0, REG_NONE,
        REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
        &hShellKey, &dw) == ERROR_SUCCESS)
    {
        if (RegCreateKeyEx(hShellKey, _T("open"), 0, REG_NONE,
            REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
            &hOpenKey, &dw) == ERROR_SUCCESS)
        {
            if (RegCreateKeyEx(hOpenKey, _T("command"), 0, REG_NONE,
                REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
                &hCommandKey, &dw) == ERROR_SUCCESS)
            {
                DWORD type, size;
                unsigned char buf[MAX_PATH+8];
                long res;

                size = sizeof(buf);
                res = RegQueryValueEx(hCommandKey, "", nil, &type, buf, &size);
                if (res == ERROR_SUCCESS && size > 1) {
                    WMSG1("  Command already exists, not altering ('%s')\n", buf);
                } else {
                    CString openCmd;

                    openCmd.Format("\"%s\" \"%%1\"", exeName);
                    if (RegSetValueEx(hCommandKey, "", 0, REG_SZ,
                        (const unsigned char*) (const char*) openCmd,
                        strlen(openCmd)) == ERROR_SUCCESS)
                    {
                        WMSG1("  Set command to '%s'\n", openCmd);
                    } else {
                        WMSG1("  WARNING: unable to set open cmd '%s'\n", openCmd);
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
        unsigned char buf[260];
        DWORD type, size;

        size = sizeof(buf);
        res = RegQueryValueEx(hExtKey, "", nil, &type, buf, &size);
        if (res == ERROR_SUCCESS) {
            WMSG1("  Got '%s'\n", buf);
            appID = buf;

            if (GetAssocAppName(appID, pHandler) != 0)
                *pHandler = appID;
        } else {
            WMSG1("RegQueryValueEx failed on '%s'\n", (LPCTSTR) *pExt);
        }
    } else {
        WMSG1("  RegOpenKeyEx failed on '%s'\n", *pExt);
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
    unsigned char buf[260];
    DWORD type, size = sizeof(buf);
    HKEY hAppKey = nil;
    long res;
    int result = -1;

    keyName = appID + "\\shell\\open\\command";

    res = RegOpenKeyEx(HKEY_CLASSES_ROOT, keyName, 0, KEY_READ, &hAppKey);
    if (res == ERROR_SUCCESS) {
        res = RegQueryValueEx(hAppKey, "", nil, &type, buf, &size);
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
            WMSG1("Unable to open shell\\open\\command for '%s'\n", appID);
        }
    } else {
        CString errBuf;
        GetWin32ErrorString(res, &errBuf);

        WMSG2("Unable to open AppID key '%s' (%s)\n",
            keyName, (LPCTSTR) errBuf);
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
    char* argv[1];
    int argc = 1;
    char* mangle = strdup(*pStr);

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
    const char* ext;
    bool weOwnIt;
    int result = 0;

    ASSERT(idx >= 0 && idx < NELEM(kFileTypeAssoc));

    ext = kFileTypeAssoc[idx].ext;
    weOwnIt = GetAssocState(ext);
    WMSG3("SetFileAssoc: ext='%s' own=%d want=%d\n", ext, weOwnIt, wantIt);

    if (weOwnIt && !wantIt) {
        /* reset it */
        WMSG1(" SetFileAssoc: clearing '%s'\n", ext);
        result = DisownExtension(ext);
    } else if (!weOwnIt && wantIt) {
        /* take it */
        WMSG1(" SetFileAssoc: taking '%s'\n", ext);
        result = OwnExtension(ext, kFileTypeAssoc[idx].appID);
    } else {
        WMSG1(" SetFileAssoc: do nothing with '%s'\n", ext);
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
MyRegistry::GetAssocState(const char* ext) const
{
    unsigned char buf[260];
    HKEY hExtKey = nil;
    DWORD type, size;
    int res;
    bool result = false;

    res = RegOpenKeyEx(HKEY_CLASSES_ROOT, ext, 0, KEY_READ, &hExtKey);
    if (res == ERROR_SUCCESS) {
        size = sizeof(buf);
        res = RegQueryValueEx(hExtKey, "", nil, &type, buf, &size);
        if (res == ERROR_SUCCESS && type == REG_SZ) {
            /* compare it to known appID values */
            WMSG2("  Found '%s', testing '%s'\n", ext, buf);
            if (IsOurAppID((char*)buf))
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
MyRegistry::DisownExtension(const char* ext) const
{
    ASSERT(ext != nil);
    ASSERT(ext[0] == '.');
    if (ext == nil || strlen(ext) < 2)
        return -1;

    if (RegDeleteKeyNT(HKEY_CLASSES_ROOT, ext) == ERROR_SUCCESS) {
        WMSG1("    HKCR\\%s subtree deleted\n", ext);
    } else {
        WMSG1("    Failed deleting HKCR\\'%s'\n", ext);
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
MyRegistry::OwnExtension(const char* ext, const char* appID) const
{
    ASSERT(ext != nil);
    ASSERT(ext[0] == '.');
    if (ext == nil || strlen(ext) < 2)
        return -1;

    HKEY hExtKey = nil;
    DWORD dw;
    int res, result = -1;

    /* delete the old key (which might be a hierarchy) */
    res = RegDeleteKeyNT(HKEY_CLASSES_ROOT, ext);
    if (res == ERROR_SUCCESS) {
        WMSG1("    HKCR\\%s subtree deleted\n", ext);
    } else if (res == ERROR_FILE_NOT_FOUND) {
        WMSG1("    No HKCR\\%s subtree to delete\n", ext);
    } else {
        WMSG1("    Failed deleting HKCR\\'%s'\n", ext);
        goto bail;
    }

    /* set the new key */
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT, ext, 0, REG_NONE,
        REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
        &hExtKey, &dw) == ERROR_SUCCESS)
    {
        res = RegSetValueEx(hExtKey, "", 0, REG_SZ,
                (const unsigned char*) appID, strlen(appID));
        if (res == ERROR_SUCCESS) {
            WMSG2("    Set '%s' to '%s'\n", ext, appID);
            result = 0;
        } else {
            WMSG3("Failed setting '%s' to '%s' (res=%d)\n", ext, appID, res);
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
