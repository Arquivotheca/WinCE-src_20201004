//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef __BTSPerfLog_H__
#define __BTSPerfLog_H__

#include <windows.h>

// #define _DUMP_
// #define _DUMP_STATS_

#if defined(_BTSPERF_EXPORTS_)
#define BTSPERF_API(type) __declspec(dllexport) type __cdecl 
#define BTSPERF_CLASS __declspec(dllexport)
#else
#define BTSPERF_API(type) __declspec(dllimport) type __cdecl 
#define BTSPERF_CLASS __declspec(dllimport)
#endif

#if !defined(HERE)
#if defined(DEBUG)
#define HERE(s) NKDbgPrintfW(_T(__FUNCTION__)  _T(": ")  s)
#else
#define HERE(s) ((PVOID)0)
#endif    // DEBUG
#endif // HERE

typedef enum
        {
            READ_DURATION,
            WRITE_DURATION
        } LOG_FLAGS;

typedef enum
        {
            DUMP_HANDLE_NAMES,
            DUMP_TLS_DATA,
            DUMP_CLASS_MEMBERS,
            DUMP_STATS
        }    DUMP_BTS;

typedef struct {
    LARGE_INTEGER r_beginMarker;
    LARGE_INTEGER read_duration;
    LARGE_INTEGER w_beginMarker;
    LARGE_INTEGER write_duration;
    DWORD bytesRead;
    DWORD bytesWritten;
    HANDLE* lphTrackMetrics;
} tlsData, *lptlsData;

typedef HRESULT (WINAPI *PFN_OpenSession)(LPCTSTR, BOOL);
typedef HRESULT (WINAPI *PFN_CloseSession)(LPCTSTR);
typedef HRESULT (WINAPI *PFN_FlushMetrics)(BOOL, GUID*, LPCTSTR, LPCTSTR, LPCTSTR, LPCTSTR, GUID*);
typedef HRESULT (WINAPI *PFN_AddAuxData)(LPCTSTR, LPCTSTR);

#pragma warning( push )
#pragma warning( disable: 4512 )

class BTSPERF_CLASS BTSPerfLog
{
private:
    const DWORD m_dwMaxThreads;
    DWORD m_dwThreadCount;
    DWORD m_dwHandleCount;
    DWORD m_cOptionalNames;
    DWORD m_tlsIndex;
    bool m_fInit_lib;
    LPWSTR m_lpszSessionPath;
    LPWSTR m_lpszScenarioNamespace;
    LPWSTR m_lpszScenarioName;
    HANDLE m_hPerfSession;
    HANDLE *m_handleCache;
    DWORD  m_handleCache_Marker;
    LPWSTR* m_lpszOptionalNames;
    LPWSTR m_lpszperfLog;
    CRITICAL_SECTION m_csInitLibrary;
    GUID m_guiPerfScenario;
    HMODULE m_hMod;
    HMODULE m_hPerfScenario;
    PFN_OpenSession m_pfnOpenSession;
    PFN_CloseSession m_pfnCloseSession;
    PFN_FlushMetrics m_pfnFlushMetrics;
    PFN_AddAuxData m_pfnAddAuxData;

    bool LoadDependentDLLs();
    DWORD GetImageFolder(HMODULE hModDll, LPWSTR fileName, DWORD dwlen);
    void Initialize(LPCWSTR lpszSessionPath, LPCWSTR lpszScenarioNamespace, LPCWSTR lpszScenarioName,GUID scenarioGuid, HMODULE hModDll);
    void Finalize();

    bool CopyTrackedDataToTLS();
    LPHANDLE InitTrackedData();
    void InitialTLS(lptlsData ptls, HANDLE *lphTrackedMetrics);
    void CacheTrackedHandles();

    bool CreateLogFileName(HMODULE hModDll, LPCWSTR logName);

#if defined(_DUMP_)
    void Print_TLS_Data();
    void Print_ClassMembers();
    void Print_Handles();
#else
    void Print_TLS_Data() {}
    void Print_ClassMembers() {}
    void Print_Handles() {}
#endif    //_DUMP_

#if defined(_DUMP_STATS_)
    void Print_Stats(LOG_FLAGS counter_type, DWORD cBytes, LARGE_INTEGER startMarker, LARGE_INTEGER endMarker);
#else
    void Print_Stats(LOG_FLAGS /* counter_type */, DWORD /* cBytes */, LARGE_INTEGER /* startMarker */, LARGE_INTEGER /* endMarker */) {}
#endif    //_DUMP_STATS_


// forbidden
    BTSPerfLog() : m_dwMaxThreads(0) {}
    explicit BTSPerfLog(const BTSPerfLog &) : m_dwMaxThreads(0) {}

protected:
    lptlsData GetTlsData();
    LPWSTR CopyBTSString(LPCWSTR str, DWORD cookie = 0);


public:

/*
 * "lpszSessionPath" needs to be set to a unique name typically this will be the "lpDescription"
 * field within the FUNCTION_TABLE_ENTRY for the Tux test
 *
 * "lpszfileName" must just the name of the file you want the results written to. This file will
*/
    BTSPerfLog(LPCWSTR lpszSessionPath, LPCWSTR lpszScenarioNamespace, LPCWSTR lpszScenarioName, GUID scenarioGuid, HMODULE hModDll) : m_dwMaxThreads(1)
    {
        HERE(L"single-threaded ctor");
        Initialize(lpszSessionPath, lpszScenarioNamespace, lpszScenarioName, scenarioGuid, hModDll);
    }

    BTSPerfLog(DWORD cThreads, LPCWSTR lpszSessionPath, LPCWSTR lpszScenarioNamespace, LPCWSTR lpszScenarioName, GUID scenarioGuid, HMODULE hModDll) : m_dwMaxThreads(cThreads)
    {
        HERE(L"multi-threaded ctor");
        Initialize(lpszSessionPath, lpszScenarioNamespace, lpszScenarioName, scenarioGuid, hModDll);
    }

    ~BTSPerfLog() { HERE(L"dtor"); Finalize(); }


#if defined(_DUMP_)
    void Dump(DUMP_BTS bts);
#else
    void Dump(DUMP_BTS /* bts */) {}
#endif    //_DUMP_

/*
 * PerfInitialize handles the setup of PerfScenario and CePerf
 *
 * This function is typically called at the begining of each Tux test along with your other intialization
 * code.
 *
 */
    bool virtual PerfInitialize(LPCWSTR *metricsNames, LPCWSTR logName);


/*
 * PerfFinalize cleans up and unloads PerfScenario and CePerf.
 *
 * When this function returns the data will be flushed to disk
 *
 */
    bool virtual PerfFinalize(LPCWSTR scaleFactor = L"1");


/*
 * PerfBegin/PerEnd mark the begining and end of the code segment which you
 * want measured.
 *
 * The "counter_type is an enumeration type which must be either the value of
 * "READ_DURATION" or "WRITE_DURATION"
 *
 * cBytes is the number of bytes read or writen in this timespan
 *
 */
    bool PerfBegin(LOG_FLAGS counter_type);
    bool PerfEnd(LOG_FLAGS counter_type, DWORD cBytes);

/*
 * You must implement PerfWriteLog yourself and perform whatever logic is 
 * required to write out your specific data. Converting the raw data into 
 * whatever units make sense for your tests such as Kilobytes\Second 
 *
 * Once the metric has been calulated call the abstract method directly as
 * follows
 *                BTSPerfLog::PerfWriteLog(h, reinterpret_cast<PVOID>({your metric})
 *
 * The BTS standard requires that for each metric your test measures you 
 * also provide the minimum and maximum values for your test
 *
 * pv can be used for whatever purpose you find useful or ignored entirely
 */
    virtual bool PerfWriteLog(HANDLE h, PVOID pv) = 0;

    HANDLE usrDefinedHnd(LPCWSTR item);

 /*
 * Allows you to pass auxiliary data directly to Perfscenario's AddAuxData function.
 */
    bool AddAuxData(LPCTSTR lpszLabel, LPCTSTR lpszValue);
};

#pragma warning( pop )
#endif // __BTSPerfLog_H__
