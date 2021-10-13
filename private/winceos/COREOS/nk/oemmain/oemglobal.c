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
/*
 *
 *
 * Module Name:
 *
 *              oemglb.c
 *
 * Abstract:
 *
 *              This file define the OEM globals.
 *
 */

#include "kernel.h"
#include <bldver.h>


// Following variable can be updated through fixupvar. Default value is 100ms.
// nk.exe:DefaultThreadQuantum     00000000 0x00000064     FIXUPVAR
DWORD DefaultThreadQuantum = DEFAULT_THREAD_QUANTUM;

BOOL ReturnFalse (void)
{
    return FALSE;
}

DWORD ReturnSelf (DWORD id)
{
    return id;
}

static DWORD FakeCalcPage (DWORD dwTotal, DWORD dwDefault)
{
    return dwDefault;
}

extern void NKInitInterlockedFuncs(void);

void DefaultInitInterlockedFuncs(void)
{
    NKInitInterlockedFuncs();
}

// compiler /GS runtime
extern DWORD_PTR __security_cookie;
extern DWORD_PTR __security_cookie_complement;

extern POEMGLOBAL g_pOemGlobal;
extern PNKGLOBAL  g_pNKGlobal;

NKPagePoolParameters pagePoolParams;

#ifdef KITLOEM
extern BOOL WINAPI KitlDllMain (HINSTANCE DllInstance, DWORD dwReason, LPVOID Reserved);
#endif

static OEMGLOBAL OemGlobal = {
    MAKELONG (CE_MINOR_VER, CE_MAJOR_VER),  // DWORD                   dwVersion;

    // initialization
    OEMInitDebugSerial,                     // PFN_InitDebugSerial     pfnInitDebugSerial;
    OEMInit,                                // PFN_InitPlatform        pfnInitPlatform;

    // debug functions
    OEMWriteDebugByte,                      // PFN_WriteDebugByte      pfnWriteDebugByte;
    (PFN_WriteDebugString)OEMWriteDebugString,  // PFN_WriteDebugString    pfnWriteDebugString;
    OEMReadDebugByte,                       // PFN_ReadDebugByte       pfnReadDebugByte;
    (PFN_WriteDebugLED)ReturnFalse,         // PFN_WriteDebugLED       pfnWriteDebugLED;

    // cache fuctions
    OEMCacheRangeFlush,                     // PFN_CacheRangeFlush     pfnCacheRangeFlush;

    // time related funcitons
    (PFN_InitClock)ReturnFalse,             // PFN_InitClock           pfnInitClock;
    OEMGetRealTime,                         // PFN_GetRealTime         pfnGetRealTime;
    (PFN_SetRealTime)OEMSetRealTime,        // PFN_SetRealTime         pfnSetRealTime;
    (PFN_SetAlarmTime)OEMSetAlarmTime,      // FPN_SetAlarmTime        pfnSetAlarmTime;
    NULL,                                   // PFN_QueryPerfCounter    pfnQueryPerfCounter;
    NULL,                                   // PFN_QueryPerfFreq       pfnQueryPerfFreq;
    OEMGetTickCount,                        // PFN_GetTickCount        pfnGetTickCount;

    // scheduler related functions
    OEMIdle,                                // PFN_Idle                pfnIdle;
    (PFN_NotifyThreadExit)ReturnFalse,      // PFN_NotifyThreadExit    pfnNotifyThreadExit;
    (PFN_NotifyReschedule)ReturnFalse,      // PFN_NotifyReschedule    pfnNotifyReschedule;
    ReturnSelf,                             // PFN_NotifyIntrOccurs    pfnNotifyIntrOccurs;
    NULL,                                   // PFN_UpdateReschedTime   pfnUpdateReschedTime;
    DEFAULT_THREAD_QUANTUM,                 // dwDefaultTheadQuantum

    // power related functions
    OEMPowerOff,                            // PFN_PowerOff            pfnPowerOff;

    // DRAM detection functions
    (PFN_GetOEMRamTable) ReturnFalse,       // PFN_GetOEMRamTable      pfnGetOEMRamTable;
    OEMGetExtensionDRAM,                    // PFN_GetExtensionDRAM    pfnGetExtensionDRAM;
    NULL,                                   // PFN_EnumExtensionDRAM   pfnEnumExtensionDRAM;
    FakeCalcPage,                           // PFN_CalcFSPages         pfnCalcFSPages;
    0,                                      // DWORD                   dwMainMemoryEndAddress;

    // interrupt handling functions
    OEMInterruptEnable,                     // PFN_InterruptEnable     pfnInterruptEnable;
    OEMInterruptDisable,                    // PFN_InterruptDisable    pfnInterruptDisable;
    OEMInterruptDone,                       // PFN_InterruptDone       pfnInterruptDone;
    OEMInterruptMask,                       // PFN_InterruptMask       pfnInterruptMask;

    // co-proc support
    (PFN_InitCoProcRegs)ReturnFalse,        // PFN_InitCoProcRegs      pfnInitCoProcRegs;
    (PFN_SaveCoProcRegs)ReturnFalse,        // PFN_SaveCoProcRegs      pfnSaveCoProcRegs;
    (PFN_RestoreCoProcRegs)ReturnFalse,     // PFN_RestoreCoProcRegs   pfnRestoreCoProcRegs;
    0,                                      // DWORD                   cbCoProcRegSize;
    FALSE,                                  // DWORD                   fSaveCoProcReg;

    // persistent registry support
    NULL,                                   // PFN_ReadRegistry        pfnReadRegistry;
    NULL,                                   // PFN_WriteRegistry       pfnWriteRegistry;

    // watchdog support
    (PFN_RefreshWatchDog)ReturnFalse,       // PFN_RefreshWatchDog     pfnRefreshWatchDog;
    0,                                      // DWORD                   dwWatchDogPeriod;
    DEFAULT_WATCHDOG_PRIORITY,              // DWORD                   dwWatchDogThreadPriority;

    // profiler support
    NULL,                                   // PFN_ProfileTimerEnable  pfnProfileTimerEnable;
    NULL,                                   // PFN_ProfileTimerDisable pfnProfileTimerDisable;

    // Simple RAM based Error Reporting support
    0,                                      // DWORD                   cbErrReportSize;
    
    // others
    OEMIoControl,                           // PFN_Ioctl               pfnOEMIoctl;
    (PFN_KDIoctl)ReturnFalse,               // PFN_KDIoctl             pfnKDIoctl;
    (PFN_IsRom)ReturnFalse,                 // PFN_IsRom               pfnIsRom;
    NULL,                                   // PFN_MapW32Priority      pfnMapW32Priority;
    (PFN_SetMemoryAttributes)ReturnFalse,   // PFN_SetMemoryAttributes pfnSetMemoryAttributes;
    (PFN_IsProcessorFeaturePresent)ReturnFalse, // PFN_IsProcessorFeaturePresent   pfnIsProcessorFeaturePresent;
    (PFN_HaltSystem) ReturnFalse,           // PFN_HaltSystem          pfnHaltSystem;
    (PFN_NotifyForceCleanBoot) ReturnFalse, // PFN_NotifyForceCleanBoot pfnNotifyForceCleanBoot;
    
    NULL,                                   // PROMChain_t             pROMChain;
    
    // Platform specific information passed from OAL to KITL.
    NULL,                                   // LPVOID                  pKitlInfo
#ifdef KITLOEM
    KitlDllMain,                            // KITL entry point (KITL is part of OEM)        
#else
    NULL,                                   // KITL entry point (KITL is in a separate DLL)  
#endif

#ifdef DEBUG
    &dpCurSettings,                         // DBGPARAM                 *pdpCurSettings
#else
    NULL,
#endif

    &__security_cookie,                     // DWORD_PTR *             p__security_cookie
    &__security_cookie_complement,          // DWORD_PTR *             p__security_cookie_complement

    DEFAULT_ALARMRESOLUTION_MSEC,           // DWORD                   dwAlarmResolution; 
    0,                                      // DWORD                   dwYearsRTCRollover; 
    0,                                      // DWORD                   dwPlatformFlags

    &pagePoolParams,                        // struct _NKPagePoolParameters *pPagePoolParam;  

    PAGEOUT_LOW,                            // DWORD                   cpPageOutLow 
    PAGEOUT_HIGH,                           // DWORD                   cpPageOutHigh

    // snapshot support
    NULL,                                   // PCSnapshotSupport       pSnapshotSupport;

    {0},                                    // DWORD                   _reserved_[]

#if defined (x86)
    {0},                                    // DWORD                   _reserved_plat_[]
#elif defined (ARM)
    {0},                                    // DWORD                   _reserved_plat_[]
    
    NULL,                                   // PFN_PTEUpdateBarrier    pfnPTEUpdateBarrier;
    NULL,                                   // PFN_VFPCoprocControl    pfnVFPCoprocControl;
#elif defined (MIPS)
    {0},                                    // DWORD                   _reserved_plat_[]
#elif defined (SHx)
    {0},                                    // DWORD                   _reserved_plat_[]
#endif
    
    FALSE,                                  // BOOL                    fMPEnable;
    (PFN_MpStartAllCPUs)ReturnFalse,        // PFN_MpStartAllCPUs      pfnStartAllCpus;
    (PFN_MpPerCPUInit)ReturnFalse,          // PFN_MpPerCPUInit        pfnMpPerCPUInit;
    (PFN_MpCpuPowerFunc)ReturnFalse,        // PFN_MpCpuPowerFunc      pfnMpCpuPowerFunc;
    (PFN_IpiHandler)ReturnFalse,            // PFN_IpiHandler          pfnIpiHandler;
    (PFN_SendIPI)ReturnFalse,               // PFN_SendIPI             pfnSendIpi;
    NULL,                                   // PFN_OEMIdleEx           pfnIdleEx;
    (PFN_InitInterlockedFunc)DefaultInitInterlockedFuncs, // PFN_InitInterlockedFunc pfnInitInterlockedFuncs;
    (PFN_KdEnableTimer)ReturnFalse,         // PFN_KdEnableTimer       pfnKdEnableTimer;
    
#if defined(x86)
    // CPU dependent functions
    OEMNMIHandler,                          // PFN_NMIHandler          pfnNMIHandler;
    0x10000,                                // DWORD                   dwIpiVector;
#elif defined (ARM)
    OEMInterruptHandler,                    // PFN_InterruptHandler    pfnInterruptHandler;
    OEMInterruptHandlerFIQ,                 // PFN_InterruptHandler    pfnFIQHandler;
    
    0,                                      // DWORD                   dwARM1stLvlBits;
    0,                                      // DWORD                   dwARMCacheMode;
    FALSE,                                  // DWORD                   f_V6_VIVT_ICache;

    // VFP functions
    (PFN_SaveRestoreVFPCtrlRegs)ReturnFalse,    // PFN_SaveRestoreVFPCtrlRegs   pfnSaveVFPCtrlRegs;
    (PFN_SaveRestoreVFPCtrlRegs)ReturnFalse,    // PFN_SaveRestoreVFPCtrlRegs   pfnRestoreVFPCtrlRegs;
    (PFN_HandleVFPException)ReturnFalse,        // PFN_HandleVFPException       pfnHandleVFPExcp;
    (PFN_IsVFPFeaturePresent)ReturnFalse,       // PFN_IsVFPFeaturePresent      pfnIsVFPFeaturePresent;

    0,                                      // DWORD                   dwPageTableCacheBits;
    0,                                      // DWORD                   dwTTBRCacheBits;
#elif defined (MIPS)
    MIPS_DEFAULT_FPU_ENABLE,                // DWORD                   dwCoProcBits;
    0,                                      // DWORD                   dwOEMTLBLastIdx;
    MIPS_FLAG_NO_OVERRIDE,                  // DWORD                   dwArchFlagOverride;
    NULL,                                   // const BYTE *pIntrPrio;
    NULL,                                   // const BYTE *pIntrMask;
#elif defined (SHx)
    OEMNMI,                                 // PFN_SHxNMIHandler       pfnNMIHandler;
    SH4_INTEVT_LENGTH,                      // platform specific interrupt event code length
#endif
};

POEMGLOBAL g_pOemGlobal = &OemGlobal;

#ifdef MIPS
extern const DWORD dwOEMArchOverride;
extern DWORD OEMTLBSize;
extern BYTE IntrPriority [];
extern BYTE IntrMask [];
#endif

#ifdef SHx
extern const DWORD dwSHxIntEventCodeLength;
#endif

extern const volatile DWORD LoaderPoolTarget;
extern const volatile DWORD FilePoolTarget;
extern const volatile DWORD PageOutLow;
extern const volatile DWORD PageOutHigh;

POEMGLOBAL OEMInitGlobals (PNKGLOBAL pNKGlobal)
{
    g_pNKGlobal  = pNKGlobal;
    g_pOemGlobal->dwPlatformFlags = OAL_ENABLE_SOFT_RESET;

    // setup page pool parameters
    pagePoolParams.NKVersion  = CURRENT_PAGE_POOL_VERSION;
    pagePoolParams.OEMVersion = 0;

    pagePoolParams.Loader.Target                = LoaderPoolTarget;

    pagePoolParams.File.Target                  = FilePoolTarget;

    g_pOemGlobal->cpPageOutLow = PageOutLow;
    g_pOemGlobal->cpPageOutHigh = PageOutHigh;
    
#if defined (ARM)
    g_pOemGlobal->dwARMCacheMode = OEMARMCacheMode ();
#elif defined (MIPS)
    g_pOemGlobal->dwOEMTLBLastIdx = OEMTLBSize;   // OEMTLBSize should probably be renamed...
    g_pOemGlobal->pIntrPrio = IntrPriority;
    g_pOemGlobal->pIntrMask = IntrMask;
    g_pOemGlobal->dwArchFlagOverride = dwOEMArchOverride;
#elif defined (SHx)
    g_pOemGlobal->dwSHxIntEventCodeLength = dwSHxIntEventCodeLength;
#endif
    g_pOemGlobal->dwDefaultThreadQuantum = DefaultThreadQuantum;
    return g_pOemGlobal;
}

