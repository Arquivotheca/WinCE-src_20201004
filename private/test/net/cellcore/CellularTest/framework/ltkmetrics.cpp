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

BOOL LTKMetrics::LoadTestSuites(FUNCTION_TABLE_ENTRY * &pFTE)
{
    if (m_pFTE)
    {
        pFTE = m_pFTE;
        return TRUE;
    }

    CaseInfo rgBasicCaseGSM [] =
    {
        // voice
        {100, 0},
        {101, 100}, // outgoing normal
        {105, 100}, // incoming normal
        {106, 20},  // incoming call / no answer
        {107, 20},  // incoming call / reject
        {109, 20},  // hold & unhold
        {110, 20},  // conference call
        {112, 20},  // 2-way call

        // data
        {200, 0},
        {201, 0},   // data download
        {202, 0},   // data upload

        // SMS
        {300, 0},   
        {301, 50},  // SMS send receive
        {302, 10},  // long SMS message

        // SIM
        {400, 0},
        {401, 0},   // read SIM phonebook 

        // complex
        {700, 0},
        {701, 0},   // voice & data interaction
        {702, 0},   // voice & SMS interaction
        
        // radio
        {800, 0},
        {801, 5},   // flight mode

        // summary
        {900, 0},
        {901, 0},   // test summary
    };

    CaseInfo rgBasicCaseCDMA [] =
    {
        // voice
        {100, 0},
        {101, 100}, // outgoing normal
        {105, 100}, // incoming normal
        {106, 20},  // incoming call / no answer
        {113, 20},  // CDMA flash
        
        // data
        {200, 0},
        {201, 0},   // data download
        {202, 0},   // data upload

        // SMS
        {300, 0},   
        {301, 50},  // SMS send receive
        {302, 10},  // long SMS message

        // SIM
        {400, 0},
        {401, 0},   // read SIM phonebook 

        // complex
        {700, 0},
        {701, 0},   // voice & data interaction
        {702, 0},   // voice & SMS interaction
        
        // radio
        {800, 0},
        {801, 5},   // flight mode

        // summary
        {900, 0},
        {901, 0},   // test summary
    };

    CaseInfo *pCase = rgBasicCaseGSM;
    DWORD dwBasicCaseNumber = sizeof(rgBasicCaseGSM) / sizeof(rgBasicCaseGSM[0]);
    if (g_TestData.fCDMA)
    {
        pCase = rgBasicCaseCDMA;
        dwBasicCaseNumber = sizeof(rgBasicCaseCDMA) / sizeof(rgBasicCaseCDMA[0]);
    }
    
    // + 2 means that add begin and end flag of FTE.
    m_dwCaseNumber = dwBasicCaseNumber + 2;


    CaseInfo rgCase2 [] = 
    { 
        {902, 0},   // start user2
    };
    
    if (TESTMODE_USER2 == g_dwTestMode)
    {
        pCase = rgCase2;
        m_dwCaseNumber = sizeof(rgCase2) / sizeof(rgCase2[0]) + 2;
    }
    
    m_pFTE = new FUNCTION_TABLE_ENTRY[m_dwCaseNumber];
    if (!m_pFTE)
    {
        g_pLog->Log(
            LOG, 
            L"ERROR: Memory is not enough. Creating LTK test suite is failed."
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

LTKMetrics::LTKMetrics()
{
    wcscpy_s(tszMetric_Name, L"LTK metrics");
}

LTKMetrics::~LTKMetrics()
{
    if (m_pFTE)
    {
        g_pLog->Log(LOG, L"INFO: ~LTKMetrics 0x%x.", m_pFTE);
        delete m_pFTE;
        m_pFTE = NULL;
    }
}

void LTKMetrics::PrintHeader (void)
{
    CMetrics::PrintHeader();
    PrintBorder();
}

void LTKMetrics::PrintSummary (void)
{
    CMetrics::PrintSummary();
    PrintBorder();
}

void LTKMetrics::PrintCheckPoint(void)
{
    CMetrics::PrintCheckPoint();
    PrintBorder();
}
