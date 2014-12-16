/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Windows Registry operations.
 */
#include "stdafx.h"
#ifdef CAN_UPDATE_FILE_ASSOC
#include "Registry.h"
#include "Main.h"
#include "MyApp.h"

#define kCompanyName    L"faddenSoft"

#if 0
#define kRegAppName     L"CiderPress"
#define kRegExeName     L"CiderPress.exe"

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

static const WCHAR kRegKeyCPKStr[] = L"CPK";
#endif

/*
 * ProgID fields.
 *
 * See http://msdn.microsoft.com/en-us/library/windows/desktop/cc144152%28v=vs.85%29.aspx
 */

static const WCHAR kDefaultIcon[] = L"DefaultIcon";
static const WCHAR kFriendlyTypeName[] = L"FriendlyTypeName";
static const WCHAR kInfoTip[] = L"InfoTip";

static const WCHAR kShellOpenCommand[] = L"\\shell\\open\\command";


/*
 * ProgID key names.
 *
 * We used to open HKEY_CLASSES_ROOT, which provides a "merged" view of
 * HKEY_LOCAL_MACHINE\Software\Classes and HKEY_CURRENT_USER\Software\Classes.
 * The HKLM entries provided defaults for all users on the machine, while the
 * HKCU entries were specific to the current user.
 *
 * It appears that Windows no longer likes it when executables other than the
 * app installer (which can run privileged) mess with HKLM, so we just work
 * with HKCU directly now.
 */
static const WCHAR kProgIdKeyNuFX[] =       L"CiderPress.NuFX.4";
static const WCHAR kProgIdKeyDiskImage[] =  L"CiderPress.DiskImage.4";
static const WCHAR kProgIdKeyBinaryII[] =   L"CiderPress.BinaryII.4";

/* file associations go here under HKCU */
static const WCHAR kFileAssocBase[] = L"Software\\Classes";

/*
 * Table of file type associations.  They will appear in the UI in the same
 * order that they appear here, so try to maintain alphabetical order.
 */
const MyRegistry::FileTypeAssoc MyRegistry::kFileTypeAssoc[] = {
    { L".2MG",  kProgIdKeyDiskImage },
    { L".APP",  kProgIdKeyDiskImage },
    { L".BNY",  kProgIdKeyBinaryII },
    { L".BQY",  kProgIdKeyBinaryII },
    { L".BSE",  kProgIdKeyNuFX },
    { L".BXY",  kProgIdKeyNuFX },
    { L".D13",  kProgIdKeyDiskImage },
    { L".DDD",  kProgIdKeyDiskImage },
    { L".DO",   kProgIdKeyDiskImage },
    { L".DSK",  kProgIdKeyDiskImage },
    { L".FDI",  kProgIdKeyDiskImage },
    { L".HDV",  kProgIdKeyDiskImage },
    { L".IMG",  kProgIdKeyDiskImage },
    { L".NIB",  kProgIdKeyDiskImage },
    { L".PO",   kProgIdKeyDiskImage },
    { L".SDK",  kProgIdKeyDiskImage },
    { L".SEA",  kProgIdKeyNuFX },
    { L".SHK",  kProgIdKeyNuFX },
//  { L".DC",   kProgIdKeyDiskImage },
//  { L".DC6",  kProgIdKeyDiskImage },
//  { L".GZ",   kProgIdKeyDiskImage },
//  { L".NB2",  kProgIdKeyDiskImage },
//  { L".RAW",  kProgIdKeyDiskImage },
//  { L".ZIP",  kProgIdKeyDiskImage },
};

#if 0
static const struct {
    const char* user;
    const char* reg;
} gBadKeys[] = {
    { "Nimrod Bonehead", "CP1-68C069-62CC9444" },
    { "Connie Tan", "CP1-877B2C-A428FFD6" },
};
#endif


/*
 * ==========================================================================
 *      One-time install/uninstall
 * ==========================================================================
 */

void MyRegistry::OneTimeInstall(void) const
{
    /* start by stomping on our ProgIDs */
    LOGI(" Removing ProgIDs");
    RegDeleteKeyHKCU(kProgIdKeyNuFX);
    RegDeleteKeyHKCU(kProgIdKeyDiskImage);
    RegDeleteKeyHKCU(kProgIdKeyBinaryII);

    /* configure the ProgIDs */
    FixBasicSettings();

    HKEY hClassesKey = NULL;
    if (OpenHKCUSoftwareClasses(&hClassesKey) != ERROR_SUCCESS) {
        return;
    }

    /* configure extensions */
    for (int i = 0; i < NELEM(kFileTypeAssoc); i++) {
        HKEY hExtKey;
        LSTATUS res = RegOpenKeyEx(hClassesKey, kFileTypeAssoc[i].ext, 0,
                KEY_READ, &hExtKey);
        if (res == ERROR_SUCCESS) {
            LOGI(" Found existing HKCU\\%ls\\'%ls', leaving alone",
                kFileAssocBase, kFileTypeAssoc[i].ext);
            RegCloseKey(hExtKey);
        } else if (res == ERROR_FILE_NOT_FOUND) {
            OwnExtension(kFileTypeAssoc[i].ext, kFileTypeAssoc[i].progId);
        } else {
            LOGW(" Got error %lu opening HKCU\\%ls\\'%ls', leaving alone",
                res, kFileAssocBase, kFileTypeAssoc[i].ext);
        }
    }

    RegCloseKey(hClassesKey);
}

void MyRegistry::OneTimeUninstall(void) const
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

    /* remove our ProgIDs */
    LOGI(" Removing ProgIDs");
    RegDeleteKeyHKCU(kProgIdKeyNuFX);
    RegDeleteKeyHKCU(kProgIdKeyDiskImage);
    RegDeleteKeyHKCU(kProgIdKeyBinaryII);
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

const WCHAR* MyRegistry::GetAppRegistryKey(void) const
{
    return kCompanyName;
}

bool MyRegistry::IsOurProgId(const WCHAR* progIdKeyName) const
{
    return (wcsicmp(progIdKeyName, kProgIdKeyNuFX) == 0 ||
            wcsicmp(progIdKeyName, kProgIdKeyDiskImage) == 0 ||
            wcsicmp(progIdKeyName, kProgIdKeyBinaryII) == 0);
}

void MyRegistry::FixBasicSettings(void) const
{
    /*
     * Fix the basic registry settings, e.g. our ProgID classes.
     *
     * We don't overwrite values that already exist.  We want to hold on to the
     * installer's settings, which should get whacked if the program is
     * uninstalled or reinstalled.  This is here for "installer-less" environments
     * and to cope with registry damage.
     */

    const WCHAR* exeName = gMyApp.GetExeFileName();
    ASSERT(exeName != NULL && wcslen(exeName) > 0);

    LOGI("Fixing any missing file type ProgID entries in registry");

    ConfigureProgId(kProgIdKeyNuFX, L"NuFX Archive (CiderPress)", exeName, 1);
    ConfigureProgId(kProgIdKeyBinaryII, L"Binary II (CiderPress)", exeName, 2);
    ConfigureProgId(kProgIdKeyDiskImage, L"Disk Image (CiderPress)", exeName, 3);
}

void MyRegistry::ConfigureProgId(const WCHAR* progIdKeyName, const WCHAR* descr,
    const WCHAR* exeName, int iconIdx) const
{
    LOGI(" ConfigureProgId '%ls' for '%ls'", progIdKeyName, exeName);

    HKEY hClassesKey = NULL;
    HKEY hAppKey = NULL;

    if (OpenHKCUSoftwareClasses(&hClassesKey) != ERROR_SUCCESS) {
        LOGW(" ConfigureProgId failed to open HKCU");
        return;
    }

    DWORD disp;
    LONG result;
    if ((result = RegCreateKeyEx(hClassesKey, progIdKeyName, 0, REG_NONE,
        REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
        &hAppKey, &disp)) == ERROR_SUCCESS)
    {
        if (disp == REG_CREATED_NEW_KEY) {
            LOGD(" Created new key for %ls", progIdKeyName);
        } else if (disp == REG_OPENED_EXISTING_KEY) {
            LOGD(" Opened existing key for %ls", progIdKeyName);
        } else {
            LOGD(" Odd RegCreateKeyEx result 0x%lx", disp);
        }

        ConfigureProgIdCommand(hAppKey, descr, exeName);

        // Configure default entry and "friendly" type name.  The friendly
        // name takes precedence (as tested on Win7), but the default entry
        // is set for backward compatibility.
        if (RegSetValueEx(hAppKey, L"", 0, REG_SZ, (const BYTE*) descr,
            (wcslen(descr)+1) * sizeof(WCHAR)) != ERROR_SUCCESS)
        {
            LOGW("  WARNING: unable to set ProgID default to '%ls'", descr);
        }
        if (RegSetValueEx(hAppKey, kFriendlyTypeName, 0, REG_SZ,
            (const BYTE*) descr,
            (wcslen(descr)+1) * sizeof(WCHAR)) != ERROR_SUCCESS)
        {
            LOGW("  WARNING: unable to set ProgID friendly name to '%ls'", descr);
        }
        //if (RegSetValueEx(hAppKey, kInfoTip, 0, REG_SZ,
        //    (const BYTE*) infoTip,
        //    (wcslen(infoTip)+1) * sizeof(WCHAR)) != ERROR_SUCCESS)
        //{
        //    LOGW("  WARNING: unable to set ProgID info tip to '%ls'", infoTip);
        //}

        // Configure the default icon field.  This is a DefaultIcon entry
        // with a (Default) name.
        HKEY hIconKey = NULL;
        if (RegCreateKeyEx(hAppKey, kDefaultIcon, 0, REG_NONE,
            REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
            &hIconKey, NULL) == ERROR_SUCCESS)
        {
            DWORD type, size;
            unsigned char buf[512];
            long res;

            size = sizeof(buf);     // size in bytes
            res = RegQueryValueEx(hIconKey, L"", NULL, &type, buf, &size);
            if (res == ERROR_SUCCESS && size > 1) {
                LOGI("  Icon for '%ls' already exists, not altering",
                    progIdKeyName);
            } else {
                CString iconStr;
                iconStr.Format(L"%ls,%d", exeName, iconIdx);

                if (RegSetValueEx(hIconKey, L"", 0, REG_SZ,
                    (const BYTE*)(LPCWSTR) iconStr,
                    wcslen(iconStr) * sizeof(WCHAR)) == ERROR_SUCCESS)
                {
                    LOGI("  Set icon for '%ls' to '%ls'", progIdKeyName,
                        (LPCWSTR) iconStr);
                } else {
                    LOGW("  WARNING: unable to set DefaultIcon  for '%ls' to '%ls'",
                        progIdKeyName, (LPCWSTR) iconStr);
                }
            }

            RegCloseKey(hIconKey);
        } else {
            LOGW(" WARNING: couldn't set up DefaultIcon for '%ls'", progIdKeyName);
        }
    } else {
        LOGW(" WARNING: couldn't create ProgId='%ls' (err=%ld)",
            progIdKeyName, result);
    }

    RegCloseKey(hAppKey);
    RegCloseKey(hClassesKey);
}

void MyRegistry::ConfigureProgIdCommand(HKEY hAppKey, const WCHAR* descr,
    const WCHAR* exeName) const
{
    HKEY hShellKey, hOpenKey, hCommandKey;
    DWORD dw;

    ASSERT(hAppKey != NULL);
    ASSERT(descr != NULL);
    ASSERT(exeName != NULL);
    hShellKey = hOpenKey = hCommandKey = NULL;
    
    // TODO: I believe we can do this with a single call -- it should create
    //  the intermediate path elements for us.
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
                res = RegQueryValueEx(hCommandKey, L"", NULL, &type, (LPBYTE) buf,
                    &size);
                if (res == ERROR_SUCCESS && size > 1) {
                    LOGI("  Command already exists, not altering ('%ls')", buf);
                } else {
                    CString openCmd;

                    openCmd.Format(L"\"%ls\" \"%%1\"", exeName);
                    if (RegSetValueEx(hCommandKey, L"", 0, REG_SZ,
                        (LPBYTE)(LPCWSTR) openCmd,
                        wcslen(openCmd) * sizeof(WCHAR)) == ERROR_SUCCESS)
                    {
                        LOGI("  Set command to '%ls'", (LPCWSTR) openCmd);
                    } else {
                        LOGW("  WARNING: unable to set open cmd '%ls'",
                            (LPCWSTR) openCmd);
                    }
                }
                RegCloseKey(hCommandKey);
            }
            RegCloseKey(hOpenKey);
        }
        RegCloseKey(hShellKey);
    }
}


int MyRegistry::GetNumFileAssocs(void) const
{
    return NELEM(kFileTypeAssoc);
}

void MyRegistry::GetFileAssoc(int idx, CString* pExt, CString* pHandler,
    bool* pOurs) const
{
    /*
     * We check to see if the file extension is associated with one of our
     * ProgID keys.  We don't bother to check whether the ProgID keys
     * are still associated with CiderPress, since nobody should be
     * messing with those.
     *
     * BUG: we should be checking to see what the shell actually does to
     * take into account the overrides that users can set.
     */

    ASSERT(idx >= 0 && idx < NELEM(kFileTypeAssoc));

    *pExt = kFileTypeAssoc[idx].ext;
    *pHandler = L"";
    *pOurs = false;

    HKEY hClassesKey = NULL;
    if (OpenHKCUSoftwareClasses(&hClassesKey) != ERROR_SUCCESS) {
        LOGW("GetFileAssoc failed to open HKCU");
        return;
    }

    CString progIdKeyName;
    HKEY hExtKey = NULL;

    long res = RegOpenKeyEx(hClassesKey, *pExt, 0, KEY_READ, &hExtKey);
    if (res == ERROR_SUCCESS) {
        WCHAR buf[MAX_PATH];        // somewhat arbitrary
        DWORD size = sizeof(buf);   // size in bytes
        DWORD type;

        res = RegQueryValueEx(hExtKey, L"", NULL, &type, (LPBYTE)buf, &size);
        if (res == ERROR_SUCCESS) {
            LOGD("  GetFileAssoc %d got '%ls'", idx, buf);
            progIdKeyName = buf;

            if (GetAssocAppName(progIdKeyName, pHandler) != 0)
                *pHandler = progIdKeyName;
        } else {
            LOGW("RegQueryValueEx failed on '%ls'", (LPCWSTR) *pExt);
        }
    } else {
        LOGW("  RegOpenKeyEx failed on '%ls'", (LPCWSTR) *pExt);
    }

    if (!pHandler->IsEmpty()) {
        *pOurs = IsOurProgId(progIdKeyName);
    }

    RegCloseKey(hExtKey);
    RegCloseKey(hClassesKey);
}

int MyRegistry::GetAssocAppName(const CString& progIdKeyName, CString* pCmd) const
{
    HKEY hAppKey = NULL;
    int result = -1;

    HKEY hClassesKey = NULL;
    if (OpenHKCUSoftwareClasses(&hClassesKey) != ERROR_SUCCESS) {
        LOGW("GetAssocAppName failed to open HKCU");
        return result;
    }

    CString keyName = progIdKeyName + kShellOpenCommand;

    long res = RegOpenKeyEx(hClassesKey, keyName, 0, KEY_READ, &hAppKey);
    if (res == ERROR_SUCCESS) {
        WCHAR buf[260];
        DWORD type;
        DWORD size = sizeof(buf);       // size in bytes

        res = RegQueryValueEx(hAppKey, L"", NULL, &type, (LPBYTE) buf, &size);
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
            LOGW("Unable to open %ls for '%ls'", (LPCWSTR) kShellOpenCommand,
                (LPCWSTR) progIdKeyName);
        }
    } else {
        CString errBuf;
        GetWin32ErrorString(res, &errBuf);

        LOGW("Unable to open ProgId key '%ls' (%ls)",
            (LPCWSTR) keyName, (LPCWSTR) errBuf);
    }

    RegCloseKey(hAppKey);
    RegCloseKey(hClassesKey);
    return result;
}

void MyRegistry::ReduceToToken(CString* pStr) const
{
    WCHAR* argv[1];
    int argc = 1;
    WCHAR* mangle = wcsdup(*pStr);

    VectorizeString(mangle, argv, &argc);

    if (argc == 1)
        *pStr = argv[0];

    free(mangle);
}

int MyRegistry::SetFileAssoc(int idx, bool wantIt) const
{
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

    const WCHAR* ext;
    bool weOwnIt;
    int result = 0;

    ASSERT(idx >= 0 && idx < NELEM(kFileTypeAssoc));

    ext = kFileTypeAssoc[idx].ext;
    weOwnIt = GetAssocState(ext);
    LOGI("SetFileAssoc: ext='%ls' own=%d want=%d", ext, weOwnIt, wantIt);

    if (weOwnIt && !wantIt) {
        /* reset it */
        LOGI(" SetFileAssoc: clearing '%ls'", ext);
        result = DisownExtension(ext);
    } else if (!weOwnIt && wantIt) {
        /* take it */
        LOGI(" SetFileAssoc: taking '%ls'", ext);
        result = OwnExtension(ext, kFileTypeAssoc[idx].progId);
    } else {
        LOGI(" SetFileAssoc: do nothing with '%ls'", ext);
        /* do nothing */
    }

    return 0;
}

bool MyRegistry::GetAssocState(const WCHAR* ext) const
{
    WCHAR buf[260];
    HKEY hExtKey = NULL;
    int res;
    bool result = false;

    HKEY hClassesKey = NULL;
    if (OpenHKCUSoftwareClasses(&hClassesKey) != ERROR_SUCCESS) {
        LOGW("GetAssocState failed to open HKCU");
        return result;
    }

    res = RegOpenKeyEx(hClassesKey, ext, 0, KEY_READ, &hExtKey);
    if (res == ERROR_SUCCESS) {
        DWORD type;
        DWORD size = sizeof(buf);       // size in bytes
        res = RegQueryValueEx(hExtKey, L"", NULL, &type, (LPBYTE) buf, &size);
        if (res == ERROR_SUCCESS && type == REG_SZ) {
            /* compare it to known ProgID values */
            LOGD("  Found '%ls', testing '%ls'", ext, buf);
            if (IsOurProgId((WCHAR*)buf))
                result = true;
        }
        RegCloseKey(hExtKey);
    }
    RegCloseKey(hClassesKey);

    return result;
}

int MyRegistry::DisownExtension(const WCHAR* ext) const
{
    ASSERT(ext != NULL);
    ASSERT(ext[0] == '.');
    if (ext == NULL || wcslen(ext) < 2)
        return -1;

    if (RegDeleteKeyHKCU(ext) == ERROR_SUCCESS) {
        LOGI("    HKCU\\%ls\\%ls subtree deleted", kFileAssocBase, ext);
    } else {
        LOGW("    Failed deleting HKCU\\%ls\\'%ls'", kFileAssocBase, ext);
        return -1;
    }

    return 0;
}

int MyRegistry::OwnExtension(const WCHAR* ext, const WCHAR* progIdKeyName) const
{
    ASSERT(ext != NULL);
    ASSERT(ext[0] == '.');
    if (ext == NULL || wcslen(ext) < 2)
        return -1;

    HKEY hClassesKey = NULL;
    HKEY hExtKey = NULL;
    int result = -1;

    if (OpenHKCUSoftwareClasses(&hClassesKey) != ERROR_SUCCESS) {
        goto bail;
    }

    // delete the old key (which might be a hierarchy)
    DWORD res = RegDeleteKeyNT(hClassesKey, ext);
    if (res == ERROR_SUCCESS) {
        LOGI("    HKCU\\%ls\\%ls subtree deleted", kFileAssocBase, ext);
    } else if (res == ERROR_FILE_NOT_FOUND) {
        LOGI("    No HKCU\\%ls\\%ls subtree to delete", kFileAssocBase, ext);
    } else {
        LOGW("    Failed deleting HKCU\\%ls\\'%ls'", kFileAssocBase, ext);
        goto bail;
    }

    // set the new key
    DWORD dw;
    if (RegCreateKeyEx(hClassesKey, ext, 0, REG_NONE,
        REG_OPTION_NON_VOLATILE, KEY_WRITE|KEY_READ, NULL,
        &hExtKey, &dw) == ERROR_SUCCESS)
    {
        // default entry gets the ProgID key name
        res = RegSetValueEx(hExtKey, L"", 0, REG_SZ, (LPBYTE) progIdKeyName,
            (wcslen(progIdKeyName)+1) * sizeof(WCHAR));
        if (res == ERROR_SUCCESS) {
            LOGI("    Set '%ls' to '%ls'", ext, progIdKeyName);
            result = 0;
        } else {
            LOGW("Failed setting '%ls' to '%ls' (res=%d)",
                ext, progIdKeyName, res);
            goto bail;
        }
    }

bail:
    RegCloseKey(hExtKey);
    RegCloseKey(hClassesKey);
    return result;
}

DWORD MyRegistry::OpenHKCUSoftwareClasses(HKEY* phKey) const
{
    DWORD result = RegOpenKeyEx(HKEY_CURRENT_USER, kFileAssocBase, 0,
        KEY_READ, phKey);
    if (result != ERROR_SUCCESS) {
        LOGW("Unable to open HKEY_CURRENT_USER \\ '%ls' for reading",
            (LPCWSTR) kFileAssocBase);
    }

    return result;
}

DWORD MyRegistry::RegDeleteKeyHKCU(const WCHAR* partialKeyName) const
{
    HKEY hClassesKey;
    DWORD result;

    result = OpenHKCUSoftwareClasses(&hClassesKey);
    if (result != ERROR_SUCCESS) { return result;  }

    result = RegDeleteKeyNT(hClassesKey, partialKeyName);
    RegCloseKey(hClassesKey);
    if (result != ERROR_SUCCESS) {
        LOGW("RegDeleteKeyNT failed (err=%lu)", result);
    }
    return result;
}

// (This comes from the MSDN sample sources.)
//
// Recursively delete a key and any sub-keys.
//
// The sample code makes no attempt to check or recover from partial
// deletions.
//
// A registry key that is opened by an application can be deleted
// without error by another application in both Windows 95 and
// Windows NT. This is by design.
//
#define MAX_KEY_LENGTH  256     // not in any header I can find ++ATM
DWORD MyRegistry::RegDeleteKeyNT(HKEY hStartKey, LPCTSTR pKeyName) const
{
    DWORD   dwRtn, dwSubKeyLength;
    LPTSTR  pSubKey = NULL;
    TCHAR   szSubKey[MAX_KEY_LENGTH]; // (256) this should be dynamic.
    HKEY    hKey;
    
    LOGD("RegDeleteKeyNT %p '%ls'", hStartKey, (LPCWSTR) pKeyName);

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
#endif
