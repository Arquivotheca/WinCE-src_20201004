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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/**     TITLE("Kernel Globals")
 *++
 *
 *
 * Module Name:
 *
 *    nkglob.c
 *
 * Abstract:
 *
 *  This file contains all the kernel globals
 *
 *--
 */
#include <kernel.h>
#include <bldver.h>
#include <nkexport.h>

#undef INTERRUPTS_OFF
#undef INTERRUPTS_ON

#ifdef ARM
#define HookInterrupt       ((BOOL (*) (int, FARPROC))NotSupported)
#define UnhookInterrupt     ((BOOL (*) (int, FARPROC))NotSupported)
#endif

struct KDataStruct  *g_pKData;
const ROMHDR        *pTOC;

static PROCESS PrcNK;

PPROCESS g_pprcNK = &PrcNK;
PHNDLTABLE g_phndtblNK;
PPAGEDIRECTORY g_ppdirNK;

#define DEFAULT_MAX_THREAD_PER_PROC     0xfffe


BOOL ReturnFalse (void)
{
    return FALSE;
}

DWORD ReturnNegOne (void)
{
    return (DWORD) -1;
}

DWORD ReturnSelf (DWORD id)
{
    return id;
}

static BOOL VirtualSetAttrib (LPVOID lpvAddress, DWORD cbSize, DWORD dwNewFlags, DWORD dwMask, LPDWORD lpdwOldFlags)
{
    return VMSetAttributes (pVMProc, lpvAddress, cbSize, dwNewFlags, dwMask, lpdwOldFlags);
}

static LPVOID NKPhysToVirt (DWORD dwShiftedPhysAddr, BOOL fCached)
{
    LPVOID lpRet = NULL;
    // physical address must be page aligned
    if (dwShiftedPhysAddr & ((VM_PAGE_SIZE >> 8) - 1)) {
        RETAILMSG (1, (L"ERROR! NKPhysToVirt - physical addresss 0x%8.8lx is not page aligned\r\n", dwShiftedPhysAddr << 8));
    } else {
        DWORD dwPfn = PFNfrom256 (dwShiftedPhysAddr);
        lpRet = fCached
                    ? Pfn2Virt (dwPfn)
                    : Pfn2VirtUC (dwPfn);
    }

    return lpRet;
}

static DWORD NKVirtToPhys (LPCVOID pVirtAddr)
{
    DWORD dwPhysAddr = INVALID_PHYSICAL_ADDRESS;
    
    // only support statically mapped kernel addresses, must be page aligned
    DEBUGCHK(IsPageAligned((DWORD) pVirtAddr));
    if (IsKernelVa (pVirtAddr)) {
        dwPhysAddr = GetPFN (pVirtAddr);
    }
    if (INVALID_PHYSICAL_ADDRESS != dwPhysAddr) {
        dwPhysAddr = PA256FromPfn (dwPhysAddr);
    } else {
        RETAILMSG (1, (L"ERROR! NKVirtToPhys - virtual addresss 0x%8.8lx is not in kernel static map region\r\n", pVirtAddr));
    }

    return dwPhysAddr;
}




int ProfileInterrupt(void);
DWORD GetEPC (void);

void __report_gsfailure(void);

#ifdef x86
BOOL
NKwrmsr(
    DWORD dwAddr,       // Address of MSR being written
    DWORD dwValHigh,    // Upper 32 bits of value being written
    DWORD dwValLow      // Lower 32 bits of value being written
    );
BOOL
NKrdmsr(
    DWORD dwAddr,       // Address of MSR being read
    DWORD *lpdwValHigh, // Receives upper 32 bits of value, can be NULL
    DWORD *lpdwValLow   // Receives lower 32 bits of value, can be NULL
    );
#endif

#ifdef ARM
extern const DWORD VectorTable [];
#endif

static NKGLOBAL  nkglobal = {
    MAKELONG (CE_MINOR_VER, CE_MAJOR_VER),      // DWORD dwVersion;

    // function to output debug sting
    (PFN_WriteDebugString) ReturnFalse,         // PFN_WriteDebugString pfnWriteDebugString;
    NKvDbgPrintfW,                              // pfnNKvDbgPrintfW
    NKwvsprintfW,                               // pfnNKwvsprintfW

    // last error
    NKSetLastError,                             // pfnSetLastError
    NKGetLastError,                             // pfnGetLastError

    // CS related
    InitializeCriticalSection,                  // pfnInitializeCS
    DeleteCriticalSection,                      // pfnDeleteCS
    EnterCriticalSection,                       // pfnEnterCS
    LeaveCriticalSection,                       // pfnLeaveCS

    // interrupt related
    INTERRUPTS_OFF,                             // pfnINT_OFF
    INTERRUPTS_ON,                              // pfnINT_ON
    INTERRUPTS_ENABLE,                          // pfnINT_ENABLE
    HookInterrupt,                              // pfnHookInterrupt
    UnhookInterrupt,                            // pfnUnhookInterrupt
    NKCallIntChain,                             // pfnNKCallIntChain
    NKIsSysIntrValid,                           // pfnNKIsSysIntrValid
    NKSetInterruptEvent,                        // pfnNKSetInterruptEvent

    // Sleep funciton
    NKSleep,                                    // pfnSleep

    // VM related
    VirtualSetAttrib,                           // pfnVMSetAttrib
    NKCreateStaticMapping,                      // pfnCreateStaticMapping
    NKDeleteStaticMapping,                      // pfnDeleteStaticMapping

    // registry related
    NKRegCreateKeyExW,                          // pfnRegCreateKeyExW
    NKRegOpenKeyExW,                            // pfnRegOpenKeyExW
    NKRegCloseKey,                              // pfnRegCloseKey
    NKRegQueryValueExW,                         // pfnRegQueryValueExW
    NKRegSetValueExW,                           // pfnRegSetValueExW
    NKRegFlushKey,                              // pfnRegFlushKey
    

    // reboot related
    NKForceCleanBoot,                           // pfnNKForceCleanBoot
    NKReboot,                                   // pfnNKReboot

    // address translation
    NKPhysToVirt,                               // pfnPhysToVirt
    NKVirtToPhys,                               // pfnVirtToPhys

    // kitl Ioctl - the only entry point to kitl
    (PFN_Ioctl) NotSupported,                    // pfnKITLIoctl;

    // utility functions
    NKwcslen,                                   // pfnNKwcslen)

    NKwcscmp,                                   // pfnNKwcscmp
    NKwcsicmp,                                  // pfnNKwcsicmp
    NKwcsnicmp,                                 // pfnNKwcsnicmp

    NKwcscpy,                                   // pfnNKwcscpy
    NKwcsncpy,                                  // pfnNKwcsncpy

    NKstrcmpiAandW,                             // pfnNKstrcmpiAandW

    NKUnicodeToAscii,                           // pfnNKUnicodeToAscii
    NKAsciiToUnicode,                           // pfnNKKAsciiToUnicode

    NKSystemTimeToFileTime,                     // pfnKSystemTimeToFileTime
    NKFileTimeToSystemTime,                     // pfnKFileTimeToSystemTime
    (BOOL (*) (const FILETIME*, const FILETIME*)) ReturnFalse,  // pfnKCompareFileTime

    // profiler
#ifdef NKPROF    
    ProfilerHitEx,                              // pfnProfilerHitEx
    GetEPC,                                     // pfnGetEPC
#else
    (void (*) (OEMProfilerData*)) ReturnFalse,
    ReturnFalse,
#endif

    // compiler /GS runtime
    &__report_gsfailure,                        // pfn__report_gsfailure

    // data
    0,                                          // dwStartupAddr;
    0,                                          // dwNextReschedTime;
    0,                                          // dwCurMSec;
    {0, 0},                                     // liIdle;
    0,                                          // dwIdleConv;
    DEFAULT_MAX_THREAD_PER_PROC,                // dwMaxThreadPerProc

    0,                                          // DWORD dwProcessorType;
    0,                                          // WORD  wProcessorLevel;
    0,                                          // WORD  wProcessorRevision;
    0,                                          // DWORD dwInstructionSet;

    NULL,                                       // DBGPARAM *pKITLDbgZone;

#ifdef x86
    // cpu dependent part
    NKwrmsr,                                    // pfnX86wrmsr
    NKrdmsr,                                    // pfnX86rdmsr
#endif

#ifdef SH4
    NULL,                                       // IntrPrio
    NULL,                                       // InterruptTable
#endif

#ifdef ARM
    VectorTable,                                // pdwOrigVectors;
    NULL,                                       // pdwCurrVectors;
#endif

};

PNKGLOBAL  g_pNKGlobal  = &nkglobal;
