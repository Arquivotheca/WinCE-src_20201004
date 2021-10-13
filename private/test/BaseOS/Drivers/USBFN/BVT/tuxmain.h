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
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//
// --------------------------------------------------------------------

#pragma once

#define MODULE_NAME     _T("USBFnBvt")

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <ceddk.h>
#include <svsutil.hxx>
#include <auto_xxx.hxx>
#include <usbfnioctl.h>
#include <string.h>
#include <devload.h>
#include <clparse.h>

#define SUSPEND_RESUME_DISABLED    0x01
#define SUSPEND_RESUME_ENABLED     0x02
#define USB_FN_BVT_CLIENT      1

//
// global macros
//
#define INVALID_HANDLE(X)   (INVALID_HANDLE_VALUE == X || NULL == X)
#define VALID_HANDLE(X)     (INVALID_HANDLE_VALUE != X && NULL != X)

// kato logging macros
#define FAIL(x)     g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x)  g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )

#define SUCCESS(x)  g_pKato->Log( LOG_PASS, \
                        TEXT("SUCCESS: %s"), TEXT(x) )

#define WARN(x)     g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

#define DETAIL(x)   g_pKato->Log( LOG_DETAIL, TEXT(x) )

// same as ERRFAIL, but doesn't log it as a failure
#define ERR(x)  g_pKato->Log( LOG_DETAIL, \
                        TEXT("ERROR in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )



void LOG(LPWSTR szFmt, ...);
void FAILLOG(LPWSTR szFmt, ...);

BOOL SetupUfnTestReg();
BOOL CleanUfnTest();
BOOL AddRegEntryForUfnDevice(LPCTSTR pszUFNName, LPCTSTR pszDllName, DWORD dwVendorID, DWORD dwProdID, LPCTSTR pszPrefixName);
BOOL GetCurrentClient(HANDLE hUfn, PUFN_CLIENT_INFO puci, BOOL fCurrent);
DWORD TestSuspendAndResume();

DWORD ChangeClient(
    HANDLE hUfn,
    LPCTSTR pszNewClient,
    BOOL fCurrent
    );


TESTPROCAPI
TestUnloadReloadUfnBusDrv(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI
TestEnumClient(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI
TestGetSetCurrentClient(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI
TestGetSetDefaultClient(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI
TestIoctlAdditional(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI
TestEnumChangeClient(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI
TestCurrentAndDefaultClientSuspendResume(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE );
// externs
extern CKato            *g_pKato;




