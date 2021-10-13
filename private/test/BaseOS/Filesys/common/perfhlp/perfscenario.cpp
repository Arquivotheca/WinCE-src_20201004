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

#include "perfhlp.h"
#include <MDPGPerf.h>
#include "PerfScenario.h"

LPVOID pCePerfInternal; // Instantiate required CePerf global
#define PERF_FILESYS TEXT("SYSTEM\\PerfFilesys")
#define STORAGE_PROFILEPATH_LABEL TEXT("StorageProfileOrPath")

static GUID PerfFilesysGUID = { 0xba3459bf, 0x8e, 0x4f3a, { 0xbf, 0x6, 0x75, 0x1d, 0x47, 0x55, 0x47, 0x4e } };

// --------------------------------------------------------------------
// Set static class variables to default values
// --------------------------------------------------------------------
BOOL CPerfScenario::m_fInitPeformed = FALSE;
BOOL CPerfScenario::m_fInitSucceeded = FALSE;
BOOL CPerfScenario::m_fUseReleaseDirectory = TRUE;
LPTSTR CPerfScenario::m_pszLogFileName = NULL;
LPTSTR CPerfScenario::m_pszTestAppName = NULL;
HANDLE CPerfScenario::m_hPerfSession = INVALID_HANDLE_VALUE;
LARGE_INTEGER CPerfScenario::m_liPerformanceFreq = {0};

// --------------------------------------------------------------------
// Constructor
// --------------------------------------------------------------------
CPerfScenario::CPerfScenario(
    LPCTSTR pszPerfMarkerName,
    LPCTSTR pszPerfMarkerParams,
    PerfDataType dataType,
    DWORD dwTestParam,
    DWORD dwTestValue,
    float flScalingFactor)
{
    RETAILMSG(1, (_T("Info : CPerfScenario::CPerfScenario() : %s %s"), pszPerfMarkerName, pszPerfMarkerParams));

    m_pszPerfMarkerString = (TCHAR*) malloc(sizeof(TCHAR) * CEPERF_MAX_ITEM_NAME_LEN);
    if (NULL == m_pszPerfMarkerString)
    {
        RETAILMSG(1, (_T("Error : CPerfScenario::CPerfScenario() : Out of Memory")));
        return;
    }

    if (NULL != pszPerfMarkerParams)
    {
        StringCchPrintf(m_pszPerfMarkerString,
            CEPERF_MAX_ITEM_NAME_LEN,
            _T("_%s,%s"),
            pszPerfMarkerName,
            pszPerfMarkerParams);
    }
    else
    {
        StringCchPrintf(m_pszPerfMarkerString,
            CEPERF_MAX_ITEM_NAME_LEN,
            _T("_%s"),
            pszPerfMarkerName);
    }

    m_dwTestParam = dwTestParam;
    m_dwTestValue = dwTestValue;
    m_flScalingFactor = flScalingFactor;
    m_dataType = dataType;
    m_ullTotalDuration = 0;
    m_dwPerfIterations = 0;
    m_fLogStarted = FALSE;

    // Register a duration tracker for this perf object
    m_hDurationTracker = INVALID_HANDLE_VALUE;
    if FAILED(CePerfRegisterTrackedItem(m_hPerfSession,
        &m_hDurationTracker,
        CEPERF_TYPE_DURATION,
        m_pszPerfMarkerString,
        CEPERF_RECORD_ABSOLUTE_PERFCOUNT | CEPERF_DURATION_RECORD_MIN,
        NULL))
    {
        RETAILMSG(1, (_T("Error : CPerfScenario::CPerfScenario() : Failed to register duration object")));
    }

    // Register a statistics tracker for this perf object
    m_hStatTracker = INVALID_HANDLE_VALUE;
    if FAILED(CePerfRegisterTrackedItem(m_hPerfSession,
        &m_hStatTracker,
        CEPERF_TYPE_STATISTIC,
        pszPerfMarkerName,
        CEPERF_STATISTIC_RECORD_SHORT,
        NULL))
    {
        RETAILMSG(1, (_T("Error : CPerfScenario::CPerfScenario() : Failed to register statistic object")));
    }
}

// --------------------------------------------------------------------
// Verifies that CEPerf/PerfScenario have been initialized
// --------------------------------------------------------------------
BOOL CPerfScenario::CheckInit()
{
    BOOL fRet = m_fInitPeformed && m_fInitSucceeded;
    if (!fRet)
    {
        RETAILMSG(1, (_T("Error : CPerfScenario::CheckInit() : CEPerf not initialized")));
    }
    return fRet;
}

// --------------------------------------------------------------------
// Starts the timer
// --------------------------------------------------------------------
BOOL CPerfScenario::StartLog()
{
    if (!CheckInit())
    {
        return FALSE;
    }

    // Check if the log is already running
    if (m_fLogStarted)
    {
        return FALSE;
    }

    m_dwPerfIterations++;
    CePerfBeginDuration(m_hDurationTracker);

    m_fLogStarted = TRUE;

    return TRUE;
}

// --------------------------------------------------------------------
// Stops the timer
// --------------------------------------------------------------------
BOOL CPerfScenario::EndLog()
{
    CEPERF_ITEM_DATA itemData = {0};
    itemData.wSize = sizeof(CEPERF_ITEM_DATA);
    itemData.wVersion = 1;

    if (!CheckInit())
    {
        return FALSE;
    }

   // Check if the log has been started
    if (!m_fLogStarted)
    {
        return FALSE;
    }

    CePerfEndDuration(m_hDurationTracker, &itemData);
    m_ullTotalDuration += itemData.Duration.liPerfCount.QuadPart;

    m_fLogStarted = FALSE;

    return TRUE;
}

// --------------------------------------------------------------------
// Generates statistics and cleans up
// --------------------------------------------------------------------
CPerfScenario::~CPerfScenario()
{
    if (PERF_DATA_THROUGHPUT == m_dataType)
    {
        if(m_ullTotalDuration>0) //to avoid divide by zero scenario(happens when the log is not started all)
        {
            ULONGLONG ullThroughput = (((ULONGLONG)(m_dwTestValue * m_liPerformanceFreq.QuadPart)) / m_ullTotalDuration);
            ullThroughput *= (ULONGLONG) m_flScalingFactor;
            CePerfAddStatistic(m_hStatTracker, (DWORD)ullThroughput, NULL);
        }
    }
    else if(PERF_DATA_DURATION == m_dataType)
    {
        if(m_ullTotalDuration>0) //to avoid divide by zero scenario(happens when the log is not started all)
        {
            ULONGLONG ullDuration = (m_liPerformanceFreq.QuadPart/ m_ullTotalDuration);
            ullDuration *= (ULONGLONG)m_flScalingFactor;
            CePerfAddStatistic(m_hStatTracker, (DWORD)ullDuration, NULL);
        }
    }

    if (FAILED(CePerfDeregisterTrackedItem(m_hStatTracker) || 
        FAILED(CePerfDeregisterTrackedItem(m_hDurationTracker))))
    {
        RETAILMSG(1, (_T("Error : CPerfScenario::~CPerfScenario() : Could not deregister ceperf measurement objects")));
    }
    m_hStatTracker     = INVALID_HANDLE_VALUE;
    m_hDurationTracker = INVALID_HANDLE_VALUE;
    

    // Add post-processing for other data types here ...
    CHECK_DELETE(m_pszPerfMarkerString);
}

// --------------------------------------------------------------------
// Not impelemented for CEPerf/PerfScenario
// --------------------------------------------------------------------
BOOL CPerfScenario::StartSystemMonitor(DWORD /*dwInterval*/)
{
    return FALSE;
}

// --------------------------------------------------------------------
// Not impelemented for CEPerf/PerfScenario
// --------------------------------------------------------------------
BOOL CPerfScenario::StopSystemMonitor()
{
    return FALSE;
}

// --------------------------------------------------------------------
// Intitializes the PerfScenario session
// --------------------------------------------------------------------
BOOL CPerfScenario::Initialize(LPCTSTR pszTestName, BOOL fUseReleaseDirectory)
{
    return Initialize(pszTestName, fUseReleaseDirectory, NULL);
}

BOOL CPerfScenario::Initialize(LPCTSTR pszTestName, BOOL fUseReleaseDirectory, LPCTSTR pszStorageProfileOrPath)
{
    TCHAR szLogRoot[MAX_PATH];
    BOOL fRet = FALSE;
    HRESULT hr = S_OK;

    // We only need to initialize once
    if (m_fInitPeformed)
    {
        return m_fInitSucceeded;
    }

    m_fInitPeformed = TRUE;
    m_fUseReleaseDirectory = fUseReleaseDirectory;
    m_pszLogFileName = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);

    // Generate file name for output log
    if (m_fUseReleaseDirectory)
    {
        VERIFY(SUCCEEDED(StringCchPrintf(m_pszLogFileName,
            MAX_PATH,
            _T("\\Release\\%s.xml"),
            pszTestName)));
    }
    else
    {
        if(GetVirtualRelease(szLogRoot, MAX_PATH)==0)
        {
            RETAILMSG(1, (_T("Error : CPerfScenario::Initialize() : GetVirtualRelease failed with error %d"), GetLastError()));
            goto done;
        }
        VERIFY(SUCCEEDED(StringCchPrintf(m_pszLogFileName,
            MAX_PATH,
            _T("%s%s.xml"),
            szLogRoot, pszTestName)));
    }

    // Activate PerfScenario recording
    hr = PerfScenarioOpenSession(PERF_FILESYS, TRUE);
    if (hr != S_OK)
    {
        RETAILMSG(1, (_T("Error : CPerfScenario::Initialize() : PerfScenarioOpenSession() failed with error %#x"), hr));
        goto done;
    }

    // Initialize PerfScenario sesion data
    m_hPerfSession = INVALID_HANDLE_VALUE;
    hr = MDPGPerfOpenSession(&m_hPerfSession, PERF_FILESYS);
    if (hr != S_OK)
    {
        RETAILMSG(1, (_T("Error : CPerfScenario::Initialize() : MDPGPerfOpenSession() failed with error %#x"), hr));
        goto done;
    }

    // Log test-specific information as auxilliary data.
    if (pszStorageProfileOrPath)
    {
        if (S_OK != PerfScenarioAddAuxData(STORAGE_PROFILEPATH_LABEL, pszStorageProfileOrPath))
        {
            RETAILMSG(1, (_T("Error : CPerfScenario::Initialize() : PerfScenarioAddAuxData() failed with error %#x"), hr));
        }
    }
    
    // Get the frequency of the high resolution timer
    QueryPerformanceFrequency(&m_liPerformanceFreq);
    RETAILMSG(1, (_T("Info : CPerfScenario::Initialize() :Using resolution value of %u t/s"),
        m_liPerformanceFreq.QuadPart));

    fRet = TRUE;
done:
    m_fInitSucceeded = fRet;
    return fRet;
}

// --------------------------------------------------------------------
// Dumps CEPerf data into specified output file
// --------------------------------------------------------------------
BOOL CPerfScenario::DumpLogToFile(LPCTSTR szFileName)
{
    BOOL fRet = FALSE;

    RETAILMSG(1, (_T("Info : CPerfScenario:::DumpToLogFile() : Attempting to flush CEPerf data to log file %s"),
        szFileName));

    DeleteFile(szFileName);

    fRet = DumpLog();

    CopyFile(m_pszLogFileName, szFileName, FALSE);
    return fRet;
}
BOOL CPerfScenario::DumpLog()
{
    return DumpLogEx(&PerfFilesysGUID, L"FILESYS NAMESPACE", L"FILESYS");
}
BOOL CPerfScenario::DumpLogEx(GUID *pguid, LPCTSTR pszScenNameSpace, LPCTSTR pszScenName)
{
    BOOL fRet = FALSE;

    if (S_OK != PerfScenarioFlushMetrics(
        FALSE,
        pguid,
        pszScenNameSpace,
        pszScenName,
        m_pszLogFileName,
        NULL,
        NULL))
    {
        RETAILMSG(1, (_T("Error : CPerfScenario::DumpLog() : PerfScenarioFlushMetrics() failed")));
        goto done;
    }

    if (FAILED(CePerfFlushSession(m_hPerfSession, NULL, CEPERF_FLUSH_DATA, 0)))
    {
        RETAILMSG(1, (_T("Error : CPerfScenario::DumpLog() : CePerfFlushSession failed")));
    }

    if ((S_OK != PerfScenarioCloseSession(PERF_FILESYS)) || 
        FAILED(CePerfCloseSession(m_hPerfSession)))
    {
        RETAILMSG(1, (_T("Error : CPerfScenario::DumpLog() : PerfScenarioCloseSession() or CePerfCloseSession failed")));
        goto done;
    }

    m_hPerfSession = INVALID_HANDLE_VALUE;
    m_fInitPeformed = FALSE;
    m_fInitSucceeded = FALSE;

    fRet = TRUE;

done:
    return fRet;
}


// --------------------------------------------------------------------
// Adds PerfScenario Auxiliary data
// --------------------------------------------------------------------
BOOL CPerfScenario::AddAuxData(LPCTSTR lpszLabel, LPCTSTR lpszValue)
{
    BOOL fRet = FALSE;

    HRESULT hr = PerfScenarioAddAuxData(lpszLabel, lpszValue);
    if (S_OK != hr)
    {
        RETAILMSG(1, (_T("Error : CPerfScenario::AddAuxData : PerfScenarioAddAuxData() failed with error %#x"), hr));
        goto done;
    }

    fRet = TRUE;

done:
    return fRet;
}