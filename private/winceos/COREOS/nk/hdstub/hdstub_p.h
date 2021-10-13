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

    hdstub_p.h

Module Description:

    Private header for hdstub

--*/

#pragma once

extern "C" {
#include "kernel.h"

BOOL HdstubInit(KDBG_INIT *);
BOOL WINAPI HdstubDLLEntry(HINSTANCE, ULONG, LPVOID);
}

#include <kdbgpublic.h>
#include <kdbgprivate.h>
#include <kdbgerrors.h>

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
#define HDZONE_MOVE    DEBUGZONE(5)
#define HDZONE_BP0     DEBUGZONE(6)
#define HDZONE_VM      DEBUGZONE(7)
#define HDZONE_DBP     DEBUGZONE(8)
#define HDZONE_UNWIND  DEBUGZONE(9)
#define HDZONE_ALERT   DEBUGZONE(15)

#define HDZONE_DEFAULT (0x8000)

extern void HdstubFreezeSystem();
extern void HdstubThawSystem();

extern BOOL HdstubTrapException(PEXCEPTION_RECORD pex, CONTEXT *pContext,
    BOOLEAN b2ndChance);
extern void HdstubTrapVmPageIn(DWORD dwPageAddr, BOOL bWriteable);
extern void HdstubTrapVmPageOut(DWORD dwPageAddr, DWORD dwNumPages);
extern void HdstubTrapModuleLoad(DWORD dwVmBaseAddr, BOOL fFirstLoad);
extern void HdstubTrapModuleUnload(DWORD dwVmBaseAddr, BOOL fLastUnload);
extern void HdstubNotify (void);

// Hardware event handler prototypes.
extern BOOL HwExceptionHandler (PEXCEPTION_RECORD pex, CONTEXT *pContext, BOOLEAN b2ndChance);
extern void HwPageInHandler (DWORD dwPageAddr, DWORD dwNumPages, BOOL bWriteable);
extern void HwPageOutHandler (DWORD dwPageAddr, DWORD dwNumPages);
extern void HwModLoadHandler (DWORD dwVmBaseAddr);
extern void HwModUnloadHandler (DWORD dwVmBaseAddr);

// Debug output functions
extern void HdstubDbgPrintf(LPCWSTR, ...);

void *HdstubMapAddr(MEMADDR const *addr);
void HdstubUnMapAddr(MEMADDR const *addr);

ULONG
MoveMemory0(
    MEMADDR const *Dest,
    MEMADDR const *Source,
    ULONG Length);

BOOL IsValidProcess(PPROCESS);
BOOL HdstubIsMemoryInaccessible(LPCVOID);
PPROCESS GetProperVM(PPROCESS, void *);
DWORD GetPhysicalAddress(PPROCESS, void *);
DWORD AddrToPA(MEMADDR const *pma);
BOOL GetPageTableEntryPtrNoCheck (PPROCESS, DWORD, DWORD **);
BOOL GetPageTableEntryPtr (PPROCESS, DWORD, DWORD**);

BOOL HdstubInitMemAccess (void);
void HdstubDeInitMemAccess (void);
BOOL HdstubIoControl (DWORD, LPVOID, DWORD);
BOOL HdstubIgnoreIllegalInstruction(DWORD);
BOOL HdstubSanitize(BYTE *, void *, ULONG, BOOL);

// DebuggerData is local to hdstub.  Avoid the extra level of pointer
// indirection.
extern DEBUGGER_DATA DebuggerData;
#define g_pDebuggerData (&DebuggerData)