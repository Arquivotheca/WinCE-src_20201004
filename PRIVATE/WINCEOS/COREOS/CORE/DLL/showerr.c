//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "windows.h"
#include "windev.h"

extern DWORD fIsDying;

// Dupe of structure in apis.c, change both or move to shared header file
typedef struct _ErrInfo {
    DWORD dwExcpCode;
    DWORD dwExcpAddr;
} ErrInfo, *PErrInfo;


_inline DWORD HexOf (DWORD dw)
{
    return dw + ((dw < 10)? '0' : ('A' - 10));
}

static int uiToHex (LPWSTR pOutStr, DWORD dw)
{
    int i;
    for (i = 0; i < 8; i ++) {
        pOutStr[i] = (WCHAR) HexOf ((dw >> (28 - i*4)) & 0xf);
    }
    return i;
}

static int SimplePrintf (LPWSTR pOutStr, int nMax, LPCWSTR pFmt, LPCWSTR pname, DWORD dwCode, DWORD dwAddr)
{
    // the format string is of the format "... %s ... %s ... %x ... %x...", where both %s
    // should be replaced with pname, and the 2 %x are replaced with dwCode and dwAddr.
    int   n = 0;
    int   nItm = 0;
    WCHAR ch;
    LPCWSTR p;
    DEBUGCHK (pOutStr && pFmt);
    if (!pname)
        pname = L"";
    while ((ch = *pFmt) && (n < nMax - 1)) {
        if (L'%' != ch) {
            pOutStr[n ++] = ch;
        } else {
            switch (nItm) {
            case 0:
            case 1:
                for (p = pname; *p; p ++)
                    pOutStr[n ++] = *p;
                // skip till 's'
                while (*++pFmt != 's')
                    ;
                break;
            case 2:
            case 3:
                n += uiToHex (pOutStr+n, (2 == nItm)? dwCode : dwAddr);
                // skip till 'x'
                while (((ch = *++pFmt) != 'x') && (ch != 'X'))
                    ;
                
                break;
            default:
                // eat it
                break;
            }
            nItm ++;
        }
        pFmt ++;
    }
    pOutStr[n] = 0;   // EOS
    return n;
}

DWORD WINAPI ShowErrThrd (LPVOID lpParam)
{
    PErrInfo pInfo = (PErrInfo) lpParam;
    DEBUGMSG (1, (L"Main thread in proc %8.8lx faulted, Excpetion code = %8.8lx, Exception Address = %8.8x!\r\n",
        GetCurrentProcessId(), pInfo->dwExcpCode, pInfo->dwExcpAddr));

    fIsDying = TRUE;
    if (!IsAPIReady(SH_WMGR)) {
        RETAILMSG(1,(L"Main thread in proc %8.8lx faulted, WMGR not on line!\r\n",GetCurrentProcessId()));

    } else {
        extern HANDLE hInstCoreDll;
        LPCWSTR pstr = (LPCWSTR) LoadString (hInstCoreDll,1, 0, 0);

        if (pstr) {
            LPCWSTR pname = GetProcName ();
            WCHAR bufx[512];
            ImmDisableIME (0);
            SimplePrintf (bufx, 512, pstr, pname, pInfo->dwExcpCode, pInfo->dwExcpAddr);
            if (pstr = (LPCWSTR) LoadString (hInstCoreDll, 2, 0, 0)) {
                MessageBox (0, bufx, pstr, MB_OK|MB_ICONEXCLAMATION|MB_TOPMOST);
            }
        }
        if (!pstr) {
            RETAILMSG (1, (L"Main thread in proc %8.8lx faulted, unable to load strings!\r\n", GetCurrentProcessId()));
        }
    }
    RETAILMSG (1, (L"Main thread in proc %8.8lx faulted - cleaning up!\r\n", GetCurrentProcessId()));
    return 0;
}
