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

BOOL StressMetrics::LoadTestSuites(FUNCTION_TABLE_ENTRY * &pFTE)
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
        {101, 100}, // outgoing call / normal
        {102, 1},   // outgoing call / busy
        {103, 1},   // outgoing call / no answer
        {104, 1},   // outgoing call / reject
        {105, 100}, // incoming call / normal
        {106, 20},  // incoming call / no answer
        {107, 20},  // incoming call / reject
        {108, 1},   // long call
        {109, 20},  // hold & unhold
        {110, 20},  // conference
        {111, 1},   // hold conference
        {112, 20},  // 2-way call
        
        // data
        {200, 0},
        {201, 0},   // data download
        {202, 0},   // data upload
        {203, 0},   // Multiple APN / sequential
        {204, 0},   // Multiple APN / simultaneous

        // SMS
        {300, 0},   
        {301, 50},  // SMS send receive
        {302, 10},  // long SMS message

        // SIM
        {400, 0},
        {402, 0},   // read, write, delete SIM phonebook
        {403, 0},   // read, write, delete SIM SMS storage

        // complex
        {700, 0},
        {701, 0},   // voice & data interaction
        {702, 0},   // voice & SMS interaction

        // radio
        {800, 0},
        {801, 5},
            
        // summary
        {900, 0},
        {901, 0},   // test summary
    };

    CaseInfo rgBasicCaseCDMA [] =
    {
        // voice
        {100, 0},
        {101, 100}, // outgoing call / normal
        {102, 1},   // outgoing call / busy
        {105, 100}, // incoming call / normal
        {106, 20},  // incoming call / no answer
        {108, 1},   // long call
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
        {402, 0},   // read, write, delete SIM phonebook
        {403, 0},   // read, write, delete SIM SMS storage
        
        // complex
        {700, 0},
        {701, 0},   // voice & data interaction
        {702, 0},   // voice & SMS interaction

        // radio
        {800, 0},
        {801, 5},
            
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

    DWORD dwStressCaseNubmer = dwBasicCaseNumber * g_TestData.dwStressIteration;
    CaseInfo *pStress = new CaseInfo[dwStressCaseNubmer];
    if (!pStress)
    {
        g_pLog->Log(
            LOG, 
            L"ERROR: OOM while creating stress case info."
            );
        return FALSE;        
    }
    
    for (DWORD i = 0; i < g_TestData.dwStressIteration; ++i)
    {
        for (DWORD j = 0; j < dwBasicCaseNumber; ++j)
        {
            pCase[j].dwCaseId += STEP_OF_ITERATIONID;
        }        
        memcpy(pStress + dwBasicCaseNumber * i, pCase, sizeof(CaseInfo) * dwBasicCaseNumber);
    }
    
    // + 2 means that add begin and end flag of FTE.
    m_dwCaseNumber = dwStressCaseNubmer + 2;
    
    m_pFTE = new FUNCTION_TABLE_ENTRY[m_dwCaseNumber];
    if (!m_pFTE)
    {
        g_pLog->Log(
            LOG, 
            L"ERROR: Memory is not enough. Creating stress test suite is failed."
            );
        delete []pStress;
        return FALSE;
    }

    // load test suites
    BOOL fRet = TRUE;
    fRet = LoadCaseList(pStress);

    if (fRet)
    {
        pFTE = m_pFTE;
    }

    delete []pStress;
    return fRet;
}

StressMetrics::StressMetrics()
{
    wcscpy_s(tszMetric_Name, L"Stress metrics");
}

StressMetrics::~StressMetrics()
{
    if (m_pFTE)
    {
        g_pLog->Log(LOG, L"INFO: ~StressMetrics 0x%x.", m_pFTE);
        delete m_pFTE;
        m_pFTE = NULL;
    }
}

void StressMetrics::PrintHeader(void)
{
    CMetrics::PrintHeader();
    PrintBorder();
}

void StressMetrics::PrintSummary(void)
{
    CMetrics::PrintSummary();
    PrintBorder();
}

void StressMetrics::PrintCheckPoint(void)
{
    CMetrics::PrintCheckPoint();
    PrintBorder();
}
