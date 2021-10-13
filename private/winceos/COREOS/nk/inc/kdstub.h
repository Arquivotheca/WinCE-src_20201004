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


typedef struct _STRING // Counted String
{
    USHORT Length;
    USHORT MaximumLength;
    // [size_is(MaximumLength), length_is(Length) ]
    PCHAR Buffer;
} STRING, *PSTRING;


// Callback to KdStub to send KDBG nested response in case of multi-packet response
// Returns TRUE if succesfull and processing can continue
//         FALSE if failed need to abort processing
typedef BOOL (*OSAXS_KDBG_RESPONSE_FUNC) (IN STRING * pResponseHeader, IN OPTIONAL STRING * pAdditionalData);


typedef struct _EXCEPTION_INFO
{
    EXCEPTION_POINTERS ExceptionPointers;
    BOOLEAN SecondChance;
} EXCEPTION_INFO;


typedef struct _KDSTUB_INIT {
    ULONG nSize;

    BOOL (*pfnINTERRUPTS_ENABLE)(BOOL);
    VOID (*pfnInitializeCriticalSection)(LPCRITICAL_SECTION);
    VOID (*pfnDeleteCriticalSection)(LPCRITICAL_SECTION);
    VOID (*pfnEnterCriticalSection)(LPCRITICAL_SECTION);
    VOID (*pfnLeaveCriticalSection)(LPCRITICAL_SECTION);
    
    // OUT params
    BOOL (*pKdSanitize)(BYTE* pbClean, VOID* pvAddrMem, ULONG nSize, BOOL fAlwaysCopy);
    VOID (*pKdReboot)(BOOL fReboot);
    BOOL (*pKdWasBreakpoint)(DWORD);

    // IN  params
    const ROMHDR* pTOC;
    ROMChain_t* pROMChain;
    PROCESS* pprcNK;
    struct KDataStruct* pKData;
    HANDLE*  phCoreDll;
    BOOL  (* pKdCleanup)(VOID);
    PPVOID ppCaptureDumpFileOnDevice;
    PPVOID ppKCaptureDumpFileOnDevice;
    BOOL  (* pfnIsDesktopDbgrExist)();
    int   (* pNKwvsprintfW)(LPWSTR, LPCWSTR, va_list, int);
    VOID  (WINAPIV* pNKDbgPrintfW)(LPCWSTR, ...);
    int   (* pKCall)(PKFN, ...);
    PVOID (* pDbgVerify)(PROCESS*, VOID*, BOOL, BOOL*);   
    void  (* pInvalidateRange)(PVOID, ULONG);
    BOOL  (* pkdpIsROM) (LPVOID, DWORD);
    PFNVOID (* pDBG_CallCheck)(PTHREAD, DWORD, PCONTEXT);
    PFN_OEMKDIoControl pKDIoControl;
    DWORD (* pKITLIoCtl)(DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
    BOOL fFPUPresent;
    BOOL fDSPPresent;
    BOOL* pfForcedPaging;
    void (* FlushCacheRange) (LPVOID, DWORD, DWORD);
    ULONG *pulHDEventFilter;
    void (*pfnHwTrap) (void);
    BOOL (*pfnIsProcessorFeaturePresent)(DWORD dwProcessorFeature);
    void * (* Pfn2Virt_) (unsigned long);
    PHDATA (*pfnHandleToHDATA)(HANDLE, PHNDLTABLE);
    void (*pfnInitializeSpinLock)(volatile struct _SPINLOCK *);
    void (*pfnAcquireSpinLock)(volatile struct _SPINLOCK *);
    void (*pfnReleaseSpinLock)(volatile struct _SPINLOCK *);
    void (*pfnStopAllOtherCPUs)(void);
    void (*pfnResumeAllOtherCPUs)(void);
    volatile SPINLOCK *pSchedLock;
    volatile SPINLOCK *pKitlSpinLock;
    volatile SPINLOCK *pPhysLock;
    volatile SPINLOCK *pOalLock;
    LPVOID (*pfnVMAlloc)(PPROCESS,LPVOID,DWORD,DWORD,DWORD);
    BOOL (*pfnVMFreeAndRelease)(PPROCESS,LPVOID,DWORD);
    DWORD (*pfnMakeWritableEntry)(PPROCESS, DWORD, DWORD);
    PPAGETABLE (*pfnGetPageTable) (PPAGEDIRECTORY, DWORD);

#if defined(MIPS)
    LONG  (* pInterlockedDecrement)(LPLONG Target);
    LONG  (* pInterlockedIncrement)(LPLONG Target);
#endif
#if defined(ARM)
    int   (* pInSysCall)(void);
#endif
#if defined(x86)
    BOOL  (* p__abnormal_termination)(VOID);
    EXCEPTION_DISPOSITION (__cdecl* p_except_handler3)(PEXCEPTION_RECORD, void*, PCONTEXT, PDISPATCHER_CONTEXT);
#else
    EXCEPTION_DISPOSITION (* p__C_specific_handler)(PEXCEPTION_RECORD, PVOID, PCONTEXT, PDISPATCHER_CONTEXT);
#endif
} KDSTUB_INIT;

#endif
