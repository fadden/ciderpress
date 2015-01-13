/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__1CB7B33E_42BF_4A98_B814_4198EA8ACC57__INCLUDED_)
#define AFX_STDAFX_H__1CB7B33E_42BF_4A98_B814_4198EA8ACC57__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers
#define VC_EXTRALEAN

// enable file association editing
#define CAN_UPDATE_FILE_ASSOC

#include "targetver.h"

#include <afxwin.h>
#include <afxwinappex.h>
#include <afxcmn.h>
#include <afxdlgs.h>
#include <afxdisp.h>
#include <afxext.h>
#include <shlobj.h>
#include <mmsystem.h>

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "../diskimg/DiskImg.h"
#include "../util/UtilLib.h"
#include "Main.h"

// TODO: reference additional headers your program requires here

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__1CB7B33E_42BF_4A98_B814_4198EA8ACC57__INCLUDED_)
