/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */

// Visual C++ 2013 yells if you don't define these values.  By default
// they're set to the highest version, but we want to support WinXP, so
// we use that definition.
//
// The symbolic names for the constants are defined in SDKDDKVer.h, but
// that also sets the default values, so the auto-generated "targetver.h"
// recommends doing it this way.  Web docs confirm that it should be done
// numerically.
#include <WinSDKVer.h>
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#include <SDKDDKVer.h>
