//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
