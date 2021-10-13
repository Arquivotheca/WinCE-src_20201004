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
////////////////////////////////////////////////////////////////////////////////
//
//  GraphTest helper class
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _TUX_GRAPH_TEST_H
#define _TUX_GRAPH_TEST_H

#include <tux.h>
#include "TestDesc.h"
#include "globals.h"

// Place holder test
TESTPROCAPI PlaceholderTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

enum {
    // Timeout when we expect an error to have occurred
    GRAPH_ERROR_TIMEOUT = 10000,

    // Default number of repetitions in build test
    GRAPH_BUILD_REPETITIONS = 10
};

// GraphTest Functions
// Parsing command line
// Construction of the function table from any config file specified
class GraphTest
{
public:
    GraphTest();
    ~GraphTest();
    HRESULT ParseCommandLine(const TCHAR* szCmdLine);
    HRESULT GetTuxFunctionTable(FUNCTION_TABLE_ENTRY** ppFte);

    static TestConfig *m_pTestConfig;
    static bool m_bSaveGRF;

private:
    void AddToFunctionTable(FUNCTION_TABLE_ENTRY* fte, TCHAR* szDesc, unsigned int depth, DWORD dwUserData, DWORD dwUniqueId, TESTPROC lpTestProc);
    HRESULT AddTestsToFunctionTable(FUNCTION_TABLE_ENTRY *fte, TestDescList* pTestDescList, int base, unsigned int depth);
    HRESULT AddGroupToFunctionTable(FUNCTION_TABLE_ENTRY *fte, TestGroup* pTestGroup, int base, unsigned int depth);
    int GetLocalTestIndex(TCHAR* szTestName);
    HRESULT AssignTestId(TestDescList* pTestDescList, TestGroupList* pTestGroupList);

private:
    FUNCTION_TABLE_ENTRY* m_pFunctionTable;
    int m_nEntries;
};


extern TESTPROCAPI HandleTuxMessages(UINT uMsg, TPPARAM tpParam);

#define WAIT_FOR_VERIFICATION        4000
#define WAIT_NEXT_ITERATION            4000
#define WAIT_BEFORE_NEXT_OPERATION    4000
#define SETPOS_WAIT_TIME                5000
#define MAX_FREE_RUNNING_TIME            10000
#define MAX_WAIT_FOR_FRAME                30000
#define NUM_ALTERNATING_STATE_CHANGES    20
#define WAIT_BETWEEN_STATES                3000
#define DURATION_THRESHOLD_IN_MS        100

#endif
