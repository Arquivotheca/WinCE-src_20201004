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
#include    <windows.h>
#include    <tchar.h>

#include    <katoex.h>

#include    "regdump.h"

#ifndef UNDER_CE
#include    <stdio.h>
#endif

#define	MAX_NAME_BUF_SIZE	256
#define	MAX_VALUE_BUF_SIZE	1024

static  void LogPrint(TCHAR *szFormat, ... )
{
    static  TCHAR   szLineBuf[MAX_VALUE_BUF_SIZE];
    static  DWORD   dwLineLen = 0;

	va_list valist;
    DWORD   dwNdx;

    /*
    *   Originally, there were only the following three lines in this function,
    *   but Kato appends cr=lf to each line, so we delay printing until
    *   cr-lf shows up and only then send the whole line.
    *   If our local buffer overfills, well, we try to recover, but the damage
    *   is already done.
    */

    // append to a local buffer
	va_start( valist, szFormat );
//  KatoLogV(KatoGetDefaultObject(),LOG_COMMENT,szFormat,valist);
    dwLineLen += _vstprintf_s(&szLineBuf[dwLineLen],MAX_VALUE_BUF_SIZE-dwLineLen,szFormat,valist);
	va_end( valist );

    // debugchk, are we off?
    if ( dwLineLen >= MAX_VALUE_BUF_SIZE )
    {
        dwLineLen = 0;
        szLineBuf[MAX_VALUE_BUF_SIZE-1] = 0;
        KatoLog(KatoGetDefaultObject(),LOG_COMMENT,szLineBuf);
        return;
    }

    // time to send this one out?
    for ( dwNdx=0; dwNdx < dwLineLen; dwNdx++ )
    {
        switch ( szLineBuf[dwNdx] )
        {
            case _T('\n'):
            case _T('\r'):
                KatoLog(KatoGetDefaultObject(),LOG_COMMENT,szLineBuf);
                dwLineLen = 0;
                break;
            default:
                break;
        }
    }

    // keep the line until you see CR-LF
    return;
}

void RegDumpKey(HKEY hTopParentKey, LPTSTR szChildKeyName, int level)
{
    TCHAR Name[MAX_NAME_BUF_SIZE];
    DWORD cbName = MAX_NAME_BUF_SIZE;
    HKEY hChildKey;
    BYTE lpData[MAX_VALUE_BUF_SIZE+2];
    int     i;
	LONG	lRet;
    HKEY    hKey = hTopParentKey;

    // Throw a pair of NULLS on end of string so _tprintfs don't run off the end of max size strings
    lpData[MAX_VALUE_BUF_SIZE] = lpData[MAX_VALUE_BUF_SIZE+1] = 0;

    for (i = 0; ERROR_SUCCESS == RegEnumKeyEx(hKey, i, Name, &cbName, NULL, NULL, NULL, NULL); i++)
    {
//        LogPrint(TEXT("[%s]\r\n"),Name);
		if ((lRet = RegOpenKeyEx(hKey, Name, 0, 0, &hChildKey)) != ERROR_SUCCESS)
		{
		    LogPrint(TEXT("Failed to open key %s, err code %u\r\n"), Name, lRet);
		}
		else
		{
            if ( ! _tcscmp(szChildKeyName, Name) )
            {
                LogPrint(TEXT("[%s]\r\n"),Name);
                DumpChild(hChildKey,1);
            }
            else
            {
		        RegDumpKey(hChildKey, szChildKeyName, level + 1);
		        RegCloseKey(hChildKey);
            }
		}

		cbName = MAX_NAME_BUF_SIZE;
    }
}

void DumpChild(HKEY hKey, int level)
{
    TCHAR Name[MAX_NAME_BUF_SIZE];
    DWORD cbName = MAX_NAME_BUF_SIZE;
    HKEY hChildKey;
    DWORD dwType; DWORD dwcbData;
    BYTE lpData[MAX_VALUE_BUF_SIZE+2];
    LPTSTR lpszString;
    int     i, j, k, x;
    UINT    n;
	LONG	lRet;

    // Throw a pair of NULLS on end of string so _tprintfs don't run off the end of max size strings
    lpData[MAX_VALUE_BUF_SIZE] = lpData[MAX_VALUE_BUF_SIZE+1] = 0;

    for (i = 0; ERROR_SUCCESS == RegEnumKeyEx(hKey, i, Name, &cbName, NULL, NULL, NULL, NULL); i++)
    {
		LogPrint( TEXT("<%04ld> "), i);
		for (j = 0; j < level; j++)
			LogPrint(TEXT("    "));

		LogPrint(TEXT("[%s]\r\n"), Name);

		if ((lRet = RegOpenKeyEx(hKey, Name, 0, 0, &hChildKey)) != ERROR_SUCCESS)
		{
		    LogPrint(TEXT("Failed to open key %s, err code %u\r\n"), Name, lRet);
		}
		else
		{
		    cbName = MAX_NAME_BUF_SIZE;
		    dwcbData = MAX_VALUE_BUF_SIZE;
		    for (k = 0; ERROR_SUCCESS == RegEnumValue(hChildKey, k, Name, &cbName, NULL, &dwType, lpData, &dwcbData); k++)
		    {
				LogPrint( TEXT("(%04ld) "), k);
				for (x = 0; x < level+1; x++)
        			LogPrint(TEXT("    "));

				switch(dwType) {
				    case REG_DWORD:
						LogPrint(TEXT("DWORD: %s = "), Name);
						_stprintf_s(Name, MAX_NAME_BUF_SIZE, TEXT("0x%lx"), *((LPDWORD)lpData));
						LogPrint(TEXT("%s"), Name);
						break;

				    case REG_SZ:
						LogPrint(TEXT("SZ: %s = %s"), Name, (LPTSTR)lpData);

						break;

				    case REG_MULTI_SZ:
                        LogPrint(TEXT("MULTI_SZ: %s"), Name);
                        lpszString = (LPTSTR)lpData;
                        while( 0 != *lpszString )
                        {
                            LogPrint(TEXT("\r\n"));
                            for (x = 0; x < level+2; x++)
                    			LogPrint(TEXT("    "));
                            LogPrint(TEXT("SZ: %s"), lpszString );
                            lpszString += _tcslen( lpszString ) + 1;
                        }
						break;

				    case REG_BINARY :
						LogPrint(TEXT("BINARY: %s ="), Name);
									for (n=0; n < (UINT)dwcbData; n++)
										LogPrint(TEXT(" 0x%lx"), *(lpData+n));
						break;

				    default:
						LogPrint(TEXT("ERROR: Unknown Type"));
				}

				LogPrint(TEXT("\r\n"));
				cbName = MAX_NAME_BUF_SIZE;
				dwcbData = MAX_VALUE_BUF_SIZE;
		    }

		    DumpChild(hChildKey, level + 1);
		    RegCloseKey(hChildKey);
		}

		cbName = MAX_NAME_BUF_SIZE;
    }
}


/*
#ifdef UNDER_CE
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nCmdShow)
#else
int _tmain(int argc, TCHAR *argv[]);
#endif
{

//	LogPrint(TEXT("HKEY_CLASSES_ROOT\r\n"));
//	DumpChild(HKEY_CLASSES_ROOT, 1);
//	LogPrint(TEXT("HKEY_CURRENT_USER\r\n"));
//	DumpChild(HKEY_CURRENT_USER, 1);
	hPPSH = PPSH_OpenStream( TEXT("REGDUMP.TXT"));
	LogPrint(TEXT("HKEY_LOCAL_MACHINE\r\n"));
	DumpChild(HKEY_LOCAL_MACHINE, 1);
//	LogPrint(TEXT("HKEY_USERS\r\n"));
//	DumpChild(HKEY_USERS, 1);

	LogPrint(TEXT("\r\n"));
	PPSH_CloseStream( hPPSH);

	return 0;
}
*/


