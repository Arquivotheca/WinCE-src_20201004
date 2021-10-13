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
//------------------------------------------------------------------------------
//
//  Module Name:  
//  
//      TTracker.h
//  
//  Abstract:  
//
//      Defines the user APIs for interacting with the thread tracker
//      
//------------------------------------------------------------------------------

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif 

#ifdef TTRACKERDLL_EXPORTS
#define TTRACKERAPI 
#else
#define TTRACKERAPI __declspec(dllimport)
#endif

// ThreadTracker functionality or logging is disabled
#define TTRACKER_HR_DISABLED                HRESULT_FROM_WIN32(ERROR_SERVICE_DISABLED)
// The ttracker DLL is unavailable
#define TTRACKER_HR_NO_DLL                  HRESULT_FROM_WIN32(ERROR_DLL_NOT_FOUND)

// The pdwProcFilterArray and dwProcFilterCount members of the 
// structure are valid
#define TTRACKER_FILTER_FLAG_PROCARRAY_VALID  (0x00000001)

// The pdwProcFilterArray represents processes to be included
// when logging.  Without this flag, the list represents
// exclusions.  Set dwProcFilterCount to 0 without this flag
// to exlude no processes (include all).
#define TTRACKER_FILTER_FLAG_PROC_INCLUDE     (0x00000002)

typedef struct _TTRACKER_FILTER
{
    DWORD cbSize;
    DWORD dwFlags;
    DWORD * pdwProcFilterArray;
    DWORD dwProcFilterCount;
} TTRACKER_FILTER;

// Flags for TTrackerLogData.  May be combined.
#define TTRACKER_LOG_THREADS     (0x00000001)
#define TTRACKER_LOG_PROCESSES   (0x00000002)

// Do not use functions beginning with ThrdTracker*, use the TTracker*
// equivalents to gain the ability to transparently switch between
// static linking and delayloading. The behavior keys off of whether
// you have TTRACKER_DELAYLOAD defined.

TTRACKERAPI HRESULT ThrdTrackerRegister(HANDLE * phTTracker, LPCWSTR pwszLogFileName);
TTRACKERAPI HRESULT ThrdTrackerRegisterWithoutLogFile(HANDLE * phTTracker);
TTRACKERAPI BOOL ThrdTrackerUnregister(HANDLE hTTracker);
TTRACKERAPI HRESULT ThrdTrackerSetFilter(HANDLE hTTracker, TTRACKER_FILTER * pFilter);
TTRACKERAPI HRESULT ThrdTrackerLogData(HANDLE hTTracker, DWORD dwFlags, LPCWSTR pwszDescription);
TTRACKERAPI HRESULT ThrdTrackerLogToFile(HANDLE hTTracker, LPCWSTR pwszLogFileName, DWORD dwFlags, LPCWSTR pwszDescription);
TTRACKERAPI HRESULT ThrdTrackerGlobalLoggingEnable(BOOL bEnable);
TTRACKERAPI HRESULT ThrdTrackerKernelLoggingEnable(BOOL bEnable);
TTRACKERAPI HRESULT ThrdTrackerClearKernelLog();

typedef HRESULT (*PFN_TRACKER_REGISTER)(HANDLE *, LPCWSTR);
typedef HRESULT (*PFN_TRACKER_REGISTER_WITHOUT_LOG_FILE)(HANDLE *);
typedef BOOL    (*PFN_TRACKER_UNREGISTER)(HANDLE);
typedef HRESULT (*PFN_TRACKER_SET_FILTER)(HANDLE, TTRACKER_FILTER *);
typedef HRESULT (*PFN_TRACKER_LOG_DATA)(HANDLE, DWORD, LPCWSTR);
typedef HRESULT (*PFN_TRACKER_LOG_TO_FILE)(HANDLE, LPCWSTR, DWORD, LPCWSTR);
typedef HRESULT (*PFN_TRACKER_GLOBAL_LOG_ENABLE)(BOOL);
typedef HRESULT (*PFN_TRACKER_KERNEL_LOG_ENABLE)(BOOL);
typedef HRESULT (*PFN_TRACKER_CLEAR_KERNEL_LOG)();

typedef struct _TTRACKER_GLOBALS
{
    HINSTANCE hTTracker;
    DWORD  dwTTrackerRefs;
    PFN_TRACKER_REGISTER pfnRegister;
    PFN_TRACKER_REGISTER_WITHOUT_LOG_FILE pfnRegisterWithoutLogFile;
    PFN_TRACKER_UNREGISTER pfnUnregister;
    PFN_TRACKER_SET_FILTER pfnSetFilter;
    PFN_TRACKER_LOG_DATA pfnLogData;
    PFN_TRACKER_LOG_TO_FILE pfnLogToFile;
    PFN_TRACKER_GLOBAL_LOG_ENABLE pfnLogEnable;
    PFN_TRACKER_KERNEL_LOG_ENABLE pfnKernelLogEnable;
    PFN_TRACKER_CLEAR_KERNEL_LOG pfnClearKernelLog;
} TTRACKER_GLOBALS;

// Delayload help. Use these functions when not explicitly linking to ttracker.lib
// You'll need to declare g_pTTGlobals in one of your c/cpp source files as well.
#ifndef TTRACKERDLL_EXPORTS
#ifndef TTRACKER_DISABLED
#ifdef TTRACKER_DELAYLOAD

TTRACKERAPI HRESULT ThrdTrackerInit(TTRACKER_GLOBALS ** ppTTGlobals);
typedef HRESULT (*PFN_TRACKER_INIT)(TTRACKER_GLOBALS **);

extern TTRACKER_GLOBALS * g_pTTGlobals;

// You are not expected to call this function directly
_inline HRESULT __LoadTTrackerDLL()
{
    // WARNING! Not safe to call Register and Unregister
    // from different threads at the same time.
    HRESULT hr = S_OK;
    if (NULL == g_pTTGlobals)
    {
        HINSTANCE hTracker;
        PFN_TRACKER_INIT pfnInit;
        
        hTracker = LoadLibrary(TEXT("ttracker.dll"));
        if (NULL == hTracker)
        {
            RETAILMSG(1, (TEXT("LoadLibrary(ttracker.dll) failed. Last errror=%d\n"), GetLastError()));
            return HRESULT_FROM_WIN32(GetLastError());
        }

        pfnInit = (PFN_TRACKER_INIT) 
            GetProcAddress(hTracker, TEXT("ThrdTrackerInit"));

        if (NULL == pfnInit)
        {
            RETAILMSG(1, (TEXT("GetProcAddress(ThrdTrackerInit) failed. Last errror=%d\n"), GetLastError()));
            return HRESULT_FROM_WIN32(GetLastError());
        }

        hr = pfnInit(&g_pTTGlobals);
        if (SUCCEEDED(hr))
            g_pTTGlobals->hTTracker = hTracker;
        else
            g_pTTGlobals = NULL;
    }

    if (SUCCEEDED(hr))
    {
        InterlockedIncrement((LPLONG) &g_pTTGlobals->dwTTrackerRefs);
    }

    return hr;
}

_inline HRESULT TTrackerRegister(HANDLE * phTTracker, LPCWSTR pwszLogFileName)
{
    HRESULT hr = __LoadTTrackerDLL();
    if (SUCCEEDED(hr))
    {
        hr = g_pTTGlobals->pfnRegister(phTTracker, pwszLogFileName);
    }

    return hr;
}

_inline HRESULT TTrackerRegisterWithoutLogFile(HANDLE * phTTracker)
{
    HRESULT hr = __LoadTTrackerDLL();
    if (SUCCEEDED(hr))
    {
        hr = g_pTTGlobals->pfnRegisterWithoutLogFile(phTTracker);
    }

    return hr;
}

// Returns TRUE if other clients exist, FALSE if this was the last reference.
_inline BOOL TTrackerUnregister(HANDLE hTTracker)
{
    // WARNING! Not safe to call Register and Unregister
    // from different threads at the same time.
    if (g_pTTGlobals)
    {
        g_pTTGlobals->pfnUnregister(hTTracker);
        if (0 == InterlockedDecrement((LPLONG) &g_pTTGlobals->dwTTrackerRefs))
        {
            // __FreeTTrackerDLL
            HINSTANCE hTTracker = g_pTTGlobals->hTTracker;
            g_pTTGlobals = NULL;
            FreeLibrary(hTTracker);
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

_inline HRESULT TTrackerSetFilter(HANDLE hTTracker, TTRACKER_FILTER * pFilter)
{
    if (!g_pTTGlobals)
        return TTRACKER_HR_NO_DLL;

    return g_pTTGlobals->pfnSetFilter(hTTracker, pFilter);
}

_inline HRESULT TTrackerLogData(HANDLE hTTracker, DWORD dwFlags, LPCWSTR pwszDescription)
{
    if (!g_pTTGlobals)
        return TTRACKER_HR_NO_DLL;

    return g_pTTGlobals->pfnLogData(hTTracker, dwFlags, pwszDescription);
}

_inline HRESULT TTrackerLogToFile(HANDLE hTTracker, LPCWSTR pwszLogFileName,
                                  DWORD dwFlags, LPCWSTR pwszDescription)
{
    if (!g_pTTGlobals)
        return TTRACKER_HR_NO_DLL;

    return g_pTTGlobals->pfnLogToFile(hTTracker, pwszLogFileName,
                                      dwFlags, pwszDescription);
}

// Control writing to the logfile for all registered ttracker clients.
// Logging is suppressed until this function is called with bEnable = TRUE.
// Due to the implementation, at least one of the processes to be
// controlled must be registered with ttracker at the time this call
// is made.
_inline HRESULT TTrackerGlobalLoggingEnable(BOOL bEnable)
{
    if (!g_pTTGlobals)
        return TTRACKER_HR_NO_DLL;

    return g_pTTGlobals->pfnLogEnable(bEnable);
}

// Control whether ttrackerk.dll is recording data underneath, regardless
// of whether anyone is writing to a log file.
_inline HRESULT TTrackerKernelLoggingEnable(BOOL bEnable)
{
    if (!g_pTTGlobals)
        return TTRACKER_HR_NO_DLL;

    return g_pTTGlobals->pfnKernelLogEnable(bEnable);
}

// Clear the data in ttrackerk.dll's record.
_inline HRESULT TTrackerClearKernelLog()
{
    if (!g_pTTGlobals)
        return TTRACKER_HR_NO_DLL;

    return g_pTTGlobals->pfnClearKernelLog();
}

#else

#define TTrackerRegister ThrdTrackerRegister
#define TTrackerRegisterWithoutLogFile ThrdTrackerRegisterWithoutLogFile
#define TTrackerUnregister ThrdTrackerUnregister
#define TTrackerSetFilter ThrdTrackerSetFilter
#define TTrackerLogData ThrdTrackerLogData
#define TTrackerLogToFile ThrdTrackerLogToFile
#define TTrackerGlobalLoggingEnable ThrdTrackerGlobalLoggingEnable
#define TTrackerKernelLoggingEnable ThrdTrackerKernelLoggingEnable
#define TTrackerClearKernelLog ThrdTrackerClearKernelLog

#endif //TTRACKER_DELAYLOAD
#else

#define TTrackerRegister(phTTracker, pwszLogFileName)           (TTRACKER_HR_DISABLED)
#define TTrackerRegisterWithoutLogFile(phTTracker)              (TTRACKER_HR_DISABLED)
#define TTrackerUnregister(hTTracker)                           (FALSE)
#define TTrackerSetFilter(hTTracker, pFilter)                   (TTRACKER_HR_DISABLED)
#define TTrackerLogData(hTTracker, dwFlags, pwszDescription)    (TTRACKER_HR_DISABLED)
#define TTrackerLogToFile ThrdTrackerLogToFile                  (TTRACKER_HR_DISABLED)
#define TTrackerGlobalLoggingEnable(bEnable)                    (TTRACKER_HR_DISABLED)
#define TTrackerKernelLoggingEnable(bEnable)                    (TTRACKER_HR_DISABLED)
#define TTrackerClearKernelLog()                                (TTRACKER_HR_DISABLED)

#endif //TTRACKER_DISABLED

#endif //TTRACKERDLL_EXPORTS

#ifdef __cplusplus
}
#endif
