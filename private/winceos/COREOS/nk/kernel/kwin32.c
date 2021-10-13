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
/**     TITLE("Kernel Win32 Handle")
 *++
 *
 *
 * Module Name:
 *
 *    KWin32.c
 *
 * Abstract:
 *
 *  This file contains the definition of the Win32 system API handle.
 *
 *--
 */
#include "kernel.h"
#include "ksysdbg.h"
#include <watchdog.h>
#include <oalioctl.h>
#include <pm.h>
#include <snapboot.h>

// for FSNOTIFY_POWER_OFF
#include <extfile.h>

VOID WINAPI Win32OutputDebugStringW (LPCWSTR str);
extern PFN_SetSystemPowerState g_pfnSetSystemPowerState;

extern TSnapState g_snapState;

BOOL NKMakeSnapshot (void);
BOOL NKPrepareForSnapshot (void);
BOOL VerifySnap (void);


static void NKNotifyMemoryLow (void)
{
    // signal the psl notification thread
    EVNTModify (GetEventPtr (phdPslNotifyEvent), EVENT_SET);
}

static BOOL CallOEMIoControl (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned)
{
    BOOL fRet;
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);

    fRet = (IOCTL_HAL_REBOOT == dwIoControlCode)
            ? NKReboot (FALSE)
            : OEMIoControl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);

    SwitchActiveProcess (pprc);    
    return fRet;
}

extern BOOL (* g_pfnExtKITLIoctl) (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);

//
// Utility Function to convert integer to %08X formatted string
//
void 
NKItoW (
    DWORD dwInt, 
    __out_ecount (cchString) LPWSTR pwszString, 
    DWORD cchString
    )
{
    WCHAR wch;
    int idx = cchString;

    if ((pwszString == NULL) || (cchString == 0))
        return;
    
    pwszString[0] = 0;

    DEBUGCHK(cchString >= 9);
    if (cchString < 9)
        return;

    // terminate string
    pwszString[--idx] = 0;

    // at start idx == 8, count = 1
    while (idx--) {
        // convert four bits at a time to wchar
        DWORD itok = dwInt & 0xF;
        if (itok >= 0 && itok <= 9) 
            wch = (WCHAR) (itok + L'0');
        else
            wch = (WCHAR) (itok - 0xa + L'A');
        
        pwszString[idx] = wch;
        dwInt >>= 4;
    }

}

//
// Helper function to call OalIoctl() in oalIoctl.dll 
//
#define OAL_IOCTL_NAME_LENGTH (9) //DWORD is 8 chars + NULL
BOOL CallOalIoctl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned)
{
    BOOL fRet = FALSE;

    if (NULL == g_pfnOalIoctl) {
        return fRet;
    }
    
    fRet = (*g_pfnOalIoctl)(dwIoControlCode, lpInBuf, nInBufSize, 
                lpOutBuf, nOutBufSize, lpBytesReturned);

    return fRet;
}

//
// external version of kernel io control API call
//
static BOOL EXTKernelIoctl (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned)
{
    BOOL fRet = FALSE;
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    switch (dwIoControlCode) {        
        // KITL ioctls
        case IOCTL_EDBG_REGISTER_CLIENT:
        case IOCTL_EDBG_DEREGISTER_CLIENT:
        case IOCTL_EDBG_REGISTER_DFLT_CLIENT:
        case IOCTL_EDBG_SEND:
        case IOCTL_EDBG_RECV:
        case IOCTL_KITL_GET_CAPABILITIES:
        case IOCTL_EDBG_SET_DEBUG:
        case IOCTL_EDBG_IS_STARTED:
        case IOCTL_KITL_REGISTER_MONITOR_THREAD:
            fRet = (g_pfnExtKITLIoctl) ? 
                   (*g_pfnExtKITLIoctl) (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned)
                   : FALSE;
        break;
       
        default:
            fRet = CallOalIoctl(dwIoControlCode, lpInBuf, nInBufSize, 
                                    lpOutBuf, nOutBufSize, lpBytesReturned);
        break;
    }

    SwitchActiveProcess (pprc);
    
    return fRet;
}

PMODULE EXTInitModule (PMODULE pMod, PUSERMODULELIST pUMod);

/*
  Get the resource section of the calling process.
  Call not allowed for kernel threads (not in API
  calls) as kernel doesn't have any resources.
*/
PCInfo NKGetCallerResInfo (LPDWORD pdwBase, PMODULE *pModMui)
{
    PPROCESS pprc = pVMProc;
    PTHREAD pth = pCurThread;
    PCInfo pcInfo = NULL;
    
    if ((pth->pprcOwner != g_pprcNK) || pth->pcstkTop) {
        // only support the call for user mode threads or
        // kernel mode threads which are in a PSL call.
        *pdwBase = (DWORD) pprc->BasePtr;
        *pModMui = pprc->pmodResource;
        pcInfo = &pprc->e32.e32_unit[RES];
    }

    return pcInfo;
}

void DoResetStack (PPROCESS pprc, DWORD dwCurSP, LPDWORD pTlsPtr, BOOL fUpdateStatistic)
{
    LONG cbDecommit = PAGEALIGN_DOWN (dwCurSP) - (pTlsPtr[PRETLS_STACKBOUND] + VM_PAGE_SIZE);

    if (((int) cbDecommit > 0) && (dwCurSP < (DWORD) pTlsPtr)) {
        DWORD dwAddr = pTlsPtr[PRETLS_STACKBOUND];
        
        // NOTE: we're assuming that VMDecommit will *NOT* use more than 1 page of stack.
        pTlsPtr[PRETLS_STACKBOUND] += cbDecommit;
        VMDecommit (pprc,
                    (LPVOID) dwAddr,
                    cbDecommit,
                    VM_PAGER_AUTO);

        if (fUpdateStatistic) {
            UNREFERENCED_PARAMETER (fUpdateStatistic);
        }
    }
}

void KrnResetStack (void)
{
    PTHREAD pth = pCurThread;

    DoResetStack (g_pprcNK, (DWORD) &pth, pth->tlsSecure, TRUE);
    
}


void UsrResetStack (void)
{
    PTHREAD pth     = pCurThread;
    DWORD   dwCurSP = pth->pcstkTop->dwPrevSP;
    
    DoResetStack (pActvProc, dwCurSP, pth->tlsNonSecure, dwCurSP - pth->dwOrigBase < pth->dwOrigStkSize);
}

/*

Note about the API set: All APIs should either return 0 on error or return
no value at-all. This will allow us to return gracefully to the user when an
argument to an API doesn't pass the PSL checks. Otherwise we will end
up throwing exceptions on invalid arguments to API calls.

*/

const PFNVOID Win32ExtMethods[] = {
    (PFNVOID)NotSupported,                  // 0 - user mode not exposed
    (PFNVOID)NotSupported,                  // 1
    (PFNVOID)EXTCreateAPISet,               // 2 - CreateAPISet    
    (PFNVOID)NKCreateProcess,               // 3 - CreateProcess
    (PFNVOID)NKOpenProcess,                 // 4 - OpenProcess
    (PFNVOID)NKCreateThread,                // 5 - CreateThread
    (PFNVOID)NKOpenThread,                  // 6 - OpenThread
    (PFNVOID)NKCreateEvent,                 // 7 - CreateEvent
    (PFNVOID)NKOpenEvent,                   // 8 - OpenEvent
    (PFNVOID)NKCreateMutex,                 // 9 - CreateMutex
    (PFNVOID)NKOpenMutex,                   // 10 - OpenMutex
    (PFNVOID)NKCreateSemaphore,             // 11 - CreateSemaphore
    (PFNVOID)NKOpenSemaphore,               // 12 - OpenSemaphore
    (PFNVOID)NKExitThread,                  // 13 - NKExitThread (kernel cleanup)
    (PFNVOID)NKTlsCall,                     // 14 - TlsCall
    (PFNVOID)NKIsBadPtr,                    // 15 - IsBadPtr
    (PFNVOID)NotSupported,                  // 16 - CeVirtualSharedAlloc (not supported for user mode)
    (PFNVOID)EXTCRITCreate,                 // 17 - InitializeCriticalSection
    (PFNVOID)UB_Sleep,                      // 18 - Sleep
    (PFNVOID)NKSetLastError,                // 19 - SetLastError
    (PFNVOID)NKGetLastError,                // 20 - GetLastError
    (PFNVOID)EXTLoadLibraryEx,              // 21 - LoadLibraryEx
    (PFNVOID)NKFreeLibrary,                 // 22 - FreeLibrary
    (PFNVOID)NKCreateWatchDog,              // 23 - CreateWatchDog
    (PFNVOID)NotSupported,                  // 24 - Unsupported
    (PFNVOID)NKDuplicateHandle,             // 25 - DuplicateHandle
    (PFNVOID)NKWaitForMultipleObjects,      // 26 - WaitForMultipleObjects
    (PFNVOID)Win32OutputDebugStringW,       // 27 - OutputDebugString
    (PFNVOID)NotSupported,                  // 28 - NKvDbgPrintfW (not supported for user mode)
    (PFNVOID)CaptureContext,                // 29 - RaiseException
    (PFNVOID)NotSupported,                  // 30 - PerformCallBack (not supported for user mode)
    (PFNVOID)OEMGetTickCount,               // 31 - GetTickCount
    (PFNVOID)NKGetProcessVersion,           // 32 - GetProcessVersion
    (PFNVOID)NotSupported,                  // 33 - GetRomFileBytes (not supported for user mode)
    (PFNVOID)NotSupported,                  // 34 - Unsupported
    (PFNVOID)NotSupported,                  // 35 - Unsupported
    (PFNVOID)NotSupported,                  // 36 - GetRomFileInfo (not supported for user mode)
    (PFNVOID)NKGetSystemInfo,               // 37 - GetSystemInfo
    (PFNVOID)NotSupported,                  // 38 - GetFSHeapInfo (not supported for user mode)
    (PFNVOID)EXTKernelIoctl,                // 39 - KernelIoControl
    (PFNVOID)EXTKernelLibIoControl,         // 40 - KernelLibIoControl
    (PFNVOID)NKGetTime,                     // 41 - NKGetTime
    (PFNVOID)EXTGetCallerProcessId,         // 42 - GetDirectCallerProcessId
    (PFNVOID)NotSupported,                  // 43 - Unsupported
    (PFNVOID)NKGetIdleTime,                 // 44 - GetIdleTime
    (PFNVOID)NKCacheRangeFlush,             // 45 - CacheRangeFlush
    (PFNVOID)EXTInitModule,                 // 46 - LoadExistingModule (user mode only)
    (PFNVOID)NKWaitForDebugEvent,           // 47 - WaitForDebugEvent
    (PFNVOID)NKContinueDebugEvent,          // 48 - ContinueDebugEvent
    (PFNVOID)NKDebugNotify,                 // 49 - DebugNotify
    (PFNVOID)NKAttachDebugger,              // 50 - ConnectDebugger (not supported for user mode)
    (PFNVOID)NotSupported,                  // 51 - Binary compress (not supported for user mode)
    (PFNVOID)ExtBinaryDecompress,           // 52 - Binary de-compress (protected with privileges)
    (PFNVOID)NotSupported,                  // 53 - decompress binary block (not supported for user mode)
    (PFNVOID)NotSupported,                  // 54 - StringCompress (not supported for user mode)
    (PFNVOID)NotSupported,                  // 55 - StringDecompress (not supported for user mode)
    (PFNVOID)NotSupported,                  // 56 - CeGetRandomSeed
    (PFNVOID)NKSleepTillTick,               // 57 - SleepTillTick
    (PFNVOID)NKQueryPerformanceFrequency,   // 58 - QueryPerformanceFrequency
    (PFNVOID)NKQueryPerformanceCounter,     // 59 - QueryPerformanceCounter
    (PFNVOID)THRDGetTimes,                  // 60 - GetThreadTimes
    (PFNVOID)APISQueryID,                   // 61 - QueryAPISetID
    (PFNVOID)NKIsNamedEventSignaled,        // 62 - IsNamedEventSignaled
    (PFNVOID)EXTCreateLocaleView,           // 63 - CreateLocaleView
    (PFNVOID)NKRegisterDbgZones,            // 64 - RegisterDbgZones
    (PFNVOID)NKSetDbgZone,                  // 65 - NKSetDbgZones
    (PFNVOID)NKCreateMsgQueue,              // 66 - CreateMsgQueue
    (PFNVOID)OEMWriteDebugLED,              // 67 - WriteDebugLED
    (PFNVOID)NKIsProcessorFeaturePresent,   // 68 - IsProcessorFeaturePresent
    (PFNVOID)NKQueryInstructionSet,         // 69 - QueryInstructionSet
    (PFNVOID)NKSetKernelAlarm,              // 70 - SetKernelAlarm
    (PFNVOID)NKRefreshKernelAlarm,          // 71 - RefreshKernelAlarm
    (PFNVOID)NotSupported,                  // 72 - SetOOMEventEx (not supported in user mode)
    (PFNVOID)NKSetTime,                     // 73 - NKSetTime
    (PFNVOID)NotSupported,                  // 74 - GetKPhys  (not supported in user mode)
    (PFNVOID)NotSupported,                  // 75 - GiveKPhys (not supported in user mode)
    (PFNVOID)InputDebugCharW,               // 76 - InputDebugCharW
    (PFNVOID)NotSupported,                  // 77 - SetLowestScheduledPriority  (not supported in user mode)
    (PFNVOID)NKUpdateNLSInfo,               // 78 - UpdateNLSInfoEx
    (PFNVOID)NotSupported,                  // 79 - ReadRegistryFromOEM  (not supported in user mode)
    (PFNVOID)NotSupported,                  // 80 - WriteRegistryToOEM  (not supported in user mode)
    (PFNVOID)NKGetStdioPathW,               // 81 - GetStdioPathW
    (PFNVOID)NKSetStdioPathW,               // 82 - SetStdioPathW
    (PFNVOID)NotSupported,                  // 83 - SetHardwareWatch
    (PFNVOID)NKPrepareThreadExit,           // 84 - PrepareThreadExit
    (PFNVOID)NotSupported,                  // 85 - InterruptInitialize
    (PFNVOID)NotSupported,                  // 86 - InterruptDisable
    (PFNVOID)NotSupported,                  // 87 - InterruptDone
    (PFNVOID)NotSupported,                  // 88 - InterruptMask
    (PFNVOID)NotSupported,                  // 89 - SetPowerHandler  (not supported in user mode)
    (PFNVOID)NotSupported,                  // 90 - PowerOffSystem  (not supported in user mode)
    (PFNVOID)NotSupported,                  // 91 - LoadIntChainHandler
    (PFNVOID)NotSupported,                  // 92 - FreeIntChainHandler
    (PFNVOID)PROCGetCode,                   // 93 - GetExitCodeProcess
    (PFNVOID)THRDGetCode,                   // 94 - GetExitCodeThread
    (PFNVOID)NotSupported,                  // 95 - CreateStaticMapping
    (PFNVOID)NKSetDaylightTime,             // 96 - SetDaylightTime
    (PFNVOID)NKSetTimeZoneBias,             // 97 - SetTimeZoneBias
    (PFNVOID)NKTHCreateSnapshot,            // 98 - THCreateSnapshot (kernel info only)
    (PFNVOID)NotSupported,                  // 99 - RemoteLocalAlloc
    (PFNVOID)NotSupported,                  // 100 - RemoteLocalReAlloc
    (PFNVOID)NotSupported,                  // 101 - RemoteLocalSize
    (PFNVOID)NotSupported,                  // 102 - RemoteLocalFree
    (PFNVOID)NotSupported,                  // 103 - NKPSLNotify
    (PFNVOID)NKNotifyMemoryLow,             // 104 - SystemMemoryLow
    (PFNVOID)NKLoadKernelLibrary,           // 105 - LoadKernelLibrary
    (PFNVOID)NKCeLogData,                   // 106 - CeLogData
    (PFNVOID)NKCeLogSetZones,               // 107 - CeLogSetZones
    (PFNVOID)NKCeLogGetZones,               // 108 - CeLogGetZones
    (PFNVOID)NKCeLogReSync,                 // 109 - CeLogReSync
    (PFNVOID)NKWaitForAPIReady,             // 110 - WaitForAPIReady
    (PFNVOID)NotSupported,                  // 111 - SetRAMMode
    (PFNVOID)NotSupported,                  // 112 - SetStoreQueueBase
    (PFNVOID)HNDLGetServerId,               // 113 - GetHandleServerId
    (PFNVOID)NotSupported,                  // 114 - Unsupported
    (PFNVOID)NotSupported,                  // 115 - Unsupported
    (PFNVOID)NotSupported,                  // 116 - SetDirectCall (k-mode only)
    (PFNVOID)EXTGetCallerProcessId,         // 117 - GetCallerVMProcessId
    (PFNVOID)EXTProfileSyscall,             // 118 - ProfileSyscall
    (PFNVOID)NKDebugSetProcessKillOnExit,   // 119 - DebugSetProcessKillOnExit    
    (PFNVOID)NKDebugActiveProcessStop,      // 120 - DebugActiveProcessStop        
    (PFNVOID)NKDebugActiveProcess,          // 121 - DebugActiveProcess     
    (PFNVOID)EXTOldGetCallerProcess,        // 122 - GetCallerProcess (old behavior)
    (PFNVOID)NotSupported,                  // 123 - ReadKernelPEHeader (not supported in user mode)
    (PFNVOID)NotSupported,                  // 124 - ForcePageout (not supported in user mode)
    (PFNVOID)NotSupported,                  // 125 - DeleteStaticMapping
    (PFNVOID)NKGetRawTime,                  // 126 - CeGetRawTime
    (PFNVOID)NKFileTimeToSystemTime,        // 127 - FileTimeToSystemTime
    (PFNVOID)NKSystemTimeToFileTime,        // 128 - SystemTimeToFileTime
    (PFNVOID)NKGetTimeAsFileTime,           // 129 - NKGetTimeAsFileTime
    (PFNVOID)NKGetCPUInfo,                  // 130 - NKGetCPUInfo
    (PFNVOID)NotSupported,                  // 131 - NKCPUPowerFunc - not supported in user ode
    (PFNVOID)NotSupported,                  // 132 - Unsupported
    (PFNVOID)NotSupported,                  // 133 - Unsupported
    (PFNVOID)NotSupported,                  // 134 - Unsupported
    (PFNVOID)NotSupported,                  // 135 - Unsupported
    (PFNVOID)NotSupported,                  // 136 - Unsupported
    (PFNVOID)NotSupported,                  // 137 - Unsupported
    (PFNVOID)NotSupported,                  // 138 - Unsupported
    (PFNVOID)UsrResetStack,                 // 139 - CeResetStack
    };

const PFNVOID Win32IntMethods[] = {
    (PFNVOID)NKHandleCall,                  // 0 - SPECIAL case for WIN32 method, direct entry for handle based calls
    (PFNVOID)NotSupported,                  // 1
    (PFNVOID)NKCreateAPISet,                // 2 - CreateAPISet    
    (PFNVOID)NKCreateProcess,               // 3 - CreateProcess
    (PFNVOID)NKOpenProcess,                 // 4 - OpenProcess
    (PFNVOID)NKCreateThread,                // 5 - CreateThread
    (PFNVOID)NKOpenThread,                  // 6 - OpenThread
    (PFNVOID)NKCreateEvent,                 // 7 - CreateEvent
    (PFNVOID)NKOpenEvent,                   // 8 - OpenEvent
    (PFNVOID)NKCreateMutex,                 // 9 - CreateMutex
    (PFNVOID)NKOpenMutex,                   // 10 - OpenMutex
    (PFNVOID)NKCreateSemaphore,             // 11 - CreateSemaphore
    (PFNVOID)NKOpenSemaphore,               // 12 - OpenSemaphore
    (PFNVOID)NKExitThread,                  // 13 - NKExitThread (kernel cleanup)
    (PFNVOID)NKTlsCall,                     // 14 - TlsCall
    (PFNVOID)NKIsBadPtr,                    // 15 - IsBadPtr
    (PFNVOID)NKVirtualSharedAlloc,          // 16 - CeVirtualSharedAlloc
    (PFNVOID)CRITCreate,                    // 17 - InitializeCriticalSection
    (PFNVOID)UB_Sleep,                      // 18 - Sleep
    (PFNVOID)NKSetLastError,                // 19 - SetLastError
    (PFNVOID)NKGetLastError,                // 20 - GetLastError
    (PFNVOID)NKLoadLibraryEx,               // 21 - LoadLibraryEx
    (PFNVOID)NKFreeLibrary,                 // 22 - FreeLibrary
    (PFNVOID)NKCreateWatchDog,              // 23 - CreateWatchDog
    (PFNVOID)NotSupported,                  // 24 - Unsupported
    (PFNVOID)NKDuplicateHandle,             // 25 - DuplicateHandle
    (PFNVOID)NKWaitForMultipleObjects,      // 26 - WaitForMultipleObjects
    (PFNVOID)Win32OutputDebugStringW,       // 27 - OutputDebugString
    (PFNVOID)NKvDbgPrintfW,                 // 28 - NKvDbgPrintfW
    (PFNVOID)CaptureContext,                // 29 - RaiseException
    (PFNVOID)NKPerformCallBack,             // 30 - PerformCallBack
    (PFNVOID)OEMGetTickCount,               // 31 - GetTickCount
    (PFNVOID)NKGetProcessVersion,           // 32 - GetProcessVersion    
    (PFNVOID)NKGetRomFileBytes,             // 33 - GetRomFileBytes
    (PFNVOID)NotSupported,                  // 34 - Unsupported
    (PFNVOID)NotSupported,                  // 35 - Unsupported
    (PFNVOID)NKGetRomFileInfo,              // 36 - GetRomFileInfo 
    (PFNVOID)NKGetSystemInfo,               // 37 - GetSystemInfo
    (PFNVOID)NKGetFSHeapInfo,               // 38 - GetFSHeapInfo
    (PFNVOID)KernelIoctl,                   // 39 - KernelIoControl
    (PFNVOID)NKKernelLibIoControl,          // 40 - KernelLibIoControl
    (PFNVOID)NKGetTime,                     // 41 - NKGetTime
    (PFNVOID)NKGetDirectCallerProcessId,    // 42 - GetDirectCallerProcessId
    (PFNVOID)NotSupported,                  // 43 - Unsupported
    (PFNVOID)NKGetIdleTime,                 // 44 - GetIdleTime
    (PFNVOID)NKCacheRangeFlush,             // 45 - CacheRangeFlush
    (PFNVOID)NKGetCallerResInfo,            // 46 - GetCallerResInfo (k-mode only, overload with u-mode only LoadExistingModule)
    (PFNVOID)NKWaitForDebugEvent,           // 47 - WaitForDebugEvent
    (PFNVOID)NKContinueDebugEvent,          // 48 - ContinueDebugEvent
    (PFNVOID)NKDebugNotify,                 // 49 - DebugNotify
    (PFNVOID)NKAttachDebugger,              // 50 - ConnectDebugger (not supported for user mode)
    (PFNVOID)NKBinaryCompress,              // 51 - Binary compress
    (PFNVOID)NKBinaryDecompress,            // 52 - Binary de-compress
    (PFNVOID)NKDecompressBinaryBlock,       // 53 - decompress binary block
    (PFNVOID)NKStringCompress,              // 54 - StringCompress
    (PFNVOID)NKStringDecompress,            // 55 - StringDecompress
    (PFNVOID)NKGetRandomSeed,               // 56 - CeGetRandomSeed
    (PFNVOID)NKSleepTillTick,               // 57 - SleepTillTick
    (PFNVOID)NKQueryPerformanceFrequency,   // 58 - QueryPerformanceFrequency
    (PFNVOID)NKQueryPerformanceCounter,     // 59 - QueryPerformanceCounter
    (PFNVOID)THRDGetTimes,                  // 60 - GetThreadTimes
    (PFNVOID)APISQueryID,                   // 61 - QueryAPISetID
    (PFNVOID)NKIsNamedEventSignaled,        // 62 - IsNamedEventSignaled
    (PFNVOID)NKCreateLocaleView,            // 63 - CreateLocaleView
    (PFNVOID)NKRegisterDbgZones,            // 64 - RegisterDbgZones
    (PFNVOID)NKSetDbgZone,                  // 65 - NKSetDbgZones
    (PFNVOID)NKCreateMsgQueue,              // 66 - CreateMsgQueue
    (PFNVOID)OEMWriteDebugLED,              // 67 - WriteDebugLED
    (PFNVOID)NKIsProcessorFeaturePresent,   // 68 - IsProcessorFeaturePresent
    (PFNVOID)NKQueryInstructionSet,         // 69 - QueryInstructionSet
    (PFNVOID)NKSetKernelAlarm,              // 70 - SetKernelAlarm
    (PFNVOID)NKRefreshKernelAlarm,          // 71 - RefreshKernelAlarm
    (PFNVOID)NKSetOOMEventEx,               // 72 - SetOOMEventEx
    (PFNVOID)NKSetTime,                     // 73 - NKSetTime
    (PFNVOID)NKGetKPhys,                    // 74 - GetKPhys
    (PFNVOID)NKGiveKPhys,                   // 75 - GiveKPhys
    (PFNVOID)InputDebugCharW,               // 76 - InputDebugCharW
    (PFNVOID)NKSetLowestScheduledPriority,  // 77 - SetLowestScheduledPriority
    (PFNVOID)NKUpdateNLSInfo,               // 78 - UpdateNLSInfoEx
    (PFNVOID)NKReadRegistryFromOEM,         // 79 - ReadRegistryFromOEM
    (PFNVOID)NKWriteRegistryToOEM,          // 80 - WriteRegistryToOEM
    (PFNVOID)NotSupported,                  // 81 - GetStdioPathW - not supported in kernel
    (PFNVOID)NotSupported,                  // 82 - SetStdioPathW - not supported in kernel
    (PFNVOID)NKSetHardwareWatch,            // 83 - SetHardwareWatch
    (PFNVOID)NKPrepareThreadExit,           // 84 - PrepareThreadExit
    (PFNVOID)INTRInitialize,                // 85 - InterruptInitialize
    (PFNVOID)INTRDisable,                   // 86 - InterruptDisable
    (PFNVOID)INTRDone,                      // 87 - InterruptDone
    (PFNVOID)INTRMask,                      // 88 - InterruptMask
    (PFNVOID)NKSetPowerHandler,             // 89 - SetPowerHandler
    (PFNVOID)NKPowerOffSystem,              // 90 - PowerOffSystem
    (PFNVOID)NKLoadIntChainHandler,         // 91 - LoadIntChainHandler
    (PFNVOID)NKFreeIntChainHandler,         // 92 - FreeIntChainHandler
    (PFNVOID)PROCGetCode,                   // 93 - GetExitCodeProcess
    (PFNVOID)THRDGetCode,                   // 94 - GetExitCodeThread
    (PFNVOID)NKCreateStaticMapping,         // 95 - CreateStaticMapping
    (PFNVOID)NKSetDaylightTime,             // 96 - SetDaylightTime
    (PFNVOID)NKSetTimeZoneBias,             // 97 - SetTimeZoneBias
    (PFNVOID)NKTHCreateSnapshot,            // 98 - THCreateSnapshot (kernel info only)
    (PFNVOID)NKRemoteLocalAlloc,            // 99 - RemoteLocalAlloc
    (PFNVOID)NKRemoteLocalReAlloc,          // 100 - RemoteLocalReAlloc
    (PFNVOID)NKRemoteLocalSize,             // 101 - RemoteLocalSize
    (PFNVOID)NKRemoteLocalFree,             // 102 - RemoteLocalFree
    (PFNVOID)NKPSLNotify,                   // 103 - NKPSLNotify
    (PFNVOID)NKNotifyMemoryLow,             // 104 - SystemMemoryLow
    (PFNVOID)NKLoadKernelLibrary,           // 105 - LoadKernelLibrary
    (PFNVOID)NKCeLogData,                   // 106 - CeLogData
    (PFNVOID)NKCeLogSetZones,               // 107 - CeLogSetZones
    (PFNVOID)NKCeLogGetZones,               // 108 - CeLogGetZones
    (PFNVOID)NKCeLogReSync,                 // 109 - CeLogReSync
    (PFNVOID)NKWaitForAPIReady,             // 110 - WaitForAPIReady
    (PFNVOID)NKSetRAMMode,                  // 111 - SetRAMMode
    (PFNVOID)NKSetStoreQueueBase,           // 112 - SetStoreQueueBase
    (PFNVOID)HNDLGetServerId,               // 113 - GetHandleServerId
    (PFNVOID)NotSupported,                  // 114 - Unsupported
    (PFNVOID)NotSupported,                  // 115 - Unsupported
    (PFNVOID)NKSetDirectCall,               // 116 - SetDirectCall (k-mode only)
    (PFNVOID)NKGetCallerVMProcessId,        // 117 - GetVMProcessId (k-mode only)
    (PFNVOID)NKProfileSyscall,              // 118 - ProfileSyscall
    (PFNVOID)NKDebugSetProcessKillOnExit,   // 119 - DebugSetProcessKillOnExit
    (PFNVOID)NKDebugActiveProcessStop,      // 120 - DebugActiveProcessStop        
    (PFNVOID)NKDebugActiveProcess,          // 121 - DebugActiveProcess        
    (PFNVOID)NKGetDirectCallerProcessId,    // 122 - GetCallerProcess
    (PFNVOID)PROCReadKernelPEHeader,        // 123 - ReadKernelPEHeader
    (PFNVOID)NKForcePageout,                // 124 - ForcePageout (k-mode only)
    (PFNVOID)NKDeleteStaticMapping,         // 125 - DeleteStaticMapping
    (PFNVOID)NKGetRawTime,                  // 126 - CeGetRawTime
    (PFNVOID)NKFileTimeToSystemTime,        // 127 - FileTimeToSystemTime
    (PFNVOID)NKSystemTimeToFileTime,        // 128 - SystemTimeToFileTime
    (PFNVOID)NKGetTimeAsFileTime,           // 129 - NKGetTimeAsFileTime
    (PFNVOID)NKGetCPUInfo,                  // 130 - NKGetCPUInfo
    (PFNVOID)NKCPUPowerFunc,                // 131 - NKCPUPowerFunc - not supported in user ode
    (PFNVOID)NotSupported,                  // 132 - Unsupported
    (PFNVOID)NotSupported,                  // 133 - Unsupported
    (PFNVOID)NotSupported,                  // 134 - Unsupported
    (PFNVOID)NotSupported,                  // 135 - Unsupported
    (PFNVOID)NotSupported,                  // 136 - Unsupported
    (PFNVOID)NotSupported,                  // 137 - Unsupported
    (PFNVOID)NotSupported,                  // 138 - Unsupported
    (PFNVOID)KrnResetStack,                 // 139 - CeResetStack
};

static const ULONGLONG win32Sigs [] = {
    0,                                          // 0 - SPECIAL case for WIN32 method, direct entry for handle based calls
    0,                                          // 1
    FNSIG4 (DW, DW, DW, DW),                    // 2 - CreateAPISet - PARAMETER VALIDATED IN FUNCTION
    FNSIG3 (I_WSTR, I_WSTR, IO_PDW),            // 3 - CreateProcess
    FNSIG3 (DW, DW, DW),                        // 4 - OpenProcess
    FNSIG6 (IO_PDW, DW, IO_PDW, DW, DW, IO_PDW),// 5 - CreateThread
    FNSIG3 (DW, DW, DW),                        // 6 - OpenThread
    FNSIG4 (IO_PDW, DW, DW, I_WSTR),            // 7 - CreateEvent
    FNSIG3 (DW, DW, I_WSTR),                    // 8 - OpenEvent
    FNSIG3 (IO_PDW, DW, I_WSTR),                // 9 - CreateMutex
    FNSIG3 (DW, DW, I_WSTR),                    // 10 - OpenMutex
    FNSIG4 (IO_PDW, DW, DW, I_WSTR),            // 11 - CreateSemaphore
    FNSIG3 (DW, DW, I_WSTR),                    // 12 - OpenSemaphore
    FNSIG1 (DW),                                // 13 - NKExitThread (kernel cleanup)
    FNSIG2 (DW, DW),                            // 14 - TlsCall
    FNSIG3 (DW, DW, DW),                        // 15 - IsBadPtr - PARAMETER VALIDATED IN FUNCTION
    FNSIG4 (O_PTR, DW, DW, IO_PDW),             // 16 - CeVirtualSharedAlloc
    FNSIG1 (IO_PDW),                            // 17 - InitializeCriticalSection
    FNSIG1 (DW),                                // 18 - Sleep
    FNSIG1 (DW),                                // 19 - SetLastError
    FNSIG0 (),                                  // 20 - GetLastError
    FNSIG3 (I_WSTR, DW, IO_PDW),                // 21 - LoadLibraryEx
    FNSIG1 (DW),                                // 22 - FreeLibrary
    FNSIG7 (IO_PDW, I_WSTR, DW, DW, DW, DW, DW),// 23 - CreateWatchDog
    FNSIG2 (IO_PDW, DW),                        // 24 - Unsupported
    FNSIG7 (DW, DW, DW, IO_PDW, DW, DW, DW),    // 25 - DuplicateHandle
    FNSIG5 (DW, IO_PDW, DW, DW, IO_PDW),        // 26 - WaitForMultipleObjects
    FNSIG1 (I_WSTR),                            // 27 - OutputDebugString
    FNSIG2 (I_WSTR, IO_PDW),                    // 28 - NKDvDbgPrintfW
    FNSIG4 (DW, DW, DW, IO_PDW),                // 29 - RaiseException
    0,                                          // 30 - PerformCallBack
    FNSIG0 (),                                  // 31 - GetTickCount
    FNSIG1 (DW),                                // 32 - GetProcessVersion
    FNSIG5 (DW, DW, DW, O_PTR, DW),             // 33 - GetRomFileBytes
    FNSIG0 (),                                  // 34 - Unsupported
    FNSIG0 (),                                  // 35 - Unsupported
    FNSIG3 (DW, IO_PDW, DW),                    // 36 - GetRomFileInfo 
    FNSIG1 (IO_PDW),                            // 37 - GetSystemInfo
    FNSIG0 (),                                  // 38 - GetFSHeapInfo
    FNSIG6 (DW, I_PTR, DW, O_PTR, DW, IO_PDW),  // 39 - KernelIoControl
    FNSIG7 (DW, DW, I_PTR, DW, O_PTR, DW, IO_PDW),  // 40 - KernelLibIoControl
    FNSIG2 (O_PDW, DW),                         // 41 - NKGetTime
    FNSIG0 (),                                  // 42 - GetCallerProcessId
    FNSIG4 (DW, O_PTR, DW, IO_PDW),             // 43 - Unsupported
    FNSIG0 (),                                  // 44 - GetIdleTime
    FNSIG3 (O_PTR, DW, DW),                     // 45 - CacheRangeFlush
    FNSIG2 (DW, IO_PDW),                        // 46 - GetCallerResInfo (k-mode only, overload with u-mode only LoadExistingModule)
    FNSIG2 (IO_PDW, DW),                        // 47 - WaitForDebugEvent
    FNSIG3 (DW, DW, DW),                        // 48 - ContinueDebugEvent    
    FNSIG2 (DW, DW),                            // 49 - DebugNotify
    FNSIG1 (I_WSTR),                            // 50 - ConnectDebugger
    FNSIG4 (I_PTR, DW, O_PTR, DW),              // 51 - Binary compress
    FNSIG5 (I_PTR, DW, O_PTR, DW, DW),          // 52 - Binary de-compress
    FNSIG4 (I_PTR, DW, O_PTR, DW),              // 53 - decompress binary block
    FNSIG4 (I_PTR, DW, O_PTR, DW),              // 54 - StringCompress
    FNSIG4 (I_PTR, DW, O_PTR, DW),              // 55 - StringDecompress
    FNSIG0 (),                                  // 56 - CeGetRandomSeed
    FNSIG0 (),                                  // 57 - SleepTillTick
    FNSIG1 (IO_PI64),                           // 58 - QueryPerformanceFrequency
    FNSIG1 (IO_PI64),                           // 59 - QueryPerformanceCounter
    FNSIG5 (DW,IO_PI64,IO_PI64,IO_PI64,IO_PI64),// 60 - GetThreadTimes 
    FNSIG2 (I_ASTR, IO_PDW),                    // 61 - QueryAPISetID
    FNSIG2 (I_WSTR, DW),                        // 62 - IsNamedEventSignaled
    FNSIG1 (O_PDW),                             // 63 - CreateLocaleView
    FNSIG2 (DW, DW),                            // 64 - RegisterDbgZones - PARAMETER VALIDATED IN FUNCTION
    FNSIG5 (DW, DW, DW, DW, IO_PDW),            // 65 - NKSetDbgZones
    FNSIG2 (I_WSTR, IO_PDW),                    // 66 - CreateMsgQueue
    FNSIG2 (DW, DW),                            // 67 - WriteDebugLED
    FNSIG1 (DW),                                // 68 - IsProcessorFeaturePresent
    FNSIG2 (DW, IO_PDW),                        // 69 - QueryInstructionSet
    FNSIG2 (DW, IO_PDW),                        // 70 - SetKernelAlarm
    FNSIG0 (),                                  // 71 - RefreshKernelAlarm
    FNSIG7 (DW,DW,DW,DW,DW, DW, DW),            // 72 - SetOOMEventEx
    FNSIG3 (I_PDW, DW, O_PDW),                  // 73 - NKSetTime
    FNSIG2 (O_PTR, DW),                         // 74 - GetKPhys
    FNSIG2 (I_PTR, DW),                         // 75 - GiveKPhys
    FNSIG0 (),                                  // 76 - InputDebugCharW
    FNSIG1 (DW),                                // 77 - SetLowestScheduledPriority
    FNSIG4 (DW,DW,DW,DW),                       // 78 - UpdateNLSInfoEx
    FNSIG3 (DW, O_PTR, DW),                     // 79 - ReadRegistryFromOEM
    FNSIG3 (DW, I_PTR, DW),                     // 80 - WriteRegistryToOEM
    FNSIG3 (DW, DW, IO_PDW),                    // 81 - GetStdioPathW - PARAMETER VALIDATED IN FUNCTION
    FNSIG2 (DW, I_WSTR),                        // 82 - SetStdioPathW
    FNSIG2 (DW, DW),                            // 83 - SetHardwareWatch - PARAMETER VALIDATED IN FUNCTION
    FNSIG1 (DW),                                // 84 - PrepareThreadExit
    FNSIG4 (DW, DW, I_PTR, DW),                 // 85 - InterruptInitialize
    FNSIG1 (DW),                                // 86 - InterruptDisable
    FNSIG1 (DW),                                // 87 - InterruptDone
    FNSIG2 (DW, DW),                            // 88 - InterruptMask
    FNSIG2 (DW, DW),                            // 89 - SetPowerHandler
    FNSIG0 (),                                  // 90 - PowerOffSystem
    FNSIG3 (I_WSTR, I_WSTR, DW),                // 91 - LoadIntChainHandler
    FNSIG1 (DW),                                // 92 - FreeIntChainHandler
    FNSIG2 (DW, IO_PDW),                        // 93 - GetExitCodeProcess
    FNSIG2 (DW, IO_PDW),                        // 94 - GetExitCodeThread
    FNSIG2 (DW, DW),                            // 95 - CreateStaticMapping - kcoredll only API
    FNSIG2 (DW, DW),                            // 96 - SetDaylightTime
    FNSIG2 (DW, DW),                            // 97 - SetTimeZoneBias
    FNSIG2 (DW, DW),                            // 98 - THCreateSnapshot (kernel info only)
    FNSIG2 (DW, DW),                            // 99 - RemoteLocalAlloc
    FNSIG3 (DW, DW, DW),                        // 100 - RemoteLocalReAlloc
    FNSIG1 (DW),                                // 101 - RemoteLocalSize
    FNSIG1 (DW),                                // 102 - RemoteLocalFree
    FNSIG3 (DW, DW, DW),                        // 103 - NKPSLNotify
    FNSIG0 (),                                  // 104 - SystemMemoryLow
    FNSIG1 (I_WSTR),                            // 105 - LoadKernelLibrary
    FNSIG8 (DW, DW, I_PTR, DW, DW, DW, DW, DW), // 106 - CeLogData
    FNSIG3 (DW, DW, DW),                        // 107 - CeLogSetZones
    FNSIG4 (O_PDW, O_PDW, O_PDW, O_PDW),        // 108 - CeLogGetZones
    FNSIG0 (),                                  // 109 - CeLogReSync
    FNSIG2 (DW, DW),                            // 110 - WaitForAPIReady
    FNSIG3 (DW, IO_PDW, IO_PDW),                // 111 - SetRAMMode (k-mode only)
    FNSIG1 (DW),                                // 112 - SetStoreQueueBase
    FNSIG1 (DW),                                // 113 - GetHandleServerId
    FNSIG4 (I_PTR, DW, DW, DW),                 // 114 - Unsupported
    FNSIG3 (DW, I_PTR, DW),                     // 115 - Unsupported
    FNSIG0 (),                                  // 116 - SetDirectCall
    FNSIG0 (),                                  // 117 - GetVMProcessId
    FNSIG2 (I_PTR, DW),                         // 118 - ProfileSyscall
    FNSIG1 (DW),                                // 119 - DebugSetProcessKillOnExit    
    FNSIG1 (DW),                                // 120 - DebugActiveProcessStop        
    FNSIG1 (DW),                                // 121 - DebugActiveProcess
    FNSIG0 (),                                  // 122 - GetCallerProcess (old behavior)
    FNSIG4 (DW, DW, IO_PDW, DW),                // 123 - ReadKernelPEHeader
    FNSIG0 (),                                  // 124 - ForcePageout
    FNSIG2 (DW, DW),                            // 125 - DeleteStaticMapping (parameter checked in function)
    FNSIG1 (IO_PDW),                            // 126 - CeGetRawTime
    FNSIG2 (I_PDW, O_PDW),                      // 127 - FileTimeToSystemTime
    FNSIG2 (I_PDW, O_PDW),                      // 128 - SystemTimeToFileTime
    FNSIG2 (O_PDW, DW),                         // 129 - NKGetTimeAsFileTime
    FNSIG2 (DW, DW),                            // 130 - NKGetCPUInfo
    FNSIG3 (DW, DW, DW),                        // 131 - NKCPUPowerFunc - not supported in user ode
    FNSIG4 (I_PTR, DW, DW, IO_PDW),             // 132 - Unsupported
    FNSIG3 (DW, O_PTR, DW),                     // 133 - Unsupported
    FNSIG5 (DW, DW, O_PTR, DW, IO_PDW),         // 134 - Unsupported
    FNSIG3 (I_PTR, DW, IO_PDW),                 // 135 - Unsupported
    FNSIG3 (O_PDW, O_PDW, O_PDW),               // 136 - Unsupported
    FNSIG1 (DW),                                // 137 - Unsupported
    FNSIG0 (),                                  // 138 - Unsupported
    FNSIG0 (),                                  // 139 - CeResetStack
};

ERRFALSE(sizeof (Win32ExtMethods) == sizeof (Win32IntMethods));
ERRFALSE(sizeof(Win32ExtMethods)/sizeof(Win32ExtMethods[0]) == sizeof(win32Sigs)/sizeof(win32Sigs[0]));

const CINFO cinfWin32 = {
    {'W', 'n', '3', '2'},
    DISPATCH_I_KPSL,
    0,
    sizeof(Win32ExtMethods)/sizeof(Win32ExtMethods[0]),
    Win32ExtMethods,
    Win32IntMethods,
    win32Sigs,
    0,
    0,
    0,
};

static FARPROC pfnGwesPwrFunc, pfnDevmgrPwrFunc;

BOOL CeSafeCopyMemory (LPVOID pDst, LPCVOID pSrc, DWORD cbSize)
{
    __try {
        memcpy (pDst, pSrc, cbSize);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        cbSize = 0;
    }
    return cbSize;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
ReturnTrue() 
{
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
NotSupported() 
{
    if (pCurThread) {
        KSetLastError(pCurThread,ERROR_NOT_SUPPORTED);
    }
    return FALSE;
}

extern void SetCPUHardwareWatch(LPVOID,DWORD);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
NKSetHardwareWatch(
    LPVOID vAddr,
    DWORD flags
    )
{

    TRUSTED_API ("NKSetHardwareWatch", FALSE);
    
#ifdef MIPS

    SetCPUHardwareWatch(GetKAddr (vAddr), flags);
    return TRUE;

#elif x86
    // Make sure this runs without preemption
    SetCPUHardwareWatch(vAddr, flags);
    return TRUE;
#else
    return FALSE;
#endif
}

void OEMInitClock(void);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
NKSetPowerHandler(
    FARPROC pfn,
    DWORD dwHndlerType
    )
{
    // supported only from kernel mode
    BOOL fRet = IsKModeAddr ((DWORD)pfn);

    if (fRet) {
        switch (dwHndlerType) {
        case PHNDLR_GWES:
            pfnGwesPwrFunc = pfn;
            break;
        case PHNDLR_DEVMGR:
            pfnDevmgrPwrFunc = pfn;
            break;
        default:
            fRet = FALSE;
        }
    }
    return fRet;
}


//------------------------------------------------------------------------------
//
//  Invoke power on/off handler in the registered process space. Note that the
//  current thread is used to set the access key and process address space information
//  but that the thread is not actually scheduled in this state. Since a reschedule
//  will always occur after a power off, it is not necessary to call SetCPUASID to
//  restore the address space information when we are done.
//
//------------------------------------------------------------------------------
static void CallPowerProc(
        FARPROC     pfn,
        BOOL        bOff,
        LPCWSTR     pszMsg
    )
{
    DEBUGCHK (pActvProc == g_pprcNK);
    
    if (pfn) {
        RETAILMSG(1, (pszMsg));

        (*pfn)(bOff);       /* set power off state */
    }
}

void CallOEMPowerOff (void)
{
    INTERRUPTS_OFF ();
#if SHx
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD | CACHE_SYNC_INSTRUCTIONS);
#endif
    OEMPowerOff ();
#if SHx
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD | CACHE_SYNC_INSTRUCTIONS);
#endif
    OEMInitClock ();
    INTERRUPTS_ON ();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void NKPowerOffSystem (void) 
{
    extern PHDATA phdEvtPwrHndlr;
    PFN_WriteDebugString pfnWriteDebugString;
    PTHREAD pCurTh = pCurThread;
    DWORD  bPrio = GET_BPRIO(pCurTh);
    DWORD  dwQuantum = pCurTh->dwQuantum;

    TRUSTED_API_VOID (L"NKPowerOffSystem");
    DEBUGCHK (pActvProc == g_pprcNK);

    THRDSetPrio256 (pCurTh, 0);
    THRDSetQuantum (pCurTh, 0);

    // this retail message is required to make sure no other thread holds the 
    // CS for debug transport. 
    // NOTE: must be done after we change the priority and before setting 
    //       PowerOffFlag
    RETAILMSG(1, (TEXT("Powering Off system:\r\n")));

    g_pKData->bPowerOff = TRUE;

    EVNTModify (GetEventPtr (phdEvtPwrHndlr), EVENT_SET);

    /* Tell GWES and FileSys that we are turning power off */
    CallPowerProc (pfnGwesPwrFunc, TRUE, TEXT("  Calling GWES power proc.\r\n"));
    CallPowerProc (pfnDevmgrPwrFunc, TRUE, TEXT("  Calling device manager power proc.\r\n"));

    if (eSnapPass1 == g_snapState) {
        NKPrepareForSnapshot ();
        
    } else if (eSnapPass2 == g_snapState) {
        NKMakeSnapshot ();
        
    } else {

        RETAILMSG(1, (TEXT("  Calling OEMPowerOff...\r\n")));

        // re-direct debug output to oem debug output
        pfnWriteDebugString = g_pNKGlobal->pfnWriteDebugString;
        g_pNKGlobal->pfnWriteDebugString = g_pOemGlobal->pfnWriteDebugString;
        
        KCall ((PKFN) CallOEMPowerOff);
        
        // restore debug output to the saved function pointer
        g_pNKGlobal->pfnWriteDebugString = pfnWriteDebugString;
        
        // refresh soft RTC
        NKRefreshRTC ();

        RETAILMSG(1, (TEXT("Back from OEMPowerOff\r\n")));
    }

    /* Tell GWES and Device manager that we are turning power back on */
    CallPowerProc (pfnDevmgrPwrFunc, FALSE, TEXT("  Calling device manager power proc.\r\n"));
    CallPowerProc (pfnGwesPwrFunc, FALSE, TEXT("  Calling GWES power proc.\r\n"));

    RETAILMSG(1, (TEXT("  Returning to normally scheduled programming.\r\n")));

    // If platform supports debug Ethernet, turn interrupts back on
    CallKitlIoctl (IOCTL_KITL_INTRINIT, NULL, 0, NULL, 0, NULL);

    g_pKData->bPowerOff = FALSE;

    if (eSnapTaken == g_snapState) {
        RETAILMSG (1, (L"Verifying Snapshot...\r\n"));
        if (VerifySnap ()) {
            if (g_pOemGlobal->pSnapshotSupport->pfnSnapRead) {
                RETAILMSG (1, (L"Snapshot verification successful.\r\n"));
            } else {
                RETAILMSG (1, (L"Snapshot not verified as read function isn't provided\r\n"));
            }
            
        } else {
            ERRORMSG (1, (L"!!!Snapshot verification failed, snapshot is invalid!!!\r\n"));
        }
    }

    THRDSetPrio256 (pCurTh, bPrio);
    THRDSetQuantum (pCurTh, dwQuantum);

}

BOOL CallKitlIoctl (DWORD dwCode, LPVOID pInBuf, DWORD nInSize, LPVOID pOutBuf, DWORD nOutSize, LPDWORD pcbRet)
{
    BOOL fRet = FALSE;
    if (g_pNKGlobal->pfnKITLIoctl) {
        PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
        fRet = g_pNKGlobal->pfnKITLIoctl (dwCode, pInBuf, nInSize, pOutBuf, nOutSize, pcbRet);
        SwitchActiveProcess (pprc);
    }
    return fRet;
}

__inline DWORD VersionOf (const e32_lite *eptr)
{
    return MAKELONG (eptr->e32_ceverminor, eptr->e32_cevermajor);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD NKGetProcessVersion (DWORD dwId)
{
    PTHREAD  pth  = pCurThread;
    DWORD    dwVersion = 0;
    
    if (dwId == pth->pprcOwner->dwId) {
        // owner process
        dwVersion = VersionOf (&pth->pprcOwner->e32);
        
    } else if ((dwId == dwActvProcId) ||!dwId || (dwId == (DWORD)GetCurrentProcess ())) {
        // current process
        dwVersion = VersionOf (&pActvProc->e32);
        
    } else if (dwId & 3) {
        // process id/handle
        PPROCESS pprcHandleOwner = (dwId & 1)? pActvProc : g_pprcNK;
        PHDATA   phd  = LockHandleData ((HANDLE) dwId, pprcHandleOwner);
        PPROCESS pprc = GetProcPtr (phd);

        if (pprc) {
            dwVersion = VersionOf (&pprc->e32);
        }
        UnlockHandleData (phd);

    } else {
        // module
        PMODULE pMod = (PMODULE) dwId;
        LockModuleList ();
        if (FindModInProc (pActvProc, pMod)) {
            dwVersion = VersionOf (&pMod->e32);
        }
        UnlockModuleList ();
    }

    return dwVersion;
}

void UpdateLogPtrForSoftReset (void)
{
    if (g_pOemGlobal->dwPlatformFlags & OAL_ENABLE_SOFT_RESET) {
        // platform supports soft reset
        NKCacheRangeFlush (&LogPtr->magic1, sizeof (DWORD), CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD);
        LogPtr->magic1 = LOG_MAGIC;
        NKCacheRangeFlush (&LogPtr->magic1, sizeof (DWORD), CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD);
    }

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

BOOL NKReboot (BOOL fClean)
{
    // extract the power manager api to call; call into PM only once per device reboot session.
    PFN_SetSystemPowerState pfnSuspendFn = (PFN_SetSystemPowerState) InterlockedExchange ((LONG*)&g_pfnSetSystemPowerState, 0);

    if (fClean) {
        NKForceCleanBoot ();

    } else {
        UpdateLogPtrForSoftReset ();
    }

    // route the reboot call through power manager if power manager is part of the image
    if (pfnSuspendFn) {
        (*pfnSuspendFn) (NULL, POWER_STATE_RESET, POWER_FORCE);
        // this should not return; if it returns call the OAL directly.
    }
    
    //
    // Tell filesys to flush unsaved data
    //
    FSNotifyMountedFS (FSNOTIFY_POWER_OFF);
    
    OEMIoControl (IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL);
    NKSleep (10000);        // Allow time for hardware to reset        

    // if we ever return, something wrong (reboot not successful)
    DEBUGCHK (0);
    return FALSE;
}

// kernelIoctl: route it through KITL before calling OEMIoControl
BOOL KernelIoctl (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned)
{    
    BOOL fRet;
    DWORD dwPrevErr = NKGetLastError(); // cache previous error
    
    NKSetLastError (0); // clear last error

    fRet = CallKitlIoctl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        
    if (!fRet && (ERROR_NOT_SUPPORTED == NKGetLastError ())) {
        fRet = CallOEMIoControl(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);                
    }

    if (fRet) {
        NKSetLastError(dwPrevErr); // restore last error
    }

    return fRet;
}

