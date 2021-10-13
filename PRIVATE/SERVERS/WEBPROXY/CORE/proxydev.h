//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __PROXYDEV_H__
#define __PROXYDEV_H__

#include "global.h"
#include "service.h"
#include "sync.hxx"
#include "proxy.h"

#define ServiceInitLock		InitializeCriticalSection(&g_csService);
#define ServiceDeinitLock	DeleteCriticalSection(&g_csService);
#define ServiceLock			EnterCriticalSection(&g_csService);
#define ServiceUnlock		LeaveCriticalSection(&g_csService);

extern DWORD g_dwState;
extern CRITICAL_SECTION g_csService;
extern HINSTANCE g_hInst;

#endif // __PROXYDEV_H__

