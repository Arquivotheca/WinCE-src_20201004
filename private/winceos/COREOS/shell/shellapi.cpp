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
#include "shellapi.h"
#include "stdlib.h"




int filedesc;

#ifdef SERIAL_SHELL
#define CP_GET_SUCCESS  0
#define CP_GET_NODATA   1
#define CP_GET_ERROR    2
extern "C" void OEMWriteDebugString(unsigned short *str);
extern "C" int OEMReadDebugByte(unsigned short *pBuf);
#endif

extern "C" {
extern	BOOL v_fUseConsole;
extern	BOOL v_fUseDebugOut;
typedef wchar_t * (__cdecl *PFN_fgetws)(wchar_t *, int, FILE *);
typedef int    (__cdecl *PFN_fputws)(const wchar_t *, FILE *);
typedef FILE * (__cdecl *PFN_getstdfilex)(int);
typedef VOID (WINAPI *PFN_OutputDebugStringW)(LPCWSTR lpOutputString);


PFN_fgetws	pfnfgetws;
PFN_fputws	pfnfputws;
PFN_getstdfilex pgetstdfilex;
PFN_OutputDebugStringW pfnOutputDebugStringW;
}


int
_crtinit(void)
{
#ifdef SERIAL_SHELL
    return 1;
#else
	if (v_fUseConsole) {
		HMODULE	hCoreDLL;
		hCoreDLL = (HMODULE)LoadLibrary(TEXT("COREDLL.DLL"));
		if (hCoreDLL == NULL) {
			RETAILMSG (1, (TEXT("Shell: unable to load CoreDLL.\r\n")));
			v_fUseConsole = FALSE;
		} else {
			pfnfgetws = (PFN_fgetws)GetProcAddress (hCoreDLL, TEXT("fgetws"));
			pfnfputws = (PFN_fputws)GetProcAddress (hCoreDLL, TEXT("fputws"));
			pfnOutputDebugStringW = (PFN_OutputDebugStringW)GetProcAddress (hCoreDLL, TEXT("OutputDebugStringW"));
			pgetstdfilex = (PFN_getstdfilex)GetProcAddress (hCoreDLL, TEXT("_getstdfilex"));
			if ((pfnfputws == NULL) || (pfnfgetws == NULL) || (pgetstdfilex == NULL)) {
				RETAILMSG (1, (TEXT("Shell: Can't find fputws or fgetws or _getstdfilex.\r\n")));
				v_fUseConsole = FALSE;
			}
		}
	}
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
		return pfnfgetws (s, cch, pgetstdfilex(0));
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
        if (v_fUseDebugOut && pfnOutputDebugStringW) {
            pfnOutputDebugStringW (s);
            return 1;
        } else {
            return pfnfputws (s, pgetstdfilex(1));
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
