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

#ifndef _SPL_HEAP_H
#define _SPL_HEAP_H

#if defined(x86)
#ifdef SHIP_BUILD
#define SPL_HEAP_ENABLED 0x0
#else
#define SPL_HEAP_ENABLED 0x1
#endif // SHIP_BUILD
#else
#define SPL_HEAP_ENABLED 0x0
#endif // x86

#if SPL_HEAP_ENABLED

BOOL KSEN_ProtectFromEntry (DWORD dwEntry);
void KSLV_ProtectFromEntry (DWORD dwEntry, LPDWORD pdwProtect);

BOOL KSEN_PageParamFormProtect (PPROCESS pprc, DWORD fProtect, DWORD dwAddr);
void KSLV_PageParamFormProtect (PPROCESS pprc, DWORD fProtect, DWORD dwAddr, LPDWORD pdwRet);

BOOL KSEN_VirtualProtect (PPROCESS pprc, DWORD dwAddr, DWORD cPages, DWORD fNewProtect);
void KSLV_VirtualProtect (PPROCESS pprc, DWORD dwAddr, DWORD cPages, DWORD fNewProtect, LPDWORD pdwRet);

BOOL KSEN_LoadPageTable (DWORD addr, DWORD dwEip, DWORD dwErrCode);
void KSLV_LoadPageTable (DWORD addr, DWORD dwEip, LPDWORD pdwEntry, LPBOOL pfRet);

BOOL KSEN_ProcessPageFault (PPROCESS pprc, DWORD dwAddr, BOOL fWrite, DWORD dwPc, DWORD dwEntry);
void KSLV_ProcessPageFault (PPROCESS pprc, DWORD dwAddr, BOOL fWrite, DWORD dwPc, LPDWORD pdwEntry, LPBOOL pfRet);

BOOL KSEN_CreateHeap (void);
void KSLV_CreateHeap (LPHANDLE lphHeap);

#else

// Return for all KSEN_xxx functions:
// 1: shim handled this call (skip normal processing)
// 0: shim bypassed this call (continue normal processing)

__inline BOOL KSEN_ProtectFromEntry (DWORD dwEntry) {return FALSE;}
__inline void KSLV_ProtectFromEntry (DWORD dwEntry, LPDWORD pdwProtect) {}

__inline BOOL KSEN_PageParamFormProtect (PPROCESS pprc, DWORD fProtect, DWORD dwAddr) {return FALSE;}
__inline void KSLV_PageParamFormProtect (PPROCESS pprc, DWORD fProtect, DWORD dwAddr, LPDWORD pdwRet) {}

__inline BOOL KSEN_VirtualProtect (PPROCESS pprc, DWORD dwAddr, DWORD cPages, DWORD fNewProtect) {return FALSE;}
__inline void KSLV_VirtualProtect (PPROCESS pprc, DWORD dwAddr, DWORD cPages, DWORD fNewProtect, LPDWORD pdwRet) {}

__inline BOOL KSEN_LoadPageTable (DWORD addr, DWORD dwEip, DWORD dwErrCode) {return FALSE;}
__inline void KSLV_LoadPageTable (DWORD addr, DWORD dwEip, LPDWORD pdwEntry, LPBOOL pfRet) {}

__inline BOOL KSEN_ProcessPageFault (PPROCESS pprc, DWORD dwAddr, BOOL fWrite, DWORD dwPc, DWORD dwEntry) {return FALSE;}
__inline void KSLV_ProcessPageFault (PPROCESS pprc, DWORD dwAddr, BOOL fWrite, DWORD dwPc, LPDWORD pdwEntry, LPBOOL pfRet) {}

__inline BOOL KSEN_CreateHeap (void) {return FALSE;}
__inline void KSLV_CreateHeap (LPHANDLE lphHeap) {}

#endif // SPL_HEAP_ENABLED

#endif // _SPL_HEAP_H

