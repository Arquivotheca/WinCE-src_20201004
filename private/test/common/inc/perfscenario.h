//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#ifndef _PERFSCENARIO_H_
#define _PERFSCENARIO_H_

#pragma once

#include <windowsqa.h>

#ifndef CEPERF_ENABLE
#define CEPERF_ENABLE
#endif
 
#ifndef TTRACKER_DELAYLOAD
#define TTRACKER_DELAYLOAD
#endif

typedef enum _PerfScenarioSessionOptions
{
    PERFSCENARIO_SESSION_DEFAULT = 0,    // This value indicates that no special options are set
    PERFSCENARIO_SESSION_USE_ETW = 1,    // Direct data collection to ETW
} PerfScenarioSessionOptions;

HRESULT WINAPI PerfScenarioOpenSession(LPCTSTR lpszSessionName, BOOL fStartRecordingNow = TRUE, PerfScenarioSessionOptions fOptions = PERFSCENARIO_SESSION_DEFAULT);
HRESULT WINAPI PerfScenarioCloseSession(LPCTSTR lpszSessionName);

HRESULT WINAPI PerfScenarioRegisterCpuInfo(LPCTSTR lpszFileName, BOOL fGlobalEnable = TRUE);
HRESULT WINAPI PerfScenarioDeregisterCpuInfo(BOOL fGlobalEnable = FALSE);
HRESULT WINAPI PerfScenarioEnableCpuInfoFeatureArea(LPCTSTR lpszFeatureName);
HRESULT WINAPI PerfScenarioDisableCpuInfoFeatureArea(LPCTSTR lpszFeatureName);

HRESULT WINAPI PerfScenarioSessionRecordingControl(LPCTSTR lpszSessionName, BOOL fEnable, BOOL fOpenSessionIfNotOpen = TRUE, PerfScenarioSessionOptions fOptions = PERFSCENARIO_SESSION_DEFAULT);
HRESULT WINAPI PerfScenarioSessionRecordingStart(LPCTSTR lpszSessionName, BOOL fOpenSessionIfNotOpen = TRUE, PerfScenarioSessionOptions fOptions = PERFSCENARIO_SESSION_DEFAULT);
HRESULT WINAPI PerfScenarioSessionRecordingStop(LPCTSTR lpszSessionName);

HRESULT WINAPI PerfScenarioStartTest();
HRESULT WINAPI PerfScenarioStopTest();

HRESULT WINAPI PerfScenarioAddAuxData(LPCTSTR lpszLabel, LPCTSTR lpszValue);

HRESULT WINAPI PerfScenarioFlushMetrics(BOOL fCloseAllSessions, GUID* scenarioGuid, LPCTSTR lpszScenarioNamespace, LPCTSTR lpszScenarioName, LPCTSTR lpszLogFileName, LPCTSTR lpszTTrackerFileName, GUID* instanceGuid);

BOOL    WINAPI PerfScenarioAnySessionsOpen();
void    WINAPI PerfScenarioCloseAllSessions();


#endif

