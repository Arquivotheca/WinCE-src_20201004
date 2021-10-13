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
#ifndef TUX_MAIN_H
#define TUX_MAIN_H

#include <tux.h>
#include "FilterTest.h"
#include "VidCapTestGraph.h"
#include "vidcaptests.h"

#define MY_EXCEPTION          0
#define MY_FAIL               2
#define MY_ABORT              4
#define MY_SKIP               6
#define MY_NOT_IMPLEMENTED    8
#define MY_PASS              10

#define countof(x) (sizeof(x)/sizeof(x[0]))

#define VidCapIDBase   1000


//extern SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam);
extern BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved);
extern void LOG(LPWSTR szFormat, ...);
extern void Debug(LPCTSTR szFormat, ...);

static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    TEXT("Filter Tests"), 0, 0, 0, NULL,

    TEXT("Video Capture Filter Tests"), 1, 0, 0, NULL,
    TEXT("Filter creation test."),            2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDFilterCreate,        FilterCreate,
    TEXT("Pin enumerator test."),            2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDPinEnumerate,        PinEnumerate,
    TEXT("Query filter info test."),        2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDQueryFilterInfo,        QueryFilterInfo,
    //TEXT("Query accept test."),                2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDQueryAccept,            QueryAcceptTest,
    TEXT("Sink ConnectDisconnect test."),    2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDSinkConnectDisconnect, SinkConnectDisconnect,
    TEXT("Valid media combination test."),    2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDValidMediaCombination,    ValidMediaCombination,
    TEXT("Stop test - no streaming."),        2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDStopTest,            StopTest,
    TEXT("Pause test - no streaming."),        2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDPauseTest,            PauseTest,
    TEXT("Run test - no streaming."),        2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDRunTest,                RunTest,
    TEXT("Streaming test - Running, with verification."),        2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDStreamingRun,        StreamingRun,
    TEXT("Flush test."),                    2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDFlushTest,            FlushTest,
    TEXT("New segment test."),                2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDNewSegmentTest,        NewSegmentTest,
    TEXT("QueryInterfaceTests."),                2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDQueryInterface,        VidCapQITest,
    TEXT("Pin QueryInterfaceTests."),                2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDQueryPinInterface,        VidCapPinQITest,
    TEXT("PropertyBag driver load tests."),                2, NULL, VidCapIDBase + IDDriverLoad,        VidCapDriverLoadTests,
    TEXT("GetCLSIDTest."),                2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDGetCLSID,        GetCLSIDTest,
    TEXT("PinFlowingTests."),                2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDPinFlowingTests,        PinFlowingTests,
    TEXT("PinFlowingGraphStateTests."),                2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDPinFlowingGraphSTateTests,        PinFlowingGraphStateTests,
    TEXT("StillPinTriggerTests."),                2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDStillPinTriggerTests,        StillPinTriggerTests,
    TEXT("VidCap metadata tests."),                2, (unsigned long)&CreateVidCapTestGraph, VidCapIDBase + IDVCapMetadataTests,        VCapMetadataTest,

    NULL, 0, 0, 0, NULL
};

#endif
