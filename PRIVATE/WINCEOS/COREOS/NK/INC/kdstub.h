//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++

Module Name:

    Kdstub.h

Abstract:

    Public header for kdstub.  Pulling out of kernel.h

--*/

#pragma once
#ifndef _KDSTUB_H
#define _KDSTUB_H


#define KDSTUB_HDEVENTFILT_MASK (HDSTUB_FILTER_MODLOAD | HDSTUB_FILTER_MODUNLOAD)


typedef struct _SAVED_THREAD_STATE
{
    BOOL        fSaved;     // Set to TRUE when saved 
    BYTE        bCPrio;     // Thread Current priority 
    BYTE        bBPrio;     // Thread Base priority 
    DWORD       dwQuantum;  // Thread quantum 
    ACCESSKEY   aky;        // Thread access key 
} SAVED_THREAD_STATE;

typedef struct _KERNDATA {
    ULONG nSize;

    // OUT params
    BOOL (*pKdSanitize)(BYTE* pbClean, VOID* pvAddrMem, ULONG nSize, BOOL fAlwaysCopy);
    VOID (*pKdReboot)(BOOL fReboot);

    // IN  params
    ROMHDR* pTOC;
    ROMChain_t* pROMChain;
    PROCESS* pProcArray;
    HDATA* pHandleList;
    struct KDataStruct* pKData;
    CRITICAL_SECTION* pVAcs;
    SECTION* pNullSection;
    SECTION* pNKSection;
    HANDLE*  phCoreDll;
    BOOL  (* pKdCleanup)(VOID);
    VOID  (* pKDEnableInt)(BOOL, SAVED_THREAD_STATE *);
    PPVOID ppCaptureDumpFileOnDevice;
    BOOL  (* pfnIsDesktopDbgrExist)();
    int   (* pNKwvsprintfW)(LPWSTR, LPCWSTR, CONST VOID *, int);
    VOID  (WINAPIV* pNKDbgPrintfW)(LPCWSTR, ...);
    int   (* pKCall)(PKFN, ...);
    PVOID (* pDbgVerify)(VOID*, BOOL, BOOL*);   
    void  (* pInvalidateRange)(PVOID, ULONG);
    BOOL  (* pDoVirtualCopy)(LPVOID, LPVOID, DWORD, DWORD);  
    BOOL  (* pkdpIsROM) (LPVOID, DWORD);
    PFNVOID (* pDBG_CallCheck)(PTHREAD, DWORD, PCONTEXT);
    VOID  (* pMD_CBRtn)(VOID);
    VOID  (* pINTERRUPTS_OFF)(VOID);
    BOOL  (* pINTERRUPTS_ENABLE)(BOOL);
    PFN_OEMKDIoControl pKDIoControl;
    DWORD (* pKITLIoCtl)(DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
    VOID* (* pGetObjectPtrByType)(HANDLE, int);
    VOID (WINAPI* pInitializeCriticalSection)(LPCRITICAL_SECTION);
    VOID (WINAPI* pDeleteCriticalSection)(LPCRITICAL_SECTION);
    VOID (WINAPI* pEnterCriticalSection)(LPCRITICAL_SECTION);
    VOID (WINAPI* pLeaveCriticalSection)(LPCRITICAL_SECTION);
    BOOL (* pCloseHandle)(HANDLE);
    BOOL (* pVirtualFree) (LPVOID, DWORD, DWORD);
    BOOL fFPUPresent;
    BOOL fDSPPresent;
    BOOL* pfForcedPaging;
    void (* FlushCacheRange) (LPVOID, DWORD, DWORD);
    ULONG *pulHDEventFilter;
    void (*pfnHwTrap) (void);
#if defined(MIPS)
    LONG  (* pInterlockedDecrement)(LPLONG Target);
    LONG  (* pInterlockedIncrement)(LPLONG Target);
#endif
#if defined(ARM)
    int   (* pInSysCall)(void);
#endif
#if defined(x86)
    void (* FPUFlushContext)(void);
    PTHREAD* ppCurFPUOwner;
    DWORD *  pdwProcessorFeatures;
    BOOL  (* p__abnormal_termination)(VOID);
    EXCEPTION_DISPOSITION (__cdecl* p_except_handler3)(PEXCEPTION_RECORD, void*, PCONTEXT, PDISPATCHER_CONTEXT);
#else
    EXCEPTION_DISPOSITION (* p__C_specific_handler)(PEXCEPTION_RECORD, PVOID, PCONTEXT, PDISPATCHER_CONTEXT);
#endif
#if defined(MIPS_HAS_FPU) || defined(SH4) || defined(ARM)
    void (* FPUFlushContext)(void);
#endif
#if defined(SHx) && !defined(SH3e) && !defined(SH4)
    void (* DSPFlushContext)(void);         // SH3DSP specific
#endif
} KERNDATA;

#endif
