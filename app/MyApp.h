/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * The application object.
 */
#ifndef __MYAPP__
#define __MYAPP__

#include "Registry.h"

#if defined(_DEBUG_LOG)
//#define kDebugLog	"C:\\test\\cplog.txt"
#define kDebugLog	"C:\\cplog.txt"
#endif

/* CiderPress version numbers */
#define kAppMajorVersion	3
#define kAppMinorVersion	0
#define kAppBugVersion		1
#define kAppDevString		""

/*
 * Windows application object.
 */
class MyApp: public CWinApp
{
public:
	MyApp(LPCTSTR lpszAppName = NULL);
	virtual ~MyApp(void);

	MyRegistry	fRegistry;

	const char* GetExeFileName(void) const { return fExeFileName; }
	const char* GetExeBaseName(void) const { return fExeBaseName; }

private:
	// Overridden functions
	virtual BOOL InitInstance(void);
	virtual BOOL OnIdle(LONG lCount);

	void LogModuleLocation(const char* name);

	CString		fExeFileName;
	CString		fExeBaseName;
};

extern MyApp gMyApp;

#endif /*__MYAPP__*/