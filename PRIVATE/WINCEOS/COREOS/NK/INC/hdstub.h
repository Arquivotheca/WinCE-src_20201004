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


typedef BOOL (*HDSTUB_EXCEPTION_FUNC) (PEXCEPTION_RECORD pex,
    CONTEXT *pContext, BOOLEAN b2ndChance);
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

    HRESULT (*pfnIoctl)(DWORD dwFunction, DWORD dwArg1, DWORD dwArg2, DWORD dwArg3, DWORD dwArg4); 
    DWORD dwFilter;
    
    struct _HDSTUB_CLIENT *pCliNext;
} HDSTUB_CLIENT;


enum HdstubClientDisposition
{
    HdstubClientFirst,          // Insert client at head of list
    HdstubClientLast            // Insert client at end of list
};


// Hdstub client initialization struct.
typedef struct _HDSTUB_DATA
{
    // Client Registration
    BOOL (*pfnRegisterClient) (struct _HDSTUB_CLIENT*, int Position);
    BOOL (*pfnUnregisterClient) (struct _HDSTUB_CLIENT*);

    // Cross-client call.
    HRESULT (*pfnCallClientIoctl) (const char *szClientName, DWORD dwFunc, DWORD dw1, DWORD dw2, DWORD dw3, DWORD dw4);
} HDSTUB_DATA;

typedef BOOL (*HDSTUB_CLINIT_FUNC) (HDSTUB_DATA*, void*);

// Hdstub initialization struct.  Function pointers are passed to kernel
// through this structure.
typedef struct _HDSTUB_INIT
{
    DWORD cbSize;       // Sanity check

    // IN
    VOID (WINAPI* pfnInitializeCriticalSection)(LPCRITICAL_SECTION);
    VOID (WINAPI* pfnDeleteCriticalSection)(LPCRITICAL_SECTION);
    VOID (WINAPI* pfnEnterCriticalSection)(LPCRITICAL_SECTION);
    VOID (WINAPI* pfnLeaveCriticalSection)(LPCRITICAL_SECTION);
    BOOL (*pfnINTERRUPTS_ENABLE)(BOOL);
#ifdef MIPS
    LONG (*pfnInterlockedDecrement)(LPLONG);
    LONG (*pfnInterlockedIncrement)(LPLONG);
#endif
#ifdef ARM
    int   (* pfnInSysCall)(void);
#endif
    void  (*pfnCleanupHdstub) ();
    DWORD (*pKITLIoCtl) (DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
    int   (*pNKSprintfW) (LPWSTR, LPCWSTR, CONST VOID *, int);
    struct KDataStruct *pKData;
    ULONG *pulHDEventFilter;
    void (*pfnHwTrap) (void);
    void (*FlushCacheRange) (LPVOID, DWORD, DWORD);
    
    // OUT
    // Function pointers that are called in response to a kernel event.
    BOOL (*pfnException) (PEXCEPTION_RECORD, CONTEXT *, BOOLEAN);
    void (*pfnVmPageIn) (DWORD, BOOL);
    void (*pfnModLoad) (DWORD);
    void (*pfnModUnload) (DWORD);

    // Notifications that need to be tracked by EXDI drivers.
    HDSTUB_EVENT *pEvent;
    DWORD *pdwTaintedModuleCount;

    BOOL (*pfnConnectClient) (HDSTUB_CLINIT_FUNC, void*);
} HDSTUB_INIT;

#endif//_HDSTUB_H_INCLUDED_
