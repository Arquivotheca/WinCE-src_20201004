//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// --------------------------------------------------------------------------
//
//
// Module:
//
//     wpcdllp.h
//
// Purpose:
//
//    Internal include file used by the core dll.
//
// --------------------------------------------------------------------------

#ifndef __COREDLL_H__
#define __COREDLL_H__


#ifdef DEBUG
#define DBGFIXHP    DEBUGZONE(0)
#define DBGLMEM     DEBUGZONE(1)
#define DBGMOV      DEBUGZONE(2)
#define DBGSBLK     DEBUGZONE(3)
#define DBGVIRTMEM  DEBUGZONE(4)
#define DBGDEVICE   DEBUGZONE(5)
#define DBGSTDIO    DEBUGZONE(8)
#define DBGSTDIOHF  DEBUGZONE(9)
#define DBGSHELL    DEBUGZONE(10)
#define DBGIMM		DEBUGZONE(11)
#define DBGEH       DEBUGZONE(11)
#define DBGHEAPVAL  DEBUGZONE(12)
#define DBGRMEM     DEBUGZONE(13)
#define DBGHEAP2    DEBUGZONE(14)

BOOL WINAPI HeapValidate(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem);

#define DEBUGHEAPVALIDATE(heap) \
    ((void)((DBGHEAPVAL) ? (HeapValidate((HANDLE) heap, 0, NULL)), 1 : 0))
#else
#define DEBUGHEAPVALIDATE(heap) ((void)0)

#endif

#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI LMemInit(VOID);
extern HANDLE hInstCoreDll;

#ifdef __cplusplus
}
#endif

#endif /* __COREDLL_H__ */

