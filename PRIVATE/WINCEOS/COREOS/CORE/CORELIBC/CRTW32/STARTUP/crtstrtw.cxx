//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <ctype.h>
#include <tchar.h>
#include <stdlib.h>

extern "C" {
#include <corecrt.h>
    extern int  __argc;
    extern char **__argv;
    extern wchar_t  **__wargv;
};

#define STANDARD_BUF_SIZE   256
#define BIG_BUF_SIZE        512

extern "C" {
int wmain (int argc, wchar_t **wargv);
void mainWCRTStartup(HINSTANCE hInstance, HINSTANCE hInstancePrev, LPWSTR lpszCmdLine, int nCmdShow);
};

static HINSTANCE    hInstGlobal;

//
//  String utility: count number of white spaces
//
static int crtstart_SkipWhiteW (wchar_t *lpString) {
    wchar_t *lpStringSav = lpString;

    while ((*lpString != L'\0') && iswspace (*lpString))
        ++lpString;

    return lpString - lpStringSav;
}

//
//  String utility: count number of non-white spaces
//
static int crtstart_SkipNonWhiteQuotesW (wchar_t *lpString) {
    wchar_t *lpStringSav = lpString;

    int fInQuotes = FALSE;

    while ((*lpString != L'\0') && ((! iswspace (*lpString)) || fInQuotes)) {
        if (*lpString == L'"')
            fInQuotes = ! fInQuotes;

        ++lpString;
    }

    return lpString - lpStringSav;
}

//
//  Remove quotes
//
static void crtstart_RemoveQuotesW (wchar_t *lpString) {
    wchar_t *lpDest = lpString;

    while (*lpString != L'\0') {
        if (*lpString == L'"')
            ++lpString;
        else
            *lpDest++ = *lpString++;
    }

    *lpDest = L'\0';
}

//
//  Count number of characters and arguments
//
static void crtstart_CountSpaceW (int &nargs, int &nchars, wchar_t *lpCmdWalker) {
    for ( ; ; ) {
        lpCmdWalker += crtstart_SkipWhiteW (lpCmdWalker);
        
        if (*lpCmdWalker == L'\0')
            break;

        int szThisString = crtstart_SkipNonWhiteQuotesW (lpCmdWalker);
        
        lpCmdWalker += szThisString;
        nchars      += szThisString + 1;

        ++nargs;

        if (*lpCmdWalker == L'\0')
            break;
    }
}

//
//  Parse argument stream
//
static int crtstart_ParseArgsWW (wchar_t *argv0, wchar_t *lpCmdLine, int &argc, wchar_t ** &argv) {
    int     iArgv0Len       = wcslen (argv0) + 1;
    int     nargs           = 1;
    int     nchars          = iArgv0Len;

    crtstart_CountSpaceW (nargs, nchars, lpCmdLine);

    void    *pvArgBlock  = LocalAlloc (LMEM_FIXED, (nargs + 1) * sizeof (wchar_t *) + nchars * sizeof(wchar_t));
    if( pvArgBlock == 0 ) {
        RETAILMSG (1, (TEXT("Initialization error %d\r\n"), GetLastError()));
        return FALSE;
    }
    wchar_t *pcTarget    = (wchar_t *)((wchar_t **)pvArgBlock + nargs + 1);
    wchar_t **ppcPointer = (wchar_t **)pvArgBlock;
    wchar_t *lpCmdWalker = lpCmdLine;

    memcpy (pcTarget, argv0, iArgv0Len * sizeof(wchar_t));
    
    *ppcPointer = pcTarget;
    ++ppcPointer;

    pcTarget   += iArgv0Len;

    for ( ; ; ) {
        lpCmdWalker += crtstart_SkipWhiteW (lpCmdWalker);
        
        if (*lpCmdWalker == L'\0')
            break;

        int szThisString = crtstart_SkipNonWhiteQuotesW (lpCmdWalker);
        
        memcpy (pcTarget, lpCmdWalker, szThisString * sizeof(wchar_t));
        pcTarget[szThisString] = L'\0';

        crtstart_RemoveQuotesW (pcTarget);

        *ppcPointer = pcTarget;
        ++ppcPointer;
        
        pcTarget += szThisString + 1;
        lpCmdWalker += szThisString;

        if (*lpCmdWalker == L'\0')
            break;
    }

    argc        = nargs;
    argv        = (wchar_t **)pvArgBlock;
    argv[nargs] = NULL;

    return TRUE;
}

//
//  Standard Windows entry point for UNICODE main
//
void mainWCRTStartup(HINSTANCE hInstance, HINSTANCE hInstancePrev, LPWSTR lpszCmdLine, int nCmdShow) {
    hInstGlobal = hInstance;

    wchar_t argv0[STANDARD_BUF_SIZE];
    int retcode;

    _try {

        if (! GetModuleFileNameW (hInstGlobal, argv0, STANDARD_BUF_SIZE)) {
        RETAILMSG(1,  (TEXT("Initialization error %d\r\n"), GetLastError()));
        return;
        }

        _cinit(); /* Initialize C's data structures */

        int     argc;
        wchar_t **wargv;

        if (! crtstart_ParseArgsWW (argv0, lpszCmdLine, argc, wargv))
        return;

        __argc  = argc;
        __argv  = NULL;
        __wargv = wargv; 

        retcode = wmain (argc, wargv);

    }
    __except ( _XcptFilter(GetExceptionCode(), GetExceptionInformation()) ) {
             /*
             * Should never reach here unless UnHandled Exception Filter does
             * not pass the exception to the debugger
             */

        retcode = GetExceptionCode();

            exit(retcode);

    } /* end of try - except */

    exit(retcode);

}
