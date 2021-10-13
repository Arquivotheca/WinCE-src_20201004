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

#include <windows.h>
#include "framework.h"
#include "radiotest_metric.h"

BOOL AllCasesMetrics::LoadTestSuites(FUNCTION_TABLE_ENTRY * &pFTE)
{
    if (m_pFTE)
    {
        pFTE = m_pFTE;
        return TRUE;
    }

    CaseInfo rgBasicCase [] =
    {
        // voice
        {100, 0},
        {101, 0},    // outgoing call / normal
        {102, 0},    // outgoing call / busy
        {103, 0},    // outgoing call / no answer
        {104, 0},    // outgoing call / reject
        {105, 0},    // incoming call / normal
        {106, 0},    // incoming call / no answer
        {107, 0},    // incoming call / reject
        {108, 0},    // long call
        {109, 0},    // hold & unhold
        {110, 0},    // conference
        {111, 0},    // hold conference
        {112, 0},    // 2-way call
        {113, 0},    // CDMA flash

        // data
        {200, 0},
        {201, 0},   // data download
        {202, 0},   // data upload
        {203, 0},   // Multiple APN / sequential
        {204, 0},   // Multiple APN / simultaneous

        // SMS
        {300, 0},
        {301, 0},   // SMS send & recv
        {302, 0},   // long SMS message

        // SIM
        {400, 0},
        {401, 0},   // read SIM phonebook 
        {402, 0},   // read, write, delete SIM phonebook
        {403, 0},   // read, write, delete SIM SMS storage

        // complex
        {700, 0},
        {701, 0},   // voice & data interaction
        {702, 0},   // voice & SMS interaction

        // radio
        {800, 0},
        {801, 0},   // flight mode

        // summary
        {900, 0},
        {901, 0},   // test summary
    };

    CaseInfo *pCase = rgBasicCase;
    
    // + 2 means that add begin and end flag of FTE.
    m_dwCaseNumber = sizeof(rgBasicCase) / sizeof(rgBasicCase[0]) + 2;
    
    m_pFTE = new FUNCTION_TABLE_ENTRY[m_dwCaseNumber];
    if (!m_pFTE)
    {
        g_pLog->Log(
            LOG, 
            L"ERROR: Memory is not enough. Creating all cases test suite is failed."
            );
        return FALSE;
    }

    // load test suites
    BOOL fRet = TRUE;
    fRet = LoadCaseList(pCase);

    if (fRet)
    {
        pFTE = m_pFTE;
    }
    return fRet;
}

AllCasesMetrics::AllCasesMetrics()
{
    wcscpy_s(tszMetric_Name, L"All Cases Metrics");
}

AllCasesMetrics::~AllCasesMetrics()
{
    if (m_pFTE)
    {
        g_pLog->Log(LOG, L"INFO: ~AllCasesMetrics 0x%x.", m_pFTE);
        delete m_pFTE;
        m_pFTE = NULL;
    }
}

void AllCasesMetrics::PrintHeader (void)
{
    CMetrics::PrintHeader();
    PrintBorder();
}

void AllCasesMetrics::PrintSummary (void)
{
    CMetrics::PrintSummary();
    PrintBorder();
}

void AllCasesMetrics::PrintCheckPoint(void)
{
    CMetrics::PrintCheckPoint();
    PrintBorder();
}
