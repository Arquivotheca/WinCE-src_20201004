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

    hdstub.h

Abstract:

    Defines and types for Hdstub.

Environment:

    CE Kernel

--*/

#pragma once

#ifndef _HDSTUB_H_INCLUDED_
#define _HDSTUB_H_INCLUDED_

#include "hdstub_common.h"


typedef BOOL (*HDSTUB_EXCEPTION_FUNC) (PEXCEPTION_RECORD pex, CONTEXT *pContext, BOOLEAN b2ndChance);
typedef BOOL (*HDSTUB_VMPAGEIN_FUNC) (DWORD dwPageAddr, DWORD dwNumPages, BOOL bWriteable);
typedef BOOL (*HDSTUB_MODLOAD_FUNC) (DWORD dwStructAddr);
typedef BOOL (*HDSTUB_MODUNLOAD_FUNC) (DWORD dwStructAddr);


// Hdstub client record
typedef struct _HDSTUB_CLIENT
{
    char *szClientName;
    
    HDSTUB_EXCEPTION_FUNC pfnException;
    HDSTUB_VMPAGEIN_FUNC  pfnVmPageIn;
    HDSTUB_MODLOAD_FUNC   pfnModLoad;
    HDSTUB_MODUNLOAD_FUNC pfnModUnload;

    HRESULT (*pfnIoctl)(DWORD dwFunction, void *pvFuncArgs); 
    DWORD dwFilter;
    
    struct _HDSTUB_CLIENT *pCliNext;
} HDSTUB_CLIENT;


enum HdstubClientDisposition
{
    HdstubClientFirst,          // Insert client at head of list
    HdstubClientLast,           // Insert client at end of list
    HdstubClientWatson          // Special Watson dumping client
};


// Hdstub client initialization struct.
typedef struct _HDSTUB_DATA
{
    // Client Registration
    BOOL (*pfnRegisterClient) (struct _HDSTUB_CLIENT*, int Position);
    BOOL (*pfnUnregisterClient) (struct _HDSTUB_CLIENT*);

    // Cross-client call.
    HRESULT (*pfnCallClientIoctl) (const char *szClientName, DWORD dwFunc, void *pvFuncArgs);
} HDSTUB_DATA;

typedef BOOL (*HDSTUB_CLINIT_FUNC) (HDSTUB_DATA*, void*);

// Hdstub initialization struct.  Function pointers are passed to kernel
// through this structure.
typedef struct _HDSTUB_INIT
{
    DWORD cbSize;       // Sanity check

    // IN
    BOOL (*pfnINTERRUPTS_ENABLE)(BOOL);
    VOID (*pfnInitializeCriticalSection)(LPCRITICAL_SECTION);
    VOID (*pfnDeleteCriticalSection)(LPCRITICAL_SECTION);
    VOID (*pfnEnterCriticalSection)(LPCRITICAL_SECTION);
    VOID (*pfnLeaveCriticalSection)(LPCRITICAL_SECTION);
#ifdef MIPS
    LONG (*pfnInterlockedDecrement)(LONG volatile *);
    LONG (*pfnInterlockedIncrement)(LONG volatile *);
#endif
#ifdef ARM
    int   (* pfnInSysCall)(void);
#endif
    void  (*pfnCleanupHdstub) ();
    DWORD (*pKITLIoCtl) (DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
    int   (*pNKSprintfW) (LPWSTR, LPCWSTR, va_list, int);
    struct KDataStruct *pKData;
    PPROCESS pprcNK;
    ULONG *pulHDEventFilter;
    void (*pfnHwTrap) (void);
    void (*FlushCacheRange) (LPVOID, DWORD, DWORD);

    // OUT
    // Function pointers that are called in response to a kernel event.
    BOOL (*pfnException) (PEXCEPTION_RECORD, CONTEXT *, DWORD);
    void (*pfnVmPageIn) (DWORD, BOOL);
    void (*pfnModLoad) (DWORD);
    void (*pfnModUnload) (DWORD);

    // Notifications that need to be tracked by EXDI drivers.
    HDSTUB_EVENT *pEvent;
    DWORD *pdwTaintedModuleCount;
    HDSTUB_SHARED_GLOBALS *pHdSharedGlobals;

    BOOL (*pfnConnectClient) (HDSTUB_CLINIT_FUNC, void*);
} HDSTUB_INIT;

#endif//_HDSTUB_H_INCLUDED_
