/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * A class representing the system registry.
 */
#ifndef APP_REGISTRY_H
#define APP_REGISTRY_H

#ifdef CAN_UPDATE_FILE_ASSOC

/*
 * All access to the registry (except for GetProfileInt/GetProfileString)
 * should go through this.
 */
class MyRegistry {
public:
    MyRegistry(void) {}
    ~MyRegistry(void) {}

    /*
    typedef enum RegStatus {
        kRegUnknown = 0,
        kRegNotSet,     // unregistered
        kRegExpired,    // unregistered, expired
        kRegValid,      // registration present and valid
        kRegInvalid,    // registration present, but invalid (!)
        kRegFailed,     // error occurred during registration
    } RegStatus;
    */

    /*
     * This is called immediately after installation finishes.
     *
     * We want to snatch up any unused file type associations.  We define them
     * as "unused" if the entry does not exist in the registry at all.  A more
     * thorough installer would also verify that the ProgID actually existed
     * and "steal" any apparent orphans, but we can let the user do that manually.
     */
    void OneTimeInstall(void) const;

    /*
     * Remove things that the standard uninstall script won't.
     *
     * We want to un-set any of our file associations.  We don't really need to
     * clean up the ".xxx" entries, because removing their ProgID entries is enough
     * to fry their little brains, but it's probably the right thing to do.
     *
     * We definitely want to strip out our ProgIDs.
     */
    void OneTimeUninstall(void) const;

    /*
    int GetRegistration(CString* pUser, CString* pCompany,
        CString* pReg, CString* pVersions, CString* pExpire);
    int SetRegistration(const CString& user, const CString& company,
        const CString& reg, const CString& versions, const CString& expire);
    RegStatus CheckRegistration(CString* pResult);
    bool IsValidRegistrationKey(const CString& user,
        const CString& company, const CString& reg);
    */

    /*
     * Return the application's registry key.  This is used as the argument to
     * CWinApp::SetRegistryKey().  The GetProfile{Int,String} calls combine this
     * (in m_pszRegistryKey) with the app name (in m_pszProfileName) and
     * prepends "HKEY_CURRENT_USER\Software\".
     */
    const WCHAR* GetAppRegistryKey(void) const;

    // Fix basic settings, e.g. HKCU ProgID classes.
    void FixBasicSettings(void) const;

    /*
     * Return the number of file type associations.
     */
    int GetNumFileAssocs(void) const;

    /*
     * Return information on a file association.
     *
     * For the given index, return the extension, the ProgID key, and an
     * indication of whether or not we believe this is ours.  If nothing
     * is associated with this extension, *pHandler receives an empty string.
     */
    void GetFileAssoc(int idx, CString* pExt, CString* pHandler,
        bool* pOurs) const;

    /*
     * Sets the state of a file association.
     */
    int SetFileAssoc(int idx, bool wantIt) const;

    //static uint16_t ComputeStringCRC(const char* str);

private:
    typedef struct FileTypeAssoc {
        const WCHAR* ext;       // e.g. ".SHK"
        const WCHAR* progId;    // e.g. "CiderPress.NuFX.4"
    } FileTypeAssoc;

    static const FileTypeAssoc kFileTypeAssoc[];

    /*
     * See if a ProgID key is one we recognize.
     */
    bool IsOurProgId(const WCHAR* progIdKeyName) const;

    /*
     * Set up the registry goodies for one ProgID.
     */
    void ConfigureProgId(const WCHAR* progIdKeyName, const WCHAR* descr,
        const WCHAR* exeName, int iconIdx) const;

    /*
     * Puts the "Open" command in "...\shell\open\command".
     */
    void ConfigureProgIdCommand(HKEY hAppKey, const WCHAR* descr,
        const WCHAR* exeName) const;

    /*
     * Given a ProgID, determine the application's name.  The executable
     * path will be stripped.
     *
     * This requires burrowing down into
     * HKEY_CURRENT_USER\<ProgIdKey>\shell\open\command\.
     *
     * This does not currently take into account the Windows shell stuff, i.e.
     * HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\<.ext>
     * (and I'm not really sure we should).
     *
     * TODO: if we don't find an association in HKCU, do a lookup in HKCR.
     *   That could get confusing if there's an older CiderPress association
     *   lurking in HKLM, but it's at least as confusing to see "no
     *   association" when there's clearly an association.
     */
    int GetAssocAppName(const CString& progIdKeyName, CString* pCmd) const;

    /*
     * Reduce a compound string to just its first token.
     */
    void ReduceToToken(CString* pStr) const;

    /*
     * Determine whether or not the filetype described by "ext" is one that we
     * currently manage.
     *
     * Returns "true" if so, "false" if not.  Returns "false" on any errors
     * encountered.
     */
    bool GetAssocState(const WCHAR* ext) const;

    /*
     * Drop ownership of a file extension.  We assume we own it.
     *
     * Returns 0 on success, -1 on error.
     */
    int DisownExtension(const WCHAR* ext) const;

    /*
     * Take ownership of a file extension.
     *
     * Returns 0 on success, -1 on error.
     */
    int OwnExtension(const WCHAR* ext, const WCHAR* progIdKeyName) const;

    /*
     * Open HKEY_CURRENT_USER\Software\Classes for reading.
     */
    DWORD OpenHKCUSoftwareClasses(HKEY* phKey) const;

    /*
     * Recursively delete a key in the HKEY_CURRENT_USER\Software\Classes
     * hierarchy.
     */
    DWORD RegDeleteKeyHKCU(const WCHAR* partialKeyName) const;

    DWORD RegDeleteKeyNT(HKEY hStartKey, LPCWSTR pKeyName) const;

    /* key validation */
    //static uint16_t CalcCRC16(uint16_t seed,
    //    const uint8_t* ptr, int count);

    static char* StripStrings(const char* str1, const char* str2);
    void ComputeKey(const char* chBuf, int salt, long* pKeyLo, long* pKeyHi);
    int VerifyKey(const char* user, const char* company, const char* key);
};

#endif

#endif /*APP_REGISTRY_H*/
