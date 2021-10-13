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

extern DWORD g_dwVoicePassRate;

BOOL BTSMetrics::LoadTestSuites(FUNCTION_TABLE_ENTRY * &pFTE)
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
        {101, 10},    // outgoing normal / normal
        {105, 10},    // incoming normal / normal

        // data
        {200, 0},
        {201, 0},   // data download
        {202, 0},   // data upload
        
        // SMS
        {300, 0},   
        {301, 10},  // SMS send receive
        {302, 2},   // long SMS message

        // radio
        {800, 0},
        {801, 1},   // flight mode
        
        // summary
        {900, 0},
        {901, 0},   // test summary
    };

    // TODO: CDMA data, SIM, ...
    CaseInfo rgBasicCaseCDMA [] =
    {
        // voice
        {100, 0},
        {101, 10},    // outgoing normal / normal
        {105, 10},    // incoming normal / normal

        // data
        {201, 0},   // data download
        {202, 0},   // data upload
        
        // SMS
        {300, 0},   
        {301, 10},  // SMS send receive
        {302, 2},   // long SMS message

        // SIM
        
        // radio
        {800, 0},
        {801, 1},   // flight mode
        
        // summary
        {900, 0},
        {901, 0},   // test summary
    };

    CaseInfo *pCase = rgBasicCaseGSM;
    DWORD dwCaseNumber = sizeof(rgBasicCaseGSM) / sizeof(rgBasicCaseGSM[0]);

    if (g_TestData.fCDMA)
    {
        pCase = rgBasicCaseCDMA;
        dwCaseNumber = sizeof(rgBasicCaseCDMA) / sizeof(rgBasicCaseCDMA[0]);
    }
    
    // + 2 means that add begin and end flag of FTE.
    m_dwCaseNumber = dwCaseNumber + 2;
    
    m_pFTE = new FUNCTION_TABLE_ENTRY[m_dwCaseNumber];
    if (!m_pFTE)
    {
        g_pLog->Log(
            LOG, 
            L"ERROR: Memory is not enough. Creating BTS test suite is failed."
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

BTSMetrics::BTSMetrics()
{
    wcscpy_s(tszMetric_Name, L"BTS metrics");

    // BTS specific settings
    g_dwVoicePassRate = 100; // 100% for voice
}

BTSMetrics::~BTSMetrics()
{
    if (m_pFTE)
    {
        g_pLog->Log(LOG, L"INFO: ~BTSMetrics 0x%x.", m_pFTE);
        delete m_pFTE;
        m_pFTE = NULL;
    }
}

void BTSMetrics::PrintHeader(void)
{
    CMetrics::PrintHeader();
    PrintBorder();
}

void BTSMetrics::PrintSummary(void)
{
    CMetrics::PrintSummary();
    PrintBorder();
}

void BTSMetrics::PrintCheckPoint(void)
{
    CMetrics::PrintCheckPoint();
    PrintBorder();
}
