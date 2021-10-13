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

    Osaxs.h

Module Description:

    OsaxsT0, OsaxsT1 header / kernel interface

--*/

#pragma once
#ifndef _OSAXS_H
#define _OSAXS_H

#include "osaxs_common.h"

/* Official client names for osaccess stubs */
#define OSAXST0_NAME "OsAxsT0"
#define OSAXST1_NAME "OsAxsT1"

/* OsAccess IOCTL Numbers */
enum OSAXST0_IOCTL
{
    OSAXST0_IOCTL_GET_FLEXIPTMINFO       = 1,
    OSAXST0_IOCTL_SAVE_EXCEPTION_CONTEXT = 2,
};

enum OSAXST1_IOCTL
{
    OSAXST1_IOCTL_MANIPULATE_VM         = 1,
    OSAXST1_IOCTL_SAVE_EXCEPTION_CONTEXT = 2,
    OSAXST1_IOCTL_GET_OSSTRUCT          = 3,
    OSAXST1_IOCTL_GET_THREADCTX         = 4,
    OSAXST1_IOCTL_SET_THREADCTX         = 5,
    OSAXST1_IOCTL_TRANSLATE_RETURN      = 6,
    OSAXST1_IOCTL_GET_MODULE_O32_DATA   = 7,
    OSAXST1_IOCTL_GET_EXCEPTION_REGISTRATION = 8
};

enum OSAXST1_MANIP_VM
{
    OSAXST1_VM_GET = 0,
    OSAXST1_VM_SET = 1,
};


// RaiseException(STATUS_CRASH_DUMP, ...) is used to programmatically capture dump files.
// This macro makes sure this exception was raised by CaptureDumpFileOnDevice.
// Make sure the extent of the ExceptionAddress includes the whole functin for worst
// case condition (MIPSIV Debug), i.e. (DWORD)pCaptureDumpFileOnDevice + 0x200
#define CAPTUREDUMPFILEONDEVICE_CALLED(pExceptionRecord, pCaptureDumpFileOnDevice) \
    ((STATUS_CRASH_DUMP == pExceptionRecord->ExceptionCode) && \
     (5 == pExceptionRecord->NumberParameters) && \
     ((DWORD)pExceptionRecord->ExceptionInformation[4] == (DWORD)pCaptureDumpFileOnDevice) && \
     ((DWORD)pExceptionRecord->ExceptionAddress >= (DWORD)pCaptureDumpFileOnDevice) && \
     ((DWORD)pExceptionRecord->ExceptionAddress <= ((DWORD)pCaptureDumpFileOnDevice + 0x200)))


typedef struct _OSAXS_DATA
{
    /* in */
    DWORD cbSize;
    struct KDataStruct *pKData;
    CRITICAL_SECTION* pVAcs;
    SECTION* pNullSection;
    SECTION* pNKSection;
    HANDLE*  phCoreDll;
    PPVOID ppCaptureDumpFileOnDevice;
    const CINFO **pSystemAPISets;
    PPROCESS pProcArray;
    ROMHDR *pRomHdr;
    fslog_t *pLogPtr;
    PFN_OEMKDIoControl pfnOEMKDIoControl;
    BOOL (*pfnNKKernelLibIoControl) (HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
    VOID* (*pfnGetObjectPtrByType) (HANDLE, int);
    void (*pfnFlushCacheRange) (LPVOID, DWORD, DWORD);
    BOOL (*pfnIsRom) (LPVOID, DWORD);
    void *(*pfnDbgVerify) (void *, BOOL, BOOL *);
    BOOL (*pfnDoThreadGetContext) (HANDLE, LPCONTEXT);
    ULONG (*pfnNKGetThreadCallStack) (PTHREAD, ULONG, LPVOID, DWORD, DWORD, PCONTEXT);
    BOOL (WINAPI* pEventModify)(HANDLE, DWORD);
    VOID   (*pfnGetSystemInfo)(LPSYSTEM_INFO);
    DWORD  (*pfnGetLastError)(void);
    void   (*pfnSetLastError)(DWORD);
    BOOL   (*pfnINTERRUPTS_ENABLE)(BOOL);
#if defined(MIPS)
    LONG  (* pInterlockedDecrement)(LPLONG Target);
    LONG  (* pInterlockedIncrement)(LPLONG Target);
#endif
#ifdef x86
    EXCEPTION_DISPOSITION (__cdecl *p_except_handler3) (EXCEPTION_RECORD *,
        void *, CONTEXT *, DISPATCHER_CONTEXT *);
    BOOL (*p__abnormal_termination) (void);
#else
    EXCEPTION_DISPOSITION (*p__C_specific_handler) (EXCEPTION_RECORD *,
        void *, CONTEXT *, DISPATCHER_CONTEXT *);
#endif

#ifdef x86
    DWORD pMD_CBRtn;
#else
    void (*pMD_CBRtn) (void);
#endif

    struct _REGISTER_DESC * (*pfnOEMGetRegDesc) (DWORD *);
    HRESULT (*pfnOEMReadRegs) (BYTE *, DWORD *, const DWORD);
    HRESULT (*pfnOEMWriteRegs) (BYTE *, const DWORD);

    int (* pKCall)(PKFN, ...);

    void (*DSPFlushContext) (void);
    void (*FPUFlushContext) (void);

    PTHREAD *ppCurFPUOwner;  
    DWORD *  pdwProcessorFeatures;
    
    DWORD (*pKITLIoCtl) (DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
    int (*pNKvsprintfW) (LPWSTR, LPCWSTR, CONST VOID *, int);
} OSAXS_DATA;

#endif
