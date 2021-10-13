//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

// utilities.h: Utilities for the MediaRenderer

#pragma once

// MultiOnStateChanged calls pSubscrber->OnStateChanged() for each variable and corresponding new value.
//   0 must be the last element of the array vpszchangedStateVars.
DWORD OnStateChanged(av::IEventSink* pSubscriber, LPCWSTR pszVariable, long lValue);
DWORD OnStateChanged(av::IEventSink* pSubscriber, LPCWSTR pszVariable, LPCWSTR pszValue);
DWORD MultiOnStateChanged(av::IEventSink* pSubscriber, const LPCWSTR vpszchangedStateVars[], LPCWSTR vpsznewValues[]);
DWORD MultiOnStateChanged(av::IEventSink* pSubscriber, const LPCWSTR vpszchangedStateVars[], long    vnnewValues[]);

void FormatMediaTime(LONGLONG lTime, ce::wstring* pstr);
HRESULT ParseMediaTime(LPCWSTR pszTime, LONGLONG* plTime);

#define AX_MIN_VOLUME           -10000
#define AX_MAX_VOLUME           0
#define AX_THREEQUARTERS_VOLUME -240
#define MIN_VOLUME_RANGE        0
#define MAX_VOLUME_RANGE        100

LONG VolumeLinToLog(short LinKnobValue);
short VolumeLogToLin(LONG LogValue);


