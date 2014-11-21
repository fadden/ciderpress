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


/*
 * All access to the registry (except for GetProfileInt/GetProfileString)
 * should go through this.
 */
class MyRegistry {
public:
    MyRegistry(void) {}
    ~MyRegistry(void) {}

    typedef enum RegStatus {
        kRegUnknown = 0,
        kRegNotSet,     // unregistered
        kRegExpired,    // unregistered, expired
        kRegValid,      // registration present and valid
        kRegInvalid,    // registration present, but invalid (!)
        kRegFailed,     // error occurred during registration
    } RegStatus;

    /*
     * This is called immediately after installation finishes.
     *
     * We want to snatch up any unused file type associations.  We define them
     * as "unused" if the entry does not exist in the registry at all.  A more
     * thorough installer would also verify that the appID actually existed
     * and "steal" any apparent orphans, but we can let the user do that manually.
     */
    void OneTimeInstall(void) const;

    /*
     * Remove things that the standard uninstall script won't.
     *
     * We want to un-set any of our file associations.  We don't really need to
     * clean up the ".xxx" entries, because removing their appID entries is enough
     * to fry their little brains, but it's probably the right thing to do.
     *
     * We definitely want to strip out our appIDs.
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
     * (in m_pszRegistryKey) with the app name (in m_pszProfileName) and prepend
     * "HKEY_CURRENT_USER\Software\".
     */
    const WCHAR* GetAppRegistryKey(void) const;

    // Fix basic settings, e.g. HKCR AppID classes.
    void FixBasicSettings(void) const;

    /*
     * Return the number of file type associations.
     */
    int GetNumFileAssocs(void) const;

    /*
     * Return information on a file association.
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
        const WCHAR* appID;     // e.g. "CiderPress.NuFX"
    } FileTypeAssoc;

    static const FileTypeAssoc kFileTypeAssoc[];

    /*
     * See if an AppID is one we recognize.
     */
    bool IsOurAppID(const WCHAR* id) const;

    /*
     * Set up the registry goodies for one appID.
     */
    void ConfigureAppID(const WCHAR* appID, const WCHAR* descr,
        const WCHAR* exeName, int iconIdx) const;

    /*
     * Set up the current key's default (which is used as the explorer
     * description) and put the "Open" command in "...\shell\open\command".
     */
    void ConfigureAppIDSubFields(HKEY hAppKey, const WCHAR* descr,
        const WCHAR* exeName) const;

    /*
     * Given an application ID, determine the application's name.
     *
     * This requires burrowing down into HKEY_CLASSES_ROOT\<appID>\shell\open\.
     */
    int GetAssocAppName(const CString& appID, CString* pCmd) const;

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
    int OwnExtension(const WCHAR* ext, const WCHAR* appID) const;

    DWORD RegDeleteKeyNT(HKEY hStartKey, LPCTSTR pKeyName) const;

    /* key validation */
    //static uint16_t CalcCRC16(uint16_t seed,
    //    const uint8_t* ptr, int count);

    static char* StripStrings(const char* str1, const char* str2);
    void ComputeKey(const char* chBuf, int salt, long* pKeyLo, long* pKeyHi);
    int VerifyKey(const char* user, const char* company, const char* key);
};

#endif /*APP_REGISTRY_H*/
