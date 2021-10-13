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
#include <XRCommon.h>

#include <windows.h>
#include "TuxProc.h"
#include <string.h>
#include <wchar.h>
#include <winbase.h>

#include "tux.h"
#define CEPERF_ENABLE
#include <ceperf.h>

#include <perftestutils.h>
#include <XRCustomEvent.h>
#include <XRPropertyBag.h>
#include <XRCollection.h>
#include <XRDelegate.h>
#include <PerfScenario.h>
#include "utils.h"
#include "Initialize.h"

#define XRPERFLISTRESULTSFILE   L"XRListPerfResults.xml"
#define SMALL_STRING_LEN        50

//Test case ranges
#define TC_GESTURE_END          19
#define TC_SCROLLING_END        29

TESTPROCAPI RunScrollingPerfTest( const WCHAR* lpXAMLName, const WCHAR* lpTestDescription, int TestNumber );

BOOL CALLBACK HostCallBackHook(VOID* UserParam, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* ReturnValue);
DWORD WINAPI RunGestureThreadProc (LPVOID lpParameter);

void CleanupGlobals();


