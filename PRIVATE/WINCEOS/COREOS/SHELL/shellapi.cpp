//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*---------------------------------------------------------------------------*\
 *
 *  module: shellapi.c
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
typedef wchar_t * (__cdecl *PFN_fgetws)(wchar_t *, int, FILE *);
typedef int    (__cdecl *PFN_fputws)(const wchar_t *, FILE *);
typedef FILE * (__cdecl *PFN_getstdfilex)(int);

PFN_fgetws	pfnfgetws;
PFN_fputws	pfnfputws;
PFN_getstdfilex pgetstdfilex;
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
			pgetstdfilex = (PFN_getstdfilex)GetProcAddress (hCoreDLL, TEXT("_getstdfilex"));
			if ((pfnfputws == NULL) || (pfnfgetws == NULL) || (pgetstdfilex == NULL)) {
				RETAILMSG (1, (TEXT("Shell: Can't find fputws or fgetws or _getstdfilex.\r\n")));
				v_fUseConsole = FALSE;
			}
		}
	}
    if (!v_fUseConsole) {
        while((filedesc = UU_ropen (TEXT("con"), 0x10000)) == -1) {
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
    int ch;
    TCHAR *pch;

	if (v_fUseConsole) {
		return pfnfgetws (s, cch, pgetstdfilex(0));
	}

    pch = s;
    while ((ch = GetChar()) != EOF) 
    {
        if ((*pch++ = (TCHAR)ch) == TEXT('\r')) 
        {
            pch --;
            break;
        }
        if (pch - s == (cch - 1))
        {
            break;
        }
    }
    if (ch == EOF)
	return 0;
    *pch = TEXT('\0');

    return s;
} 



int
Puts (TCHAR *s)
{
#ifdef SERIAL_SHELL
    OEMWriteDebugString(s);
    return 1;
#else
    char szAscii [256];
    size_t  nChars;
    
    if (v_fUseConsole) {
        return pfnfputws (s, pgetstdfilex(1));
    }

    if ((filedesc != -1)
        && ((nChars = wcstombs (szAscii,s,256)) != (size_t)-1))
    {
        return UU_rwrite (filedesc, (unsigned char *)szAscii, nChars);
    }
        
    return EOF;
#endif
}
