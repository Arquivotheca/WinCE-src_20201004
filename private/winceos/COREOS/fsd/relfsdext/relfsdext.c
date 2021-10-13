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
#include <windows.h>
#include <fsioctl.h>

/*
Shell Extension handling

Extension DLL needs to provide an entry point named "ParseCommand"

Function Signature Info:
#define SHELL_EXTENSION_FUNCTION    TEXT("ParseCommand")
#define TXTSHELL_REG                TEXT("Software\\Microsoft\\TxtShell")
typedef void (*PFN_FmtPuts)(TCHAR *s,...);
typedef TCHAR * (*PFN_Gets)(TCHAR *s, int cch);
typedef BOOL (*PFN_ParseCommand)(LPTSTR szCmd, LPTSTR szCmdLine, PFN_FmtPuts pfnFmtPuts, PFN_Gets pfnGets);

It can be loaded/unloaded via the "loadext" command in the shell.  Initial extensions can be specified in the registry as "FriendlyName"="DLLNAME.DLL" in HLM\Software\Microsoft\TxtShell

ParseCommand should return TRUE if it handled the current command, FALSE if it did not.  It will be passed the command "?" when the user enters "?" on the command line.
*/

typedef void (*PFN_FmtPuts)(TCHAR *s,...);
typedef TCHAR * (*PFN_Gets)(TCHAR *s, int cch);

BOOL ParseCommand(LPTSTR szCmd, LPTSTR szCmdLine, PFN_FmtPuts pfnFmtPuts, PFN_Gets pfnGets)
{
    BOOL    bRet = FALSE;
    if (!_tcscmp(szCmd, TEXT("loaddbglist")))
    {
        pfnFmtPuts(TEXT("\r\nLoading %s... "), szCmdLine ? szCmdLine : TEXT("dbglist.txt"));

        bRet = CeFsIoControl(TEXT("\\Release"), FSCTL_REFRESH_VOLUME, 
                             szCmdLine, szCmdLine ? (_tcslen(szCmdLine)+1)*sizeof(szCmdLine[0]) : 0, 
                             NULL, 0, NULL, NULL);

        if (bRet)
        {
            pfnFmtPuts(TEXT("SUCCEEDED\r\n"));
        }
        else
        {
            pfnFmtPuts(TEXT("FAILED!\r\n"));
        }
    }
    else if (szCmd[0] == TEXT('?'))
    {
        pfnFmtPuts(TEXT("\r\n**** RELFSD SHELL EXTENSION ****\r\n\r\n"));
        pfnFmtPuts(TEXT(" loaddbglist <file>\r\n"));
        pfnFmtPuts(TEXT("  - load Release Directory Modules from optional <file>\r\n"));
        pfnFmtPuts(TEXT("    if no <file> is specified, use dbglist.txt \r\n"));
        pfnFmtPuts(TEXT("\r\n"));

        bRet = TRUE;
    }

    return bRet;
}
