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
#pragma once

#include <windows.h>



static DWORD gTlsIndex;

static void _callback(DWORD ThreadId, LPVOID SlotValue) 
{
}

//
// EXPOSED APIs
//

__inline bool WINAPI CeTlsProcessAttach()
{
    gTlsIndex = CeTlsAlloc (_callback);
    return (gTlsIndex == 0xffffffff) ? false : true;
}

__inline void WINAPI CeTlsProcessDetach()
{
    TlsFree (gTlsIndex);
}

__inline void WINAPI CeTlsThreadAttach()
{
    // No action required
}

__inline void WINAPI CeTlsThreadDetach()
{
    CeTlsFree (gTlsIndex);
}



