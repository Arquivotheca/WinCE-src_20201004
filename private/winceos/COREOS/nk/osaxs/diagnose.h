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
//
//    diagnose.h - Header for DDx diagnositic utilities
//

#ifndef _DIAGNOSE_H_
#define _DIAGNOSE_H_


#include "kernel.h"
// Nkshx.h defines these, so get rid of them before including DwCeDump.h
#ifdef ProcessorLevel
#undef ProcessorLevel
#endif
#ifdef ProcessorRevision
#undef ProcessorRevision
#endif


#include "DwCeDump.h"
#include "ddx.h"


typedef BOOL (*PFN_KENRELLIBIOCTL) (HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
extern PFN_KENRELLIBIOCTL g_pfnKernelLibIoctl;
extern HANDLE g_hProbeDll;

#define DDX_MAX_DIAGNOSES               8
#define DDX_MAX_SYMBOLS                 64
#define DDX_MAX_PERSISTENT_DIAGNOSES    64
#define DDX_MAX_KERNEL_OBJECTS          400
#define MAX_KHEAP_SIZE                  0x4000000      // 64M max

#define NUM_LOG_PAGES                   2
#define LOG_SIZE                        (NUM_LOG_PAGES * VM_PAGE_SIZE)

#define NUM_HEAP_PAGES                  2
#define HEAP_SIZE                       (NUM_HEAP_PAGES * VM_PAGE_SIZE)
#define DDX_HEAP_SIG                    0xDD8DD801

#define DDX_PROBE_DLL                   L"DDxProbe.dll"

typedef struct _KERNEL_OBJECT
{
    DWORD  type;
    LPVOID pObj;
    int    next;
}
KERNEL_OBJECT, *PKENREL_OBJECT;


typedef struct _DDX_DIAGNOSIS
{
    ULONG32  Type;
    ULONG32  SubType;
    ULONG32  ProcessId;
    ULONG32  ThreadId;
    ULONG32  Severity;
    ULONG32  Confidence;
    WCHAR*   pwszDescription;
    ULONG32  sizeofDescription; 
} 
DDX_DIAGNOSIS, *PDDX_DIAGNOSIS;



typedef struct _DDX_DIAGNOSIS_ID
{
    ULONG32  Type;
    ULONG32  SubType;
    PPROCESS pProcess;
    DWORD    dwThreadId;
    DWORD    dwData;
} 
DDX_DIAGNOSIS_ID, *PDDX_DIAGNOSIS_ID;



typedef struct _DDX_HEAP_ITEM_INFO
{
    /*[IN] */ LPBYTE ptr;
    /*[OUT]*/ LPBYTE pMem;
    /*[OUT]*/ DWORD  cbSize;
    /*[OUT]*/ DWORD  dwFlags;
    /*[OUT]*/ DWORD  dwAllocSentinel1;
    /*[OUT]*/ DWORD  dwAllocSentinel2;
}
DDX_HEAP_ITEM_INFO, *PDDX_HEAP_ITEM_INFO;


enum 
{
    DDX_UNKNOWN_TYPE     = 0,
    DDX_AFFECTED_THREAD  = 1,
    DDX_AFFECTED_MODULE  = 2,
    DDX_AFFECTED_PROCESS = 3
};

enum 
{
    DDX_DIAGNOSIS_ERROR    = 0,
    DDX_POSITIVE_DIAGNOSIS = 1,
    DDX_NO_DIAGNOSIS       = 2
};



__inline
BOOL IsDDxBreak()
{
    switch (DD_ExceptionState.exception->ExceptionCode)
    {
    case STATUS_NO_MEMORY:
    case STATUS_HEAP_CORRUPTION:
    case STATUS_POSSIBLE_DEADLOCK:
    case STATUS_POSSIBLE_STARVATION:
    case STATUS_POSSIBLE_ABONDONED_SYNC:
    case STATUS_STACK_OVERFLOW_WARNING:
        return TRUE;
    default:
        return FALSE;
    }
}


__inline 
LPCWSTR
GetThreadProcName(
                  THREAD* pth
                  )
{
    if (pth && 
        pth->pprcOwner &&
        pth->pprcOwner->lpszProcName)
    {
        return pth->pprcOwner->lpszProcName;
    }

    return NULL;
}


BOOL IsDDxEnabled(void);
int  ddxlog(LPCWSTR pszFmt, ...);
void InitDDxLog(LPWSTR wszKernVA);
DWORD BeginDDxLogging(void);
DWORD EndDDxLogging(void);
LPWSTR GetDDxLogHead(void);
void ResetDDx(void);
int AddSymbol(PPROCESS pProc, DWORD dwAddress);
BOOL AddDiagnosis(CEDUMP_DIAGNOSIS_DESCRIPTOR* pDiagnosis);
BOOL IsKnownDiagnosis(DDX_DIAGNOSIS_ID* pId);
BOOL AddPersistentDiagnosis(DDX_DIAGNOSIS_ID* pId);
int AddAffectedProcess(PPROCESS pProc);
int AddAffectedThread(PTHREAD pThread);
UINT GetNumProcesses(void);
BOOL Diagnose(DWORD dwExceptionCode);
DDxResult_e DiagnoseDeadlock(void);
DDxResult_e DiagnoseException(DWORD dwExceptionCode);
DDxResult_e DiagnoseStackOverflow(DWORD dwExceptionCode);
DDxResult_e DiagnoseStarvation(void);
DDxResult_e DiagnoseHeap(DWORD dwExceptionCode);
DDxResult_e DiagnoseOOM(DWORD dwExceptionCode);
LPBYTE DDxAlloc(DWORD cbSize, DWORD dwFlags);
LPBYTE DDxClearHeap(void);
BOOL IsHeapItem (DWORD dwAddr);
int CountHeaps (PPROCESS pProc);
BOOL GetProcessHeapInfo (PPROCESS pProc, CEDUMP_HEAP_INFO* phi, int* pindex);
PMODULE IsAddrInModule(PPROCESS pProcess, DWORD dwAddr);


extern CEDUMP_DIAGNOSIS_LIST       g_ceDmpDiagnosisList;
extern CEDUMP_DIAGNOSIS_DESCRIPTOR g_ceDmpDiagnoses[];
extern UINT                        g_nDiagnoses;
extern CEDUMP_BUCKET_PARAMETERS    g_ceDmpDDxBucketParameters;

extern HRESULT GetBucketParameters(PCEDUMP_BUCKET_PARAMETERS pceDmpBucketParameters, PTHREAD pDmpThread, PPROCESS pDmpProc);


#endif // _DIAGNOSE_H_

