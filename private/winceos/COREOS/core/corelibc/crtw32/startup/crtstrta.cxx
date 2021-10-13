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

#include <internal.h>
#include <windows.h>
#include <ctype.h>
#include <process.h>
#include <stdlib.h>

#if defined(_UNICODE)
//#define _tmain wmain
#define _targv wargv
#define __targv __wargv
#define crtstart_SkipWhite crtstart_SkipWhiteW
#define crtstart_SkipNonWhiteQuotes crtstart_SkipNonWhiteQuotesW
#define crtstart_RemoveQuotes crtstart_RemoveQuotesW
#define crtstart_CountSpace crtstart_CountSpaceW
#define crtstart_ParseArgsW crtstart_ParseArgsWW
#define _wcstotcs(d, s, n) wcslen(s)
#define _tmainCRTStartup wmainCRTStartup
#else
//#define _tmain main
#define _targv argv
#define __targv __argv
#define crtstart_SkipWhite crtstart_SkipWhiteA
#define crtstart_SkipNonWhiteQuotes crtstart_SkipNonWhiteQuotesA
#define crtstart_RemoveQuotes crtstart_RemoveQuotesA
#define crtstart_CountSpace crtstart_CountSpaceA
#define crtstart_ParseArgsW crtstart_ParseArgsWA
#define _wcstotcs(d, s, n) wcstombs(d, s, n)
#define _tmainCRTStartup mainCRTStartup
#endif

#define STANDARD_BUF_SIZE   256
#define BIG_BUF_SIZE        512

extern "C"
{

void
_tmainCRTStartup(
    HINSTANCE hInstance,
    HINSTANCE hInstancePrev,
    LPWSTR lpszCmdLine,
    int nCmdShow
    );
};

static HINSTANCE hInstGlobal;

static
void
crtstart_FreeArguments()
{
    if (__targv != NULL)
    {
        free(__targv);
    }
}


/*
 * String utility: count number of white spaces
 */
static
int
crtstart_SkipWhite(
    const TCHAR *lpString
    )
{
    const TCHAR *lpStringSav = lpString;

    while ((*lpString != _T('\0')) && _istspace(*lpString))
    {
        ++lpString;
    }

    return lpString - lpStringSav;
}


/*
 * String utility: count number of non-white spaces
 */
static
int
crtstart_SkipNonWhiteQuotes(
    const TCHAR *lpString
    )
{
    const TCHAR *lpStringSav = lpString;

    bool fInQuotes = false;

    while ((*lpString != _T('\0')) && (!_istspace(*lpString) || fInQuotes))
    {
        if (*lpString == _T('"'))
        {
            fInQuotes = !fInQuotes;
        }

        ++lpString;
    }

    return lpString - lpStringSav;
}


/*
 * String utility: remove quotes
 */
static
void
crtstart_RemoveQuotes(
    TCHAR *lpString
    )
{
    TCHAR *lpDest = lpString;

    while (*lpString != _T('\0'))
    {
        if (*lpString == _T('"'))
        {
            ++lpString;
        }
        else
        {
            *lpDest++ = *lpString++;
        }
    }

    *lpDest = L'\0';
}


/*
 *  String utility: count number characters and arguments
 */
static
void
crtstart_CountSpace(
    int &nargs,
    int &nchars,
    TCHAR const *lpCmdWalker
    )
{
    for (;;)
    {
        lpCmdWalker += crtstart_SkipWhite(lpCmdWalker);

        if (*lpCmdWalker == _T('\0'))
        {
            break;
        }

        int szThisString = crtstart_SkipNonWhiteQuotes(lpCmdWalker);

        lpCmdWalker += szThisString;
        nchars      += szThisString + 1;

        ++nargs;

        if (*lpCmdWalker == _T('\0'))
        {
            break;
        }
    }
}


/*
 * Parse argument stream into a data structure that looks like the following
 *
 * +- _targv
 * |
 * v 0    1         argc
 * +----+----+-...-+----+---------+-----+-...--+
 * |    |    |     |  0 |proc name|arg 1| args |
 * +----+----+-...-+----+---------+-----+-...--+
 *    \    \    \       ^         ^     ^
 *     \____\____\_____/         /     /
 *           \____\_____________/     /
 *                 \_________________/
 */
static
int
crtstart_ParseArgsW(
    wchar_t const * argv0,
    wchar_t const * lpCmdLine,
    int &argc,
    TCHAR ** &_targv
    )
{
    TCHAR const * pctCmdLine = NULL;
#ifndef _UNICODE
    char  * ptCmdLine = NULL;
    char    argv0A[2 * STANDARD_BUF_SIZE];
#endif
#pragma warning(suppress: 4996) // API declared __declspec(deprecated)
    int     iArgv0Len       = _wcstotcs(argv0A, argv0, _countof(argv0A)) + 1;
    int     nargs           = 1;
    int     nchars          = iArgv0Len;

    if (iArgv0Len == -1)
    {
        goto crtstart_ParseArgsW_Error;
    }

#ifndef _UNICODE
    int  sz    = (wcslen(lpCmdLine)+1) * sizeof(wchar_t);
    ptCmdLine = (char *)malloc(sz);
    if(ptCmdLine == 0)
    {
        goto crtstart_ParseArgsW_Error;
    }
#pragma warning(suppress: 4996) // API declared __declspec(deprecated)
    wcstombs(ptCmdLine, lpCmdLine, sz);
    pctCmdLine = ptCmdLine;
#else
    pctCmdLine = lpCmdLine;
#endif

    crtstart_CountSpace(nargs, nchars, pctCmdLine);

    void *pvArgBlock = malloc((nargs+1)*sizeof(TCHAR *) + nchars*sizeof(TCHAR));
    if (pvArgBlock == 0)
    {
#ifndef _UNICODE
        free(ptCmdLine);
#endif
        goto crtstart_ParseArgsW_Error;
    }
    TCHAR *pcTarget    = (TCHAR *)((TCHAR **)pvArgBlock + nargs + 1);
    TCHAR **ppcPointer = (TCHAR **)pvArgBlock;
    TCHAR const *lpCmdWalker = pctCmdLine;

#ifndef _UNICODE
    memcpy(pcTarget, argv0A, iArgv0Len * sizeof(TCHAR));
#else
    memcpy(pcTarget, argv0, iArgv0Len * sizeof(TCHAR));
#endif

    *ppcPointer = pcTarget;
    ++ppcPointer;

    pcTarget += iArgv0Len;

    for (;;)
    {
        lpCmdWalker += crtstart_SkipWhite(lpCmdWalker);

        if (*lpCmdWalker == _T('\0'))
        {
            break;
        }

        int szThisString = crtstart_SkipNonWhiteQuotes(lpCmdWalker);

        memcpy(pcTarget, lpCmdWalker, szThisString * sizeof(TCHAR));
        pcTarget[szThisString] = _T('\0');

        crtstart_RemoveQuotes(pcTarget);

        *ppcPointer = pcTarget;
        ++ppcPointer;

        pcTarget += szThisString + 1;
        lpCmdWalker += szThisString;

        if (*lpCmdWalker == _T('\0'))
        {
            break;
        }
    }

#ifndef _UNICODE
    free(ptCmdLine);
#endif

    argc          = nargs;
    _targv        = (TCHAR **)pvArgBlock;
    _targv[nargs] = NULL;

    return TRUE;

crtstart_ParseArgsW_Error:
    RETAILMSG (1, (L"Initialization error (%d)\r\n", GetLastError()));
    return FALSE;
}


/*
 *  Standard Win32 entry point for (w)main
 */
static
void
mainCRTStartupHelper(
    HINSTANCE hInstance,
    LPCWSTR lpszCmdLine
    )
{
    int retcode = -3;
    wchar_t argv0[STANDARD_BUF_SIZE];

    hInstGlobal = hInstance;

    __try
    {
        if (!GetModuleFileNameW(hInstGlobal, argv0, STANDARD_BUF_SIZE))
        {
            RETAILMSG(1, (L"Initialization error (%d)\r\n", GetLastError()));
            _exit(-3);
        }

        /*
         * Register cleanup routine first so it is runs absolutely last
         */
        atexit(&crtstart_FreeArguments);

        /*
         * Initialize C's data structures
         */
        _cinit(TRUE);

        int         argc;
        TCHAR **_targv;

        if (!crtstart_ParseArgsW(argv0, lpszCmdLine, argc, _targv))
        {
            _exit(-4);
        }

        __argc  = argc;
        __targv = _targv;

        retcode = _tmain(argc, _targv, NULL);

        /* call terminators and terminate current process */
        exit(retcode);
    }
    __except(_XcptFilter(retcode = GetExceptionCode(), GetExceptionInformation()))
    {
        /*
         * Should never reach here unless UnhandledExceptionFilter does
         * not pass the exception to the debugger
         */
        _exit(retcode);
    }
}


void
_tmainCRTStartup(
    HINSTANCE hInstance,
    HINSTANCE hInstancePrev,
    LPWSTR lpszCmdLine,
    int nCmdShow
    )
{
    /*
     * The /GS security cookie must be initialized before any exception
     * handling targeting the current image is registered.  No function
     * using exception handling can be called in the current image until
     * after __security_init_cookie has been called.
     */
    __security_init_cookie();

    mainCRTStartupHelper(hInstance, lpszCmdLine);
}

