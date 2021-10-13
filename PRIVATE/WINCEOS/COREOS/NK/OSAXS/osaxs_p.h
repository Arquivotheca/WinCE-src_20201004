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

    Osaxs_p.h

Module Description:

    Private header for osaxs

--*/
#pragma once
#ifndef _OSAXS_P_H
#define _OSAXS_P_H

#ifdef TARGET_BUILD
#define KData (*g_OsaxsData.pKData)

#include "kernel.h"
#include "kdstub.h"
#include "hdstub.h"
#include "osaxs.h"

#define ProcArray (g_OsaxsData.pProcArray)

#endif
#include "OsAxsFlexi.h"
#include "OsAxsProtocol.h"


// This is the index into the LITE_EXTRA field.
#define E32_EXTRA_DBGDIR_RVA (6)


/* FLEXI structures */
typedef struct _FLEXI_FIELD_INFO
{
    ULONG32 ul32Id;
    ULONG32 ul32Size;
    WCHAR *szLabel;
    WCHAR *szFormat;
} FLEXI_FIELD_INFO;

// Macros used to generate FLEXI_FIELD_INFO list.  
#define BEGIN_FIELD_INFO_LIST(name)  FLEXI_FIELD_INFO name [] = {
#define FIELD_INFO(id, size, label, format) {(id), (size), (label), (format)},
#define END_FIELD_INFO_LIST() };

// Hard coded size of the FLEXI Info header.
#define CB_FLEXIPTM_HEADER 16

/*
 * Number of Bytes in the Flexi Field Info structure when stored to memory:
 * Offset    Data
 * ------    ----
 * 0         Field Identify
 * 4         Field Stored Size
 * 8         Offset to the Field Label String (which follows all of the field stuctures)
 * 12        Offset to the Field Format String (which follows all of the field structures)
 * 16        End of Field Info Structure.
 */
#define CB_FIELDINFO 16

// Size of Process Name
#define CCH_PROCNAME  32

// Size of Process command line
#define CCH_CMDLINE   128

// Number of fields for Process Data.
#define C_PROCESSFIELDS (sizeof (ProcessDesc) / sizeof (ProcessDesc[0]))

// Number of fields for Thread data.
#define C_THREADFIELDS (sizeof (ThreadDesc) / sizeof (ThreadDesc[0]))

// Size of module name
#define CCH_MODULENAME  32

// Size of module ref counts.
#define CB_MODREFCNT (sizeof (WORD) * MAX_PROCESSES)

// Number of Module fields.
#define C_MODULEFIELDS (sizeof (ModuleDesc) / sizeof (ModuleDesc[0]))

/* Compatibility layer - TODO: Clean this up.  Kind of messy now. */
#if defined (TARGET_BUILD)

#define pVAcs                   (g_OsaxsData.pVAcs)
#define NullSection             (*(g_OsaxsData.pNullSection))
#define NKSection               (*(g_OsaxsData.pNKSection))
#define FetchThreadStruct(pthd) ((PTHREAD)(pthd))
#define FetchProcArray()        (g_OsaxsData.pProcArray)
#define FetchProcStruct(pproc)  ((PROCESS*)(pproc))
#define FetchKData()            (g_OsaxsData.pKData)
#define FetchDeviceAddr(x)      ((void *)(x))
#define MD_CBRtn                (g_OsaxsData.pMD_CBRtn)
#define FetchDword(base, off)   ( ((DWORD*)(base))[off] )
#define FetchModuleStruct(ptr)  ( (MODULE *)(ptr) )
#define FetchTOC()              (g_OsaxsData.pRomHdr)
#define FetchLogPtr()           (g_OsaxsData.pLogPtr)
#define FetchMemoryInfo()       ((MEMORYINFO *)(g_OsaxsData.pKData ? g_OsaxsData.pKData->aInfo[KINX_MEMINFO] : NULL))
#define FetchCopyEntry(ptr)     ((COPYentry *)(ptr))
#define IsCurrentThread(pth)    ((pth) == pCurThread)
#define pfnOEMKDIoControl       (g_OsaxsData.pfnOEMKDIoControl)
#define pfnNKKernelLibIoControl (g_OsaxsData.pfnNKKernelLibIoControl)
#define KCall                   (g_OsaxsData.pKCall)
#define ppCaptureDumpFileOnDevice (g_OsaxsData.ppCaptureDumpFileOnDevice)
#define pfnDoThreadGetContext   (g_OsaxsData.pfnDoThreadGetContext)
#define pfnNKGetThreadCallStack (g_OsaxsData.pfnNKGetThreadCallStack)
#define OsAxsIsCurThread(pth)   (pCurThread == (pth))
#define pfnGetSystemInfo        (g_OsaxsData.pfnGetSystemInfo)
#define pfnGetLastError         (g_OsaxsData.pfnGetLastError)
#define pfnSetLastError         (g_OsaxsData.pfnSetLastError)
#define hCoreDll                (*(g_OsaxsData.phCoreDll))
#if defined(MIPS) 
#define InterlockedDecrement    (g_OsaxsData.pInterlockedDecrement)
#define InterlockedIncrement    (g_OsaxsData.pInterlockedIncrement)
#endif
#endif /* Compatibility layer */


DWORD GetThreadProgramCounter (const THREAD *pth);

#ifdef TARGET_BUILD

/********* Debug Message Macro */
#define OXZONE_INIT      DEBUGZONE(0)   // 0x0001 
#define OXZONE_PSL       DEBUGZONE(1)   // 0x0002
#define OXZONE_FLEXI     DEBUGZONE(2)   // 0x0004
#define OXZONE_MEM       DEBUGZONE(3)   // 0x0008
#define OXZONE_PACKDATA  DEBUGZONE(4)
#define OXZONE_CV_INFO   DEBUGZONE(5)
#define OXZONE_IOCTL     DEBUGZONE(6)
#define OXZONE_ZONE7     DEBUGZONE(7)
#define OXZONE_ZONE8     DEBUGZONE(8)
#define OXZONE_ZONE9     DEBUGZONE(9)
#define OXZONE_ZONEa     DEBUGZONE(10)
#define OXZONE_ZONEb     DEBUGZONE(11)
#define OXZONE_ZONEc     DEBUGZONE(12)
#define OXZONE_ZONEd     DEBUGZONE(13)
#define OXZONE_DWDMPGEN  DEBUGZONE(14)   // 0x4000
#define OXZONE_ALERT     DEBUGZONE(15)   // 0x8000

#define OXZONE_DEFAULT   (0x8000) // OXZONE_ALERT

#define DEBUGGERPRINTF DbgPrintf
#include "debuggermsg.h"

/********** targexcept.c */
#if defined(x86)
extern "C" EXCEPTION_DISPOSITION __cdecl _except_handler3 (EXCEPTION_RECORD *, void *,
    CONTEXT *, DISPATCHER_CONTEXT *);
extern "C" BOOL __abnormal_termination(void);
#else
extern "C" EXCEPTION_DISPOSITION __C_specific_handler (EXCEPTION_RECORD *, void *, 
    CONTEXT *, DISPATCHER_CONTEXT *);
#endif

#if DEBUG
#define KD_ASSERT(exp) if (!(exp)) DEBUGGERMSG (OXZONE_ALERT, (L"**** KD_ASSERT **** " L#exp L"\r\n"))
#else
#define KD_ASSERT(exp)
#endif

/********** initt0.c / initt1.c */
extern HDSTUB_DATA Hdstub;
extern OSAXS_DATA g_OsaxsData;

/********** targdbg.cpp */
extern void (*g_pfnOutputDebugString) (const char *, ...);
extern DBGPARAM dpCurSettings;
extern void DbgPrintf (LPCWSTR, ...);

/********** mem.cpp */
extern void* SafeDbgVerify (void * pvAddr, BOOL fProbeOnly);
extern DWORD SafeMoveMemory (PROCESS *pProcess, void *pvDestination, void *pvSource, DWORD dwLength);

/********** except.cpp */
extern CONTEXT *g_pExceptionCtx;

#endif /* TARGET_BUILD */

/********** memcommon.c */
struct ModuleSignature
{
    DWORD dwType;
    GUID guid;
    DWORD dwAge;
};

extern void *MapPtrInProc (void *pv, PROCESS *pProc);
HRESULT GetProcessDebugInfo (PROCESS *, ModuleSignature *);
HRESULT GetModuleDebugInfo (MODULE *pMod, ModuleSignature *);


#if 0
/********** pslret.c */
extern HRESULT TranslateRAForPSL (PSLRETSTATE *, PTHREAD, PSLRETINOUT *);
extern BOOL ThreadExists (DWORD);
#endif

/********** flexptmi.c */
extern HRESULT WriteFieldInfo (FLEXI_FIELD_INFO *pFields, const DWORD cFields, 
    DWORD &riOut, const DWORD cbOut, BYTE *pbOut);
extern HRESULT WritePTMResponseHeader(DWORD, DWORD, DWORD,
    DWORD &, const DWORD, BYTE *);
extern HRESULT GetFPTMI (FLEXI_REQUEST *, DWORD *, BYTE *);

/********** DwDmpGen.cpp */
extern SAVED_THREAD_STATE * g_psvdThread;
extern PEXCEPTION_RECORD g_pExceptionRecord;
extern PCONTEXT          g_pContextException;
BOOL CaptureDumpFile(PEXCEPTION_RECORD pExceptionRecord, CONTEXT *pContextRecord, BOOLEAN fSecondChance);

/********** Inline utility functions to write data into a buffer with a specified
            size at a specified offset */
// Write an arbitrary buffer to output buffer
static __forceinline HRESULT Write (const void * const pv, const DWORD cb, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;
    if (!pbOut || ((riOut + cb ) <= cbOut))
    {
        if (pbOut)
            memcpy (&pbOut[riOut], pv, cb);
        riOut += cb;
    }
    else
        hr = E_OUTOFMEMORY;
    return hr;
}

// Write a byte to output buffer
static __forceinline HRESULT Write (const BYTE b, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;

    if (!pbOut || (riOut < cbOut))
    {
        if (pbOut)
            pbOut[riOut] = b;
        riOut++; 
    }
    else
        hr = E_OUTOFMEMORY;
    return hr;
}

// Write a word to the output buffer
static __forceinline HRESULT Write (const WORD w, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;
    if (!pbOut || ((riOut + sizeof (w)) <= cbOut))
    {
        if (pbOut)
            memcpy (&pbOut[riOut], &w, sizeof (w));
        riOut += sizeof (w);
    }
    else
        hr = E_OUTOFMEMORY;
    return hr;
}

// Write a DWORD to the output buffer
static __forceinline HRESULT Write (const DWORD dw, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;
    if (!pbOut || ((riOut + sizeof (dw)) <= cbOut))
    {
        if (pbOut)
            memcpy (&pbOut[riOut], &dw, sizeof (dw));
        riOut += sizeof (dw);
    }
    else
        hr = E_OUTOFMEMORY;
    return hr;
}

// Write a 64-bit Ulong to the output buffer
static __forceinline HRESULT Write (const ULONGLONG ul64, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;
    if (!pbOut || ((riOut + sizeof (ul64)) <= cbOut))
    {
        if (pbOut)
            memcpy (&pbOut[riOut], &ul64, sizeof (ul64));
        riOut += sizeof (ul64);
    }
    else
        hr = E_OUTOFMEMORY;
    return hr;
}


/********** strings.c */
extern void WideToAnsi (const WCHAR *, char *, DWORD);
size_t kdbgwcslen(const wchar_t * pWstr);
int kdbgwcsnicmp(const WCHAR *wz1, const WCHAR *wz2, size_t i);

/********** except.cpp */
enum _CTXFLAGS
{
    GET_CTX_ALL = 0x7,
    GET_CTX_HDR = 0x1,
    GET_CTX_DESC = 0x2,
    GET_CTX = 0x4,
};

ULONG GetCallStack(HANDLE hThrd, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip);

BOOL KdpFlushExtendedContext(CONTEXT* pCtx);

HRESULT SaveExceptionContext (CONTEXT            *  pCtx,
                              SAVED_THREAD_STATE *  psvdThread,
                              CONTEXT            ** ppCtxSave,
                              SAVED_THREAD_STATE ** ppsvdThreadSave);

extern HRESULT GetExceptionContext (DWORD dwFlags, DWORD *pcbBuf, BYTE *pbBuf);
extern HRESULT WriteExceptionContext (DWORD &riOut, const DWORD cbOut, BYTE *pbOut, BOOL fIntegerOnly);

/********** packdata.cpp */
#define E32_EXTRA_DBGDIR_RVA (6)
HRESULT PackProcess (PROCESS *, OSAXS_PORTABLE_PROCESS *);
HRESULT PackModule (PROCESS *, OSAXS_PORTABLE_MODULE *);
HRESULT PackModule (MODULE *, OSAXS_PORTABLE_MODULE *);
HRESULT GetStruct (OSAXS_GETOSSTRUCT *, void *, DWORD *);
HRESULT PackThreadContext (THREAD *, void *, DWORD *);
HRESULT UnpackThreadContext (THREAD *, void *, DWORD);
HRESULT GetModuleO32Data (DWORD dwAddrModule, DWORD *pdwNumO32Data, void *pvResult, DWORD *pcbResult);
#if defined(x86)
HRESULT GetExceptionRegistration(DWORD* pdwBuff);
#endif

#ifdef OSAXST1_BUILD
HRESULT ManipulateVm (DWORD, DWORD, DWORD, DWORD *);
#endif

#endif
