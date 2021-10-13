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
/*++

Module Name:

    hdstub_p.h

Module Description:

    Private header for hdstub

--*/

#pragma once

#ifndef _HDSTUB_P_H_INCLUDED_
#define _HDSTUB_P_H_INCLUDED_

extern struct KDataStruct *g_pKData;

#define KData (*g_pKData)

#include "kernel.h"
#include "hdstub.h"

// This option flushes memory before entering hardware trap to ensure that memory is in a more consistent state for some
// hardware probes.
#define HDHW_NEEDS_CACHE_FLUSH

//---------------------
// Debug Zone Settings
//---------------------
extern DBGPARAM dpCurSettings;

#define HDZONE_INIT    DEBUGZONE(0)
#define HDZONE_ENTRY   DEBUGZONE(1)
#define HDZONE_CLIENT  DEBUGZONE(2)
#define HDZONE_HW      DEBUGZONE(3)
#define HDZONE_CRITSEC DEBUGZONE(4)
#define HDZONE_ALERT   DEBUGZONE(15)

#define HDZONE_DEFAULT (0x8000)

#define DEBUGGERPRINTF HdstubDbgPrintf
#include "debuggermsg.h"

extern HDSTUB_INIT g_HdStubData;

extern BOOL HdstubInit(HDSTUB_INIT *pHdInit);
extern HRESULT HdstubCallClientIoctl (const char *szClientName, DWORD dwFunction, void *);
extern BOOL HdstubRegisterClient (HDSTUB_CLIENT *pClient, int Disposition);
extern BOOL HdstubUnregisterClient (HDSTUB_CLIENT *pClient);

extern BOOL HdstubTrapException(PEXCEPTION_RECORD pex, CONTEXT *pContext,
    DWORD dwExceptionPhase);
extern void HdstubTrapVmPageIn(DWORD dwPageAddr, BOOL bWriteable);
extern void HdstubTrapModuleLoad(DWORD dwVmBaseAddr);
extern void HdstubTrapModuleUnload(DWORD dwVmBaseAddr);
extern void HdstubNotify (void);

// Hardware event handler prototypes.
extern BOOL HwExceptionHandler (PEXCEPTION_RECORD pex, CONTEXT *pContext, BOOLEAN b2ndChance);
extern void HwPageInHandler (DWORD dwPageAddr, DWORD dwNumPages, BOOL bWriteable);
extern void HwModLoadHandler (DWORD dwVmBaseAddr);
extern void HwModUnloadHandler (DWORD dwVmBaseAddr);

// Debug output functions
extern void HdstubDbgPrintf(LPCWSTR, ...);

// Pointers to implement Printf
extern void (WINAPIV* g_pfnNKvsprintfW)(LPWSTR, LPCWSTR, va_list, int);
extern void (*g_pfnOutputDebugString)(const char *sz, ...);
extern DWORD (*g_pfnKitlIoctl) (DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);

// Global event structure
extern HDSTUB_EVENT g_Event;
extern HDSTUB_INIT g_HdStubData;

#ifdef MIPS
extern LONG (*g_pfnInterlockedDecrement)(LPLONG);
extern LONG (*g_pfnInterlockedIncrement)(LPLONG);
#define InterlockedDecrement g_pfnInterlockedDecrement 
#define InterlockedIncrement g_pfnInterlockedIncrement 
#endif

//
// InSysCall for ARM.
//
#if defined(ARM)
#define InSysCall               (g_HdStubData.pfnInSysCall)
#endif

// HdstubNotify is now living in kwin32.c as HwTrap
#define HdstubNotify            (g_HdStubData.pfnHwTrap) 

#define g_pprcNK                (g_HdStubData.pprcNK)

#define InitializeCriticalSection (g_HdStubData.pfnInitializeCriticalSection)
#define DeleteCriticalSection   (g_HdStubData.pfnDeleteCriticalSection)
#define EnterCriticalSection    (g_HdStubData.pfnEnterCriticalSection)
#define LeaveCriticalSection    (g_HdStubData.pfnLeaveCriticalSection)

#endif
