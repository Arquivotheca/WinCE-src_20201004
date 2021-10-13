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
#ifndef __PERFLOG_H__
#define __PERFLOG_H__

#include <windows.h>
#include <tchar.h>
#include <ceperf.h>
#include "devinfo.h"


#define INVALID_HANDLE(X)   (INVALID_HANDLE_VALUE == X || NULL == X)
#define VALID_HANDLE(X)     (INVALID_HANDLE_VALUE != X && NULL != X)
#define VALID_POINTER(X)    (X != NULL && 0xcccccccc != (int) X)
#define CHECK_FREE(X) if (VALID_POINTER(X)) free(X)
#define CHECK_DELETE(X) if (VALID_POINTER(X)) delete X
#define CHECK_LOCAL_FREE(X) if (VALID_POINTER(X)) LocalFree(X)
#define CHECK_CLOSE_HANDLE(X) if (VALID_HANDLE(X)) CloseHandle(X)

#define DEFAULT_PERF_MARKER_NAME _T("Default")
#define DEFAULT_PERF_TEST_NAME _T("Perf")
#define DEFAULT_PERF_MARKER_PARAMS _T("")

// --------------------------------------------------------------------
// Enumerates data types that can be logged/calculated
// --------------------------------------------------------------------
enum PerfDataType
{
    PERF_DATA_DURATION,
    PERF_DATA_THROUGHPUT,
};

// --------------------------------------------------------------------
// Abstract class that defines the PerfHelper interface
// --------------------------------------------------------------------
class CPerfHelper
{
public:
    CPerfHelper() {};
    virtual ~CPerfHelper() {};
    virtual BOOL StartLog() = 0;
    virtual BOOL EndLog() = 0;
    virtual BOOL StartSystemMonitor(DWORD dwInterval) = 0;
    virtual BOOL StopSystemMonitor() = 0;
};

// --------------------------------------------------------------------
// CPerfScenario class : uses CEPerf/PerfScenario to generate perf data
// --------------------------------------------------------------------
class CPerfScenario : public CPerfHelper
{
public:
    CPerfScenario(LPCTSTR pszPerfMarkerName,
        LPCTSTR pszPerfMarkerParams,
        PerfDataType dataType,
        DWORD dwTestParam,
        DWORD dwTestValue,
        float flScalingFactor);
    ~CPerfScenario();
    BOOL StartLog();
    BOOL EndLog();
    BOOL StartSystemMonitor(DWORD dwInterval);
    BOOL StopSystemMonitor();
    static BOOL Initialize(LPCTSTR pszTestName, BOOL fUseReleaseDirectory, LPCTSTR pszStorageProfileOrPath);
    static BOOL Initialize(LPCTSTR pszTestName, BOOL fUseReleaseDirectory);
    static BOOL DumpLog();
    static BOOL DumpLogEx(GUID *pguid, LPCTSTR pszScenNameSpace, LPCTSTR pszScenName);
    static BOOL DumpLogToFile(LPCTSTR pszFileName);
    static BOOL AddAuxData(LPCTSTR lpszLabel, LPCTSTR lpszValue);
    static const DWORD MAX_NAME_LENGTH = CEPERF_MAX_ITEM_NAME_LEN;  // 32
    static const DWORD MAX_PARAM_LENGTH = CEPERF_MAX_ITEM_NAME_LEN; // 32
    static BOOL CheckInit();

private:
    static BOOL m_fInitPeformed;
    static BOOL m_fInitSucceeded;
    static BOOL m_fUseReleaseDirectory;
    static LPTSTR m_pszLogFileName;
    static LPTSTR m_pszTestAppName;
    static HANDLE m_hPerfSession;
    static LARGE_INTEGER m_liPerformanceFreq;
    BOOL m_fLogStarted;
    LPTSTR m_pszPerfMarkerString;
    DWORD m_dwTestParam;
    DWORD m_dwTestValue;
    PerfDataType m_dataType;
    HANDLE m_hDurationTracker;
    HANDLE m_hStatTracker;
    ULONGLONG m_ullTotalDuration;
    DWORD m_dwPerfIterations;
    float m_flScalingFactor;
};

// --------------------------------------------------------------------
// CPerflog class : uses perflog.dll to generate perf data
// --------------------------------------------------------------------
class CPerflog : public CPerfHelper
{
public:
    CPerflog(LPCTSTR pszPerfMarkerName, LPCTSTR pszPerfMarkerParams);
    ~CPerflog();
    BOOL StartLog();
    BOOL EndLog();
    BOOL StartSystemMonitor(DWORD dwInterval = SYSLOG_INTERVAL);
    BOOL StopSystemMonitor();
    static BOOL Initialize(BOOL fUseReleaseDirectory = TRUE);
    static BOOL Initialize(LPCTSTR pszTestName, BOOL fUseReleaseDirectory);
    static BOOL DumpLogToFile(LPCTSTR pszFileName);
    static const DWORD MAX_NAME_LENGTH = 40;
    static const DWORD MAX_PARAM_LENGTH = 160;
protected:
private:
    BOOL m_fLogStarted;
    BOOL m_fSysMonStarted;
    DWORD m_dwPerfMaker;
    LPTSTR m_pszPerfMarkerName;
    LPTSTR m_pszPerfMarkerParams;
    LPTSTR m_pszPerfMarkerString;
    static BOOL CheckInit();
    static BOOL SetPerfOutputDirectory();
    static BOOL FindLogFile();
    static void PerformCalibration();
    static BOOL m_fInitPeformed;
    static BOOL m_fInitSucceeded;
    static BOOL m_fUseReleaseDirectory;
    static LPTSTR m_pszLogFileName;
    static LPTSTR m_pszTestAppName;
    static DWORD m_dwLastLogFileSize;
    static const DWORD MARK_TEST = 0;
    static const DWORD MAX_MARKER_LENGTH = 200;
    static const DWORD CALIBRATION_COUNT = 10;
    static const DWORD SYSLOG_INTERVAL = 1000;
    static const DWORD DEFAULT_WRITE_SIZE = 32768;
    static DWORD m_dwMarkerCount;
    static LPCRITICAL_SECTION m_lpCriticalSection;
};

#endif // __PERFLOG_H__
