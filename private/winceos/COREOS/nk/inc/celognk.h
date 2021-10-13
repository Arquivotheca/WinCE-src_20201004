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

#ifndef __CELOGNK_H_
#define __CELOGNK_H_


// This must match the table in kcall.h.
// It is defined here so that it can be shared with readlog.
#define MAX_KCALL_PROFILE 77


#if (defined(IN_KERNEL) && !defined(NK_LOADER)) || defined(CELOG_UNIT_TEST)


//==============================================================================
//
//  Logging operation
//
//==============================================================================

// Include the public SDK structures.                
#include <celog.h>

// Include exported profiler functions
#include <profiler.h>

#define MAX_CALLSTACK   20

// Globals that allow a logging engine to be hot-plugged in the system.
#ifdef __cplusplus
extern void (*g_pfnCeLogData)(...);
#else
extern void (*g_pfnCeLogData)();
#endif
extern PFNVOID g_pfnCeLogInterrupt;
extern PFNVOID g_pfnCeLogInterruptData;
extern PFNVOID g_pfnCeLogSetZones;


#undef IsCeLogStatus
#undef IsCeLogEnabled
#undef IsCeLogZoneEnabled

#define KDataKernelCelogStatus          (g_pKData->dwCelogStatus)
#define IsCeLogStatus(Status)           ((KDataKernelCelogStatus & (Status)) != 0)
#define IsCeLogEnabled(Status, ZoneCE)  (IsCeLogStatus(Status) && ((KDataKernelCelogStatus & (ZoneCE)) != 0))
#define IsCeLogZoneEnabled(ZoneCE)      (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL, (ZoneCE)))

// Set the proper status and zones
#define CeLogEnable(Status, ZoneCE)     (KDataKernelCelogStatus = ((Status) | ((ZoneCE) & ~CELOGSTATUS_ENABLED_ANY)))
// Set the proper status without changing the zones
#define CeLogEnableStatus(Status)       (CeLogEnable((Status), KDataKernelCelogStatus & ~CELOGSTATUS_ENABLED_ANY))
// Set the proper zones without changing the status
#define CeLogEnableZones(ZoneCE)        (CeLogEnable(KDataKernelCelogStatus & CELOGSTATUS_ENABLED_ANY, (ZoneCE)))
// Disable all logging
#define CeLogDisable()                  (KDataKernelCelogStatus = 0)


// Kernel logger functions
void NKCeLogData(BOOL fTimeStamp, DWORD dwID, VOID *pData, DWORD dwLen, DWORD dwZoneUser, DWORD dwZoneCE, DWORD dwFlag, BOOL fFlagged);
void NKCeLogSetZones(DWORD dwZoneUser, DWORD dwZoneCE, DWORD dwZoneProcess);
BOOL NKCeLogGetZones(LPDWORD lpdwZoneUser, LPDWORD lpdwZoneCE, LPDWORD lpdwZoneProcess, LPDWORD lpdwAvailableZones);
BOOL NKCeLogReSync();

void LoggerInit (void);

BOOL KernelLibIoControl_CeLog(
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    );

#define CELOG_ISR_ENTRY_FLAG 0x80000000


//==============================================================================
//
// Default zone settings
//
//==============================================================================

#ifndef CELOG_USER_MASK
#define CELOG_USER_MASK    0xFFFFFFFF
#endif

// GWES events are turned off by default to prevent data the user doesn't know
#ifndef CELOG_CE_MASK
#define CELOG_CE_MASK      (0xFFFFFFFF & ~CELZONE_GWES)
#endif

#ifndef CELOG_PROCESS_MASK
#define CELOG_PROCESS_MASK 0xFFFFFFFF
#endif


//==============================================================================
//
// Interface to a CeLog plugin DLL
//
//==============================================================================

#define CELOG_IMPORT_VERSION            5
#define CELOG_PRIVATE_IMPORT_VERSION    2
#define CELOG_EXPORT_VERSION            3

typedef LONG (*PFN_RegQueryValueExW) (HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef BOOL (*PFN_INTERRUPTS_ENABLE) (BOOL);
typedef LPVOID (*PFN_Pfn2Virt) (DWORD);
typedef int (*PFN_KCall) (PKFN, ...);
typedef void (*PFN_CeLogData) (BOOL, DWORD, PVOID, DWORD, DWORD, DWORD, DWORD, BOOL);


// Private kernel functions and data provided for CeLog to use.
// Queried with IOCTL_CELOG_IMPORT_PRIVATE.
// This structure may change on future OS versions.
typedef struct _CeLogPrivateImports {
    DWORD    dwVersion;                       // Version of this structure, set to CELOG_PRIVATE_IMPORT_VERSION
    PFN_RegQueryValueExW  pRegQueryValueExW;  // Pointer to NKRegQueryValueExW function
    PFN_INTERRUPTS_ENABLE pINTERRUPTS_ENABLE; // Pointer to INTERRUPTS_ENABLE function
    PFN_Pfn2Virt          pPfn2Virt_DO_NOT_USE;          // Pointer to Pfn2Virt wrapper function
    PFN_KCall             pKCall;             // Pointer to KCall function
    PFN_CeLogData         pCeLogData;         // Pointer to kernel's CeLogData entry point
    struct KDataStruct   *pKData;             // Pointer to KData struct
} CeLogPrivateImports;


//==============================================================================
//
// The instrumentation that logs all the kernel CeLog events.
//
//==============================================================================

#ifdef ARM
void CELOG_Interrupt(DWORD dwLogValue, DWORD ra);
#else
void CELOG_Interrupt(DWORD dwLogValue);
#endif

void CELOG_OutputDebugString(DWORD dwProcessId, DWORD dwThreadId, LPCWSTR pszMessage);
void CELOG_ThreadMigrate(DWORD dwProcessId);
void CELOG_KCThreadSwitch(PTHREAD pThread);
void CELOG_KCThreadQuantumExpire();
void CELOG_KCThreadPriorityInvert(PTHREAD pThread);
void CELOG_KCThreadSetPriority(PTHREAD pThread, int nPriority);
void CELOG_ThreadSetQuantum(PTHREAD pThread, DWORD dwQuantum);
void CELOG_KCThreadSuspend(PTHREAD pThread);
void CELOG_KCThreadResume(PTHREAD pThread);
void CELOG_ThreadCreate(PTHREAD pThread, PPROCESS pProcess, LPVOID lpvThreadParam);
void CELOG_ThreadTerminate(PTHREAD pThread);
void CELOG_ThreadDelete(PTHREAD pThread);
void CELOG_ProcessCreateEx(PPROCESS pProc);
void CELOG_ProcessTerminate(PPROCESS pProcess);
void CELOG_ProcessDelete(PPROCESS pProcess);
void CELOG_Sleep(DWORD dwTimeout);
void CELOG_OpenOrCreateHandle(PHDATA phd, BOOL fCreate);
void CELOG_EventModify(PEVENT pEvent, DWORD type);
void CELOG_EventDelete(PEVENT pEvent);
void CELOG_WaitForMultipleObjects(DWORD cObjects, PHDATA* phds, DWORD dwTimeout);
void CELOG_MutexRelease(PMUTEX pMutex);
void CELOG_MutexDelete(PMUTEX pMutex);
void CELOG_SemaphoreRelease(PSEMAPHORE pSemaphore, LONG lReleaseCount, LONG lPreviousCount);
void CELOG_SemaphoreDelete(PSEMAPHORE pSemaphore);
void CELOG_CriticalSectionDelete(PMUTEX pCriticalSection);
void CELOG_CriticalSectionEnter(PMUTEX pCriticalSection, LPCRITICAL_SECTION lpcs);
void CELOG_CriticalSectionLeave(DWORD dwOwnerId, LPCRITICAL_SECTION lpcs);
void CELOG_VirtualAlloc(PPROCESS pProcess, DWORD dwResult, DWORD dwAddress, DWORD cPages, DWORD dwType, DWORD dwProtect);
void CELOG_VirtualCopy(PPROCESS pProcessDest, DWORD dwDest, PPROCESS pProcessSrc, DWORD dwSource, DWORD cPages, DWORD dwPfn, DWORD dwProtect);
void CELOG_VirtualFree(PPROCESS pProcess, DWORD dwAddress, DWORD cPages, DWORD dwType);
void CELOG_MapFileCreate(PFSMAP pMap, PNAME pMapName);
void CELOG_MapFileDestroy(PFSMAP pMap);
void CELOG_MapFileViewOpen(PPROCESS pProcess, PMAPVIEW pView);
void CELOG_MapFileViewClose(PPROCESS pProcess, PMAPVIEW pView);
void CELOG_MapFileViewFlush(PPROCESS pProcess, PFSMAP pfsmap, LPBYTE pFlushStart, DWORD cbSize, DWORD cDirtyPages, BOOL fBegin);
void CELOG_MapFileValidate(DWORD cbSize, DWORD dwRestoreSize, DWORD dwRestoreFlags, BOOL fBegin);
void CELOG_PageFault(PPROCESS pProcess, DWORD dwAddress, BOOL fBegin, BOOL fOtherFlag);
void CELOG_ModuleLoad(PPROCESS pProcess, PMODULE pModule);
void CELOG_ModuleFree(PPROCESS pProcess, PMODULE pModule);
void CELOG_ModuleFailedLoad (DWORD dwProcessId, DWORD dwFlags, LPCWSTR lpwszModuleName);
void CELOG_LaunchingFilesys();
void CELOG_LowMemSignalled(long pageFreeCount, int  cpNeed, long cpLowThreshold, long cpCriticalThreshold, long cpLowBlockSize, long cpCriticalBlockSize);

BOOL KCallCeLogReSync();

// Special functions used only by CELOG_ReSync, to minimize stack usage
void CELOG_SyncProcess(PPROCESS pProcess);
void CELOG_SyncThread(PTHREAD pThread, PPROCESS pProcess);
void CELOG_SyncModule(PMODULE pModule);
BOOL CELOG_SyncModuleRefCount(PMODULELIST pml, PPROCESS pProcess, BOOL Reset);



//==============================================================================
//
//  Profiler definitions
//
//==============================================================================

void GetThreadName(
    PTHREAD pth,
    HANDLE* hModule,
    WCHAR*  szThreadFunctionName
    );

void
NKProfileSyscall(
    const VOID* pControl,       // I_PTR, variable-sized struct
    DWORD cbControl
    );

void
EXTProfileSyscall(
    const VOID* pControl,       // I_PTR, variable-sized struct
    DWORD cbControl
    );


typedef struct {
    BOOL  bProfileTimerRunning; // Is the OEM profiler interrupt running?
    DWORD dwProfilerFlags;
    
    // State used inside ProfileStart/ProfileStop
    int   scPauseContinueCalls;
    BOOL  bStart;               // Is the profiler started?  (TRUE even while paused)
    struct {
        DWORD dwZoneUser;
        DWORD dwZoneCE;
        DWORD dwZoneProcess;
    } SavedCeLogZones;

    // Used for symbol lookup
    DWORD dwNumROMModules;      // Number of modules in all XIP regions

    // Total number of profiler samples is dwSamplesRecorded + dwSamplesDropped + dwSamplesInIdle
    DWORD dwSamplesRecorded;    // Number of profiler hits that were recorded in the buffer or symbol list
    DWORD dwSamplesDropped;     // Number of profiler hits that were not recorded due to buffer overrun

    // System state that is tracked while the profiler is running in Monte Carlo mode.
    DWORD dwSamplesInIdle;      // Number of profiler samples when no threads were scheduled
    DWORD dwInterrupts;         // Number of interrupts that occurred while profiling, including profiler interrupt
    
    // System state counters that will run when profiling is paused are tracked by 
    // recording start values while running and elapsed values during the pause.
    DWORD dwTLBCount;           // Used to count TLB misses while profiling
    DWORD dwTickCount;          // Used to count ticks while profiling
} ProfilerState;

extern ProfilerState g_ProfilerState;


// Number of TLB misses that had profiler interrupts pending.  Tracked while
// the profiler is running in Monte Carlo mode.  This value is not stored in
// the ProfilerState structure because it is more easily accessed from assembly
// code this way.
extern DWORD g_ProfilerState_dwProfilerIntsInTLB;

// These values can be overwritten as FIXUPVARs in config.bib
extern const volatile DWORD dwProfileBufferMax;         // Size of profiling buffer to allocate (if possible)
extern const volatile DWORD dwProfileBootUSecInterval;  // Interval to pass to ProfileStart() on boot
extern const volatile DWORD dwProfileBootOptions;       // Flags to pass to ProfileStart() on boot

extern BOOL CommonProfileStart(const ProfilerControl* pControl, DWORD dwControlSize);
extern void CommonProfileStop(const ProfilerControl* pControl, DWORD dwControlSize);

#ifdef NKPROF

extern void  PROF_ClassicProfilerHit(OEMProfilerData *pData);
extern void  PROF_ClassicProfileStart(const ProfilerControl* pControl, DWORD dwControlSize);
extern void  PROF_ClassicProfileStop(const ProfilerControl* pControl, DWORD dwControlSize);
extern void  PROF_GetThreadName(PTHREAD pth, HANDLE* hModule, WCHAR* szThreadFunctionName);
extern BOOL  PROF_LogIntCallStack();

#endif // NKPROF


//==============================================================================
// KCall Profiling
//==============================================================================

#ifdef NKPROF

typedef struct KPRF_t {
    DWORD    hits;
    LONGLONG max;
    LONGLONG min;
    LONGLONG total;
    LONGLONG tmp;
} KPRF_t;

extern KPRF_t (* KPRFInfo)[MAX_KCALL_PROFILE];

_inline void KCALLPROFON(int IND) {
    LARGE_INTEGER liPerf;
    if (g_ProfilerState.dwProfilerFlags & PROFILE_KCALL) {
        DWORD idxCpu = PcbGetCurCpu()-1;
        DEBUGCHK(InSysCall());
        DEBUGCHK(KPRFInfo && !KPRFInfo[idxCpu][IND].tmp);
        DEBUGCHK(IND<MAX_KCALL_PROFILE);
        NKQueryPerformanceCounter(&liPerf);
        KPRFInfo[idxCpu][IND].tmp = liPerf.QuadPart;
    }
}

_inline void KCALLPROFOFF(int IND) {
    LARGE_INTEGER liPerf;
    LONGLONG t2;
    if (g_ProfilerState.dwProfilerFlags & PROFILE_KCALL) {
        DWORD idxCpu = PcbGetCurCpu()-1;
        DEBUGCHK(InSysCall());
        DEBUGCHK(KPRFInfo && KPRFInfo[idxCpu][IND].tmp);
        DEBUGCHK(IND<MAX_KCALL_PROFILE);
        NKQueryPerformanceCounter(&liPerf);
        t2 = liPerf.QuadPart - KPRFInfo[idxCpu][IND].tmp;
        KPRFInfo[idxCpu][IND].tmp = 0;
        if (t2 > KPRFInfo[idxCpu][IND].max)
            KPRFInfo[idxCpu][IND].max = t2;
        if (t2 && (!KPRFInfo[idxCpu][IND].min || (t2 < KPRFInfo[idxCpu][IND].min)))
            KPRFInfo[idxCpu][IND].min = t2;
        KPRFInfo[idxCpu][IND].total += t2;
        KPRFInfo[idxCpu][IND].hits++;
    }
}

#else // NKPROF

#define KCALLPROFON(IND) 0
#define KCALLPROFOFF(IND) 0

#endif // NKPROF

//==============================================================================


#endif // IN_KERNEL

#endif // __CELOGNK_H_
