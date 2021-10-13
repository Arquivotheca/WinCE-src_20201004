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

