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

#include "framework.h"
#include "radiotest.h"
#include "radiotest_metric.h"
#include "radiotest_utils.h"
#include "radiotest_version.h"
#include "radiotest_tsp.h"
#include "Devinfo.h"

#define MAX_SEQFILES    99

extern SuiteInfo g_rgSuite [COUNT_OF_SUITE];
extern DWORD g_dwTotalCaseCount;
extern DWORD g_dwCurrentCaseNumber;

CMetrics::CMetrics() : 
    dwNumTestsTotal(0),
    dwNumTestsPassed(0),
    dwNumTestsFailed(0),
    dwNumConsecutiveTestsFailed(0),
    dwNumLongCallsPassed(0),
    dwGPRSThroughput(0),
    dwGPRSTransferPercent(0),
    dwGPRSConnectionsMade(0),
    dwGPRSConnectionsFailed(0),
    dwGPRSTransferMs(0),
    dwGPRSBytesTransferred(0),
    dwGPRSByteExpected(0),
    dwGPRSTimeouts(0),
    dwRadioResets(0),
    fPassDuration(FALSE),
    fTooManyFails(FALSE),
    uliTestStartTime(0),
    uliTestEndTime(0),
    m_hMetricFile(NULL), 
    m_pFTE(NULL), 
    FLAG_OF_CASELIST(0xFFFF)
 {
    return;
 }

CMetrics::~CMetrics()
{
    return;
}

// Prints out the following information at the start of the test:
// Test date & time
// Radiotest version
// Platform name
// CE Build version
// Radio build info
// MSISDN
// IMSI
// IMEI
void CMetrics::PrintHeader(void)
{
    SYSTEMTIME     systime;
    TCHAR   tszManufacturer[DEVICE_INFO_LENGTH + 1];
    TCHAR   tszModel[DEVICE_INFO_LENGTH + 1];
    TCHAR   tszRevision[DEVICE_INFO_LENGTH + 1];

    GetLocalTime( &systime );
    uliTestStartTime = GetLocalTimeUINT64();

   // Get radio info
    TSP_RetrieveRadioInfo( tszManufacturer, tszModel, tszRevision );

    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "\n" ) );
    PrintBorder();
    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "\n" ) );
    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "Starting %s at %.2d/%.2d/%.2d %.2d:%.2d:%.2d" ),
                         tszMetric_Name,
                         systime.wMonth, systime.wDay, systime.wYear,
                         systime.wHour, systime.wMinute, systime.wSecond );

    PrintAppVersion();
    PrintDeviceInfo();
    PrintRadioInfo( tszManufacturer, tszModel, tszRevision );

    if (g_TestData.tszNetwork_Name[0])
        g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "%-20.20s = %s"), TEXT( "Network"), g_TestData.tszNetwork_Name );

    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT("%-20.20s = %s"),
                           TEXT("MSISDN"), g_TestData.tszNumber_OwnNumberIntl );
    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT("%-20.20s = %s"),
                           TEXT("IMSI"), g_TestData.tszIMSI );
    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT("%-20.20s = %s"),
                           TEXT("IMEI"), g_TestData.tszSerialNumber );


    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "\n" ));

    Sleep( 5 );     // wait a bit so the test can verify this info
}

void CMetrics::PrintSummary(void)
{
    uliTestEndTime = GetLocalTimeUINT64();

    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT(" \n\n" ) );
    PrintBorder();
    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "Metric Summary:" ) );
    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "\n" ) );
    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "%-30.30s = %s" ), TEXT( "Metric Name" ), tszMetric_Name );
    if (fTooManyFails)
        g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "%-30.30s = %s" ),
                             TEXT( "Test end reason" ),
                             TEXT( "Too many errors" ) );
    else if (fPassDuration)
        g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "%-30.30s = %s" ),
                             TEXT( "Test end reason" ),
                             TEXT( "Passed duration" ) );
    else
        g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "%-30.30s = %s" ),
                             TEXT( "Test end reason" ),
                             TEXT( "Normal" ) );
}

void CMetrics::PrintCheckPoint(void)
{
    PrintBorder();
    g_pLog->Log( LOG, TEXT( "Check point:" ) );
    g_pLog->Log( LOG, TEXT( "\n" ) );
}

void CMetrics::PrintAppVersion (void)
{

     g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE,
                           _T("%-20.20s = %d.%d.%d (built on %s at %s)"),
                           _T("Radio Test version"),
                           APP_VERSION_OS,
                           APP_VERSION_MAJOR,
                           APP_VERSION_MINOR,
                           _T(__DATE__),
                           _T(__TIME__) );
}


void CMetrics::PrintRadioInfo (TCHAR *tszManufacturer, TCHAR* tszModel, TCHAR* tszRevision)
{
    // Print out the info
    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE,
                            _T("%-20.20s = %s"),
                            _T("Radio Manufacturer"),
                            tszManufacturer );

     g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE,
                            _T("%-20.20s = %s"),
                            _T("Radio model"),
                            tszModel );

     g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE,
                            _T("%-20.20s = %s"),
                            _T("Radio revision"),
                            tszRevision );

}


void CMetrics::PrintDeviceInfo (void)
{
    //initialize dwROMBuildNum to -1 since private builds could be 0
    DWORD   dwMajorVersion = 0, dwMinorVersion = 0, dwOSBuildNum = 0, dwROMBuildNum = (DWORD)-1;
    TCHAR   tszPlatname[DEVICE_INFO_LENGTH + 1] = _T("");
    BOOL    fOK;

    fOK = GetBuildInfo( &dwMajorVersion, &dwMinorVersion, &dwOSBuildNum, &dwROMBuildNum );
    if (fOK)
    {
        if (dwROMBuildNum != -1)
            g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE,
                               _T("%-20.20s = %d.%d.%d (Build %d)"),
                               _T("OS Version"),
                               dwMajorVersion, dwMinorVersion, dwOSBuildNum, dwROMBuildNum );
        else
            g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE,
                               _T("%-20.20s = %d.%d.%d"),
                               _T("OS Version"),
                               dwMajorVersion, dwMinorVersion, dwOSBuildNum );
    }
    else
    {
        g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE,
                               _T("%-20.20s = %s"),
                               _T("OS Version"),
                               _T("<Unknown>") );
    }

    fOK = GetPlatformName( tszPlatname, DEVICE_INFO_LENGTH );
    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE,
                           _T("%-20.20s = %s (expected %s)"),
                           _T("Platform"),
                           (fOK && tszPlatname[0]) ? tszPlatname : _T("<Unknwon>"),
                           (g_TestData.tszDevice_Name[0]) ? g_TestData.tszDevice_Name : _T("<Unspecified>") );

}

BOOL CMetrics::LoadCaseList(const CaseInfo *pList)
{
    BOOL fRet = TRUE;
    DWORD dwIndex = 0;

    // first item should be set to NULL
    m_pFTE[dwIndex].lpDescription    = TOOLNAME;
    m_pFTE[dwIndex].uDepth           = 0;
    m_pFTE[dwIndex].dwUserData       = 0;
    m_pFTE[dwIndex].dwUniqueID       = 0;
    m_pFTE[dwIndex].lpTestProc       = NULL;

    g_dwTotalCaseCount = 0;
    g_dwCurrentCaseNumber = 0;

    while (++dwIndex < m_dwCaseNumber - 1)
    {
        DWORD dwCaseId = pList[dwIndex - 1].dwCaseId % STEP_OF_ITERATIONID;
        DWORD dwSuiteIndex = dwCaseId / STEP_OF_CASEID;
        if (dwSuiteIndex >= COUNT_OF_SUITE)
        {
            g_pLog->Log(
                LOG,
                L"ERROR: Case ID %d exceeds. Base ID of case should be smaller than %d.", 
                dwCaseId, 
                COUNT_OF_SUITE * STEP_OF_CASEID
                );
            fRet = FALSE;
            break;
        }

        DWORD dwCaseCount = g_rgSuite[dwSuiteIndex].count;
        DWORD dwCaseIndex = dwCaseId % STEP_OF_CASEID;
        if (dwCaseIndex >= dwCaseCount)
        {
            g_pLog->Log(
                LOG,
                L"ERROR: Case ID %d exceeds. Offset ID of case should be smaller than %d.", 
                dwCaseId, 
                dwCaseCount
                );
            fRet = FALSE;
            break;
        }

        CaseProcess testCase = g_rgSuite[dwSuiteIndex].pList[dwCaseIndex];
        m_pFTE[dwIndex].lpDescription    = testCase.wszName;
        m_pFTE[dwIndex].uDepth           = 0;
        m_pFTE[dwIndex].dwUserData       = pList[dwIndex - 1].dwUserData;
        m_pFTE[dwIndex].dwUniqueID       = pList[dwIndex - 1].dwCaseId;

        if (g_rgSuite[dwSuiteIndex].baseID == dwCaseId)
        {
            m_pFTE[dwIndex].lpTestProc = NULL;
        }
        else
        {
            m_pFTE[dwIndex].lpTestProc = TestExecutor;
        }

        g_pLog->Log(
            LOG,
            L"DETAIL: %d-%d Load %s (%d)",
            m_dwCaseNumber - 2,
            dwIndex,
            m_pFTE[dwIndex].lpDescription,
            m_pFTE[dwIndex].dwUniqueID
            );
        
        ++g_dwTotalCaseCount;
    }

    // end flag of FTE
    m_pFTE[dwIndex].lpDescription    = NULL;
    m_pFTE[dwIndex].uDepth           = 0;
    m_pFTE[dwIndex].dwUserData       = 0;
    m_pFTE[dwIndex].dwUniqueID       = 0;
    m_pFTE[dwIndex].lpTestProc       = NULL;
    
    LogToViewerTitle(L" - [0/%u]", g_dwTotalCaseCount);

    return fRet;
}

BOOL CMetrics::LoadTestSuites(FUNCTION_TABLE_ENTRY * &pFTE)
{
    return TRUE;
}
