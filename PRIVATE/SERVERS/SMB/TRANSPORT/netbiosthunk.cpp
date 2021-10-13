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
#include "nb.h"
#include <windows.h>
#include <afdfunc.h>
#include "netbios.h"


HANDLE g_hNetbiosIOCTL = INVALID_HANDLE_VALUE;


DWORD NETbiosThunk(DWORD x1, DWORD dwOpCode, PVOID pNCB, DWORD cBuf1,
              PBYTE pBuf1, DWORD cBuf2, PDWORD pBuf2)
{    
    ASSERT(INVALID_HANDLE_VALUE != g_hNetbiosIOCTL);
    NB_IOCTL_PASS_STRUCT pass;
    pass.dwOpCode = dwOpCode;
    pass.pNCB = pNCB;
    pass.cBuf1 = cBuf1;
    pass.pBuf1 = pBuf1;
    pass.cBuf2 = cBuf2;
    pass.pBuf2 = pBuf2;

    return DeviceIoControl(g_hNetbiosIOCTL, NB_IOCTL_FUNCTION_CALL, &pass, sizeof(NB_IOCTL_PASS_STRUCT), 0,0,NULL,0);
}
