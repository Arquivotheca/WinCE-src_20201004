//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@doc LIBRARY


Module Name:
    NetInit.h

Abstract:
    Definition of a helper functions to initialize the system

Revision History:
     2-Feb-2000		Created

-------------------------------------------------------------------*/
#ifndef _NET_INIT_H_
#define _NET_INIT_H_

#include <windows.h>
#include <tchar.h>
#include "netmain.h"  // Include NetMain.h to get the flag definitions.


#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI NetInitStartLogWrap(HANDLE h);
BOOL WINAPI NetInit(DWORD param);

#ifdef __cplusplus
}
#endif

#endif
