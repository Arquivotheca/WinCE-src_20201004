//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
        BOOL fIntSave = g_kdKernData.pINTERRUPTS_ENABLE(FALSE); 

        // Get it into a string
        NKwvsprintfW(wszBuf, lpszFmt, (LPVOID)(((DWORD)&lpszFmt)+sizeof(lpszFmt)), sizeof(wszBuf)/sizeof(WCHAR));

        g_pfnOutputDebugString((char *)wszBuf, WideToSingle(wszBuf));

        g_kdKernData.pINTERRUPTS_ENABLE(fIntSave); 
    }
}

