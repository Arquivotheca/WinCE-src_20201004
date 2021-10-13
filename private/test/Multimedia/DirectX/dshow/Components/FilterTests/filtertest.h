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
#ifndef _FILTER_TEST_H
#define _FILTER_TEST_H

#include <streams.h>
#include <tux.h>

enum TestID
{
    IDFilterCreate,
    IDPinEnumerate,
    IDQueryFilterInfo,
	// BUGBUG - QueryAccept does not work as a test
    IDQueryAccept,
    IDSourceConnectDisconnect,
    IDSinkConnectDisconnect,
    IDGraphConnectDisconnect,
    IDValidMediaCombination,
    IDStopTest,
    IDPauseTest,
    IDRunTest,
    IDStreamingRun,
    IDFlushTest,
    IDNewSegmentTest,
	IDTestIDEndMarker
};

class CTestGraph;

typedef CTestGraph* (*TestGraphFactory)();

TESTPROCAPI FilterCreate(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI PinEnumerate(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI QueryFilterInfo(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI QueryAcceptTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI SourceConnectDisconnect(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI SinkConnectDisconnect(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI GraphConnectDisconnect(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI ValidMediaCombination(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI StopTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI PauseTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI RunTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI StreamingRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
/**
This test exercises the flush mechanism in the graph and hence in the filter under test. 
Flushing is independent of the streaming thread and hence should work irrespective of the streaming.
**/
TESTPROCAPI FlushTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI NewSegmentTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI EndOfStreamTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

#endif