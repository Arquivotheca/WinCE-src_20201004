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
/*---------------------------------------------------------------------------*\
 *
 *  module: shellapi.cpp
 *
\*---------------------------------------------------------------------------*/



#include <windows.h>
#include <stdio.h>
#include "shellapi.h"

int filedesc;

#ifdef SERIAL_SHELL
#define CP_GET_SUCCESS  0
#define CP_GET_NODATA   1
#define CP_GET_ERROR    2
extern "C" void OEMWriteDebugString(unsigned short *str);
extern "C" int OEMReadDebugByte(unsigned short *pBuf);
#endif

extern "C" {
extern  BOOL v_fUseConsole;
extern  BOOL v_fUseDebugOut;
}

int
_crtinit(void)
{
#ifdef SERIAL_SHELL
    return 1;
#else
    if (!v_fUseConsole) {
        while((filedesc = UU_ropen (TEXT("con"), 0x10002)) == -1) {
            // wait 2 seconds and keep opening console.
            Sleep(2000);
        }
    }
    return 1;
#endif
} 


int
GetChar (void)
{

#ifdef SERIAL_SHELL
    unsigned short ch;
    int status;

    while ( (status=OEMReadDebugByte(&ch)) == CP_GET_NODATA)
    Sleep(1000);
    if (status == CP_GET_SUCCESS)
       return (int)ch;
    else
       return EOF;
#else
    unsigned char ch;
    UINT cnt;

    if (filedesc != -1)
    {
        do 
        {
            cnt = UU_rread (filedesc, &ch, 1);
            if (0 == cnt)
            {
                /* Yes, we are polling, but let's try not 
                   to take up too much CPU */
                Sleep(1000);
            } else if (-1 == cnt)
        return EOF;
        } while (0 == cnt);
        return (int)ch;
    }
    return EOF;
#endif    
}

   

TCHAR *
Gets (TCHAR *s, int cch)
{
    int ch = EOF;
    int iIndex = 0;
    TCHAR *pch;

    if (v_fUseConsole) {
        // Make sure that DebugOut is disabled
        v_fUseDebugOut = FALSE;
        return _fgetts (s, cch, stdin);
    }

    pch = s;
    if(cch <= 1)
    {
        return 0;
    }
    while ((iIndex < (cch - 1)) && (ch = GetChar()) != EOF) 
    {
        if ((*(pch+iIndex) = (TCHAR)ch) == TEXT('\r')) 
        {
            break;
        }
    iIndex++;
    }
    if (ch == EOF)
    return 0;
    *(pch+iIndex) = TEXT('\0');

    return s;
} 



int
Puts (const TCHAR *s)
{
#ifdef SERIAL_SHELL
    OEMWriteDebugString(s);
    return 1;
#else
    char szAscii [256];
    size_t  nChars;
    
    if (v_fUseConsole) {
        if (v_fUseDebugOut) {
            OutputDebugStringW (s);
            return 1;
        } else {
            return _fputts (s, stdout);
        }
    }

    if ((filedesc != -1)
        && ((nChars = wcstombs (szAscii,s,256)) != (size_t)-1))
    {
        return UU_rwrite (filedesc, (unsigned char *)szAscii, nChars);
    }
        
    return EOF;
#endif
}
