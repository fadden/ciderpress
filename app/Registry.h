/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * A class representing the system registry.
 */
#ifndef __REGISTRY__
#define __REGISTRY__


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
		kRegNotSet,		// unregistered
		kRegExpired,	// unregistered, expired
		kRegValid,		// registration present and valid
		kRegInvalid,	// registration present, but invalid (!)
		kRegFailed,		// error occurred during registration
	} RegStatus;

	void OneTimeInstall(void) const;
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

	// Get the registry key to be used for our application.
	const char* GetAppRegistryKey(void) const;

	// Fix basic settings, e.g. HKCR AppID classes.
	void FixBasicSettings(void) const;

	int GetNumFileAssocs(void) const;
	void GetFileAssoc(int idx, CString* pExt, CString* pHandler,
		bool* pOurs) const;
	int SetFileAssoc(int idx, bool wantIt) const;

	static unsigned short ComputeStringCRC(const char* str);

private:
	typedef struct FileTypeAssoc {
		const char*	ext;		// e.g. ".SHK"
		const char*	appID;		// e.g. "CiderPress.NuFX"
	} FileTypeAssoc;

	static const FileTypeAssoc kFileTypeAssoc[];

	bool IsOurAppID(const char* id) const;
	void ConfigureAppID(const char* appID, const char* descr,
		const char* exeName, int iconIdx) const;
	void ConfigureAppIDSubFields(HKEY hAppKey, const char* descr,
		const char* exeName) const;
	int GetAssocAppName(const CString& appID, CString* pCmd) const;
	void ReduceToToken(CString* pStr) const;
	bool GetAssocState(const char* ext) const;
	int DisownExtension(const char* ext) const;
	int OwnExtension(const char* ext, const char* appID) const;
	DWORD RegDeleteKeyNT(HKEY hStartKey, LPCTSTR pKeyName) const;

	/* key validation */
	static unsigned short CalcCRC16(unsigned short seed,
		const unsigned char* ptr, int count);
	static char* StripStrings(const char* str1, const char* str2);
	void ComputeKey(const char* chBuf, int salt, long* pKeyLo, long* pKeyHi);
	int VerifyKey(const char* user, const char* company, const char* key);
};

#endif /*__REGISTRY__*/