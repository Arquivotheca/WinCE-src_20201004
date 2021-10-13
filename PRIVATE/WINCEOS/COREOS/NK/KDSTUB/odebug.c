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

    odebug.c

Abstract:

    This module implements printing to extra serial port.


--*/
#include <windows.h>
#include "kdp.h"

#define DBG_PRINTF_BUFSIZE 512

void (*g_pfnOutputDebugString)(char*, ...) = NULL;

static WCHAR wszBuf[DBG_PRINTF_BUFSIZE];

BOOL g_fInKdpSendDbgMessage = FALSE;

DWORD WideToSingle(TCHAR *wszBuf)
{
    int i=0,j=0;
    while(wszBuf[i])  {
        ((char *)wszBuf)[j] = (char)wszBuf[i];
        j++;
        i++;
    }
    ((char *)wszBuf)[j] = 0;
    return j;
}   

VOID NKOtherPrintfW(LPWSTR lpszFmt, ...) 
{

    if (g_pfnOutputDebugString) 
    {
        BOOL fIntSave = INTERRUPTS_ENABLE (FALSE); 
        DWORD dwLen;

        // Get it into a string
        NKwvsprintfW(wszBuf, lpszFmt, (LPVOID)(((DWORD)&lpszFmt)+sizeof(lpszFmt)), sizeof(wszBuf)/sizeof(WCHAR));

        g_pfnOutputDebugString((char *)wszBuf, dwLen = WideToSingle(wszBuf));

        INTERRUPTS_ENABLE (fIntSave); 

        if (KDZONE_DBGMSG2KDAPI && !g_fInKdpSendDbgMessage)
        {
            g_fInKdpSendDbgMessage = TRUE; // Prevent recursive entry
            KdpSendDbgMessage((CHAR*)wszBuf, dwLen+1);
            g_fInKdpSendDbgMessage = FALSE;
        }
    }
}

