//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------

#pragma once

#define MODULE_NAME     _T("BACKLIGHTAPITEST")

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <ceddk.h>
#include <backlight.h>

#include "regmani.h"
#include <clparse.h>

#define countof(a) (sizeof(a)/sizeof(*(a)))

extern BOOL g_bEnablePMtest;
VOID Debug(LPCTSTR szFormat, ...);

// IOCTLs
// -------
// IOCTL_BKL_GET_BRIGHTNESS
// IOCTL_BKL_GET_BRIGHTNESS_CAPABILITIES
// IOCTL_BKL_GET_SETTINGS
// IOCTL_BKL_SET_SETTINGS
// IOCTL_BKL_FORCE_UPDATE
// IOCTL_POWER_CAPABILITIES
// IOCTL_POWER_QUERY
// IOCTL_POWER_SET
// IOCTL_POWER_GET

// Test function prototypes (TestProc's)
TESTPROCAPI GetBklBrightness      ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI GetBklBrightnessCap      ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI GetBklSettings          ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI SetBklSettings          ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI ForceBklUpdate          ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI PwrMgrBklOps          ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

// Helper function prototypes

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every
// test case within the entire team to be uniquely identified.

#define BASE 1000