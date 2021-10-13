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
int main (int argc, char **argv);

void mainACRTStartup(HINSTANCE hInstance, HINSTANCE hInstancePrev, LPWSTR lpszCmdLine, int nCmdShow);
};

static HINSTANCE    hInstGlobal;

//
//  String utility: count number of white spaces
//
static int crtstart_SkipWhiteA (char *lpString) {
    char *lpStringSav = lpString;

    while ((*lpString != '\0') && isspace (*lpString))
        ++lpString;

    return lpString - lpStringSav;
}

//
//  String utility: count number of non-white spaces
//
static int crtstart_SkipNonWhiteQuotesA (char *lpString)
{
    char *lpStringSav = lpString;
    int  fInQuotes = FALSE;

    while ((*lpString != '\0') && ((! isspace (*lpString)) || fInQuotes)) {
        if (*lpString == '"')
            fInQuotes = ! fInQuotes;

        ++lpString;
    }

    return lpString - lpStringSav;
}

//
//  Count number of characters and arguments
//
static void crtstart_CountSpaceA (int &nargs, int &nchars, char *lpCmdWalker) {
    for ( ; ; ) {
        lpCmdWalker += crtstart_SkipWhiteA (lpCmdWalker);
        
        if (*lpCmdWalker == '\0')
            break;

        int szThisString = crtstart_SkipNonWhiteQuotesA (lpCmdWalker);
        
        lpCmdWalker += szThisString;
        nchars      += szThisString + 1;

        ++nargs;

        if (*lpCmdWalker == '\0')
            break;
    }
}

//
//  Parse argument stream
//
static int crtstart_ParseArgsWA (wchar_t *argv0, wchar_t *lpCmdLine, int &argc, char ** &argv) {
    char    argv0A [2 * STANDARD_BUF_SIZE];
    
    int     iArgv0Len       = wcstombs (argv0A, argv0, 2 * STANDARD_BUF_SIZE) + 1;
    int     nargs           = 1;
    int     nchars          = iArgv0Len;

    int     sz          = (wcslen(lpCmdLine) + 1) * sizeof(wchar_t);
    char    *lpCmdLineA = (char *)LocalAlloc (LMEM_FIXED, sz);
    if( lpCmdLineA == 0 ) {
        goto crtstart_ParseArgsWA_Error;
    }
    wcstombs (lpCmdLineA, lpCmdLine, sz);
    
    crtstart_CountSpaceA (nargs, nchars, lpCmdLineA);

    void    *pvArgBlock  = LocalAlloc (LMEM_FIXED, (nargs + 1) * sizeof (char *) + nchars * sizeof(char));
    if( pvArgBlock == 0 ) {
        LocalFree (lpCmdLineA);
        goto crtstart_ParseArgsWA_Error;
    }

    char    *pcTarget    = (char *)((char **)pvArgBlock + nargs + 1);
    char    **ppcPointer = (char **)pvArgBlock;
    char    *lpCmdWalker = lpCmdLineA;

    memcpy (pcTarget, argv0A, iArgv0Len * sizeof(char));
    
    *ppcPointer = pcTarget;
    ++ppcPointer;

    pcTarget   += iArgv0Len;

    for ( ; ; ) {
        lpCmdWalker += crtstart_SkipWhiteA (lpCmdWalker);
        
        if (*lpCmdWalker == '\0')
            break;

        int szThisString = crtstart_SkipNonWhiteQuotesA (lpCmdWalker);
        
        memcpy (pcTarget, lpCmdWalker, szThisString * sizeof(char));
        pcTarget[szThisString] = '\0';

        *ppcPointer = pcTarget;
        ++ppcPointer;
        
        pcTarget += szThisString + 1;
        lpCmdWalker += szThisString;

        if (*lpCmdWalker == '\0')
            break;
    }

    LocalFree (lpCmdLineA);

    argc        = nargs;
    argv        = (char **)pvArgBlock;
    argv[nargs] = NULL;

    return TRUE;

crtstart_ParseArgsWA_Error:
    RETAILMSG (1, (TEXT("Initialization error %d\r\n"), GetLastError()));
    return FALSE;
}

//
//  Standard Windows entry point for ASCII main
//
void mainACRTStartup(HINSTANCE hInstance, HINSTANCE hInstancePrev, LPWSTR lpszCmdLine, int nCmdShow) {
    hInstGlobal = hInstance;
    int retcode;

    _try {
        wchar_t argv0[STANDARD_BUF_SIZE];

        if (! GetModuleFileNameW (hInstGlobal, argv0, STANDARD_BUF_SIZE)) {
        RETAILMSG (1, (TEXT("Initialization error %d\r\n"), GetLastError()));
        return;
        }

        _cinit(); /* Initialize C's data structures */

        int     argc;
        char    **argv;

        if (! crtstart_ParseArgsWA (argv0, lpszCmdLine, argc, argv))
        return;

        __argc  = argc;
        __argv  = argv;
        __wargv = NULL;

        retcode = main (argc, argv);

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

