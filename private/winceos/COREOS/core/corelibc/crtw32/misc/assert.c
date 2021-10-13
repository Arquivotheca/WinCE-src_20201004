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
/***
*assert.c - Display a message and abort
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*Revision History:
*       05-19-88  JCR   Module created.
*       08-10-88  PHG   Corrected copyright date
*       03-14-90  GJF   Replaced _LOAD_DS with _CALLTYPE1 and added #include
*                       <cruntime.h>. Also, fixed the copyright.
*       04-05-90  GJF   Added #include <assert.h>
*       10-04-90  GJF   New-style function declarator.
*       06-19-91  GJF   Conditionally use setvbuf() on stderr to prevent
*                       the implicit call to malloc() if stderr is being used
*                       for the first time (assert() should work even if the
*                       heap is trashed).
*       01-25-92  RID   Mac module created from x86 version
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  GJF   Substantially revised to use MessageBox for GUI apps.
*       02-15-95  CFW   Make all CRT message boxes look alike.
*       02-16-95  JWM   Spliced _WIN32 & Mac versions.
*       02-24-95  CFW   Use __crtMessageBoxA.
*       02-27-95  CFW   Change debug break scheme, change __crtMBoxA params.
*       03-29-95  BWT   Fix posix build by adding _exit prototype.
*       06-06-95  CFW   Remove _MB_SERVICE_NOTIFICATION.
*       10-17-96  GJF   Thou shalt not scribble on the caller's filename 
*                       string! Also, fixed miscount of double newline.
*       05-17-99  PML   Remove all Macintosh support.
*       10-20-99  GB    Fix dotdotdot for filename. VS7#4731   
*       03-28-01  PML   Protect against GetModuleFileName overflow (vs7#231284)
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       08-28-03  SJ    Added _wassert, CrtSetReportHookW2,CrtDbgReportW, 
*                       & other helper functions. VSWhidbey#55308
*       04-20-05  MSL   Better validation after GetStdHandle
*       11-08-06  PMB   Remove support for Win9x and _osplatform and relatives.
*                       DDB#11325
*
*******************************************************************************/

#include <cruntime.h>
#include <windows.h>
#include <file2.h>
#include <internal.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <string.h>
#include <signal.h>
#include <awint.h>
#include <limits.h>
#include <dbgint.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#define _ASSERT_OK
#include <assert.h>

extern IMAGE_DOS_HEADER __ImageBase;

#ifdef _POSIX_
_CRTIMP void   __cdecl _exit(int);
#endif

/*
 * assertion format string for use with output to stderr
 */
static TCHAR _assertstring[] = _T("Assertion failed: %s, file %s, line %d\n");

/*      Format of MessageBox for assertions:
*
*       ================= Microsft Visual C++ Debug Library ================
*
*       Assertion Failed!
*
*       Program: c:\test\mytest\foo.exe
*       File: c:\test\mytest\bar.c
*       Line: 69
*
*       Expression: <expression>
*
*       For information on how your program can cause an assertion
*       failure, see the Visual C++ documentation on asserts
*
*       (Press Retry to debug the application - JIT must be enabled)
*
*       ===================================================================
*/

/*
 * assertion string components for message box
 */
#define BOXINTRO    _T("Assertion failed!")
#define PROGINTRO   _T("Program: ")
#define FILEINTRO   _T("File: ")
#define LINEINTRO   _T("Line: ")
#define EXPRINTRO   _T("Expression: ")
#define INFOINTRO   _T("For information on how your program can cause an assertion\n") \
                    _T("failure, see the Visual C++ documentation on asserts")
#define HELPINTRO   _T("(Press Retry to debug the application - JIT must be enabled)")

static TCHAR * dotdotdot = _T("...");
static TCHAR * newline = _T("\n");
static TCHAR * dblnewline = _T("\n\n");

#define DOTDOTDOTSZ 3
#define NEWLINESZ   1
#define DBLNEWLINESZ   2

#define MAXLINELEN  64 /* max length for line in message box */
#define ASSERTBUFSZ (MAXLINELEN * 9) /* 9 lines in message box */

#ifdef _UNICODE
void __cdecl _assert (const char *, const char *,unsigned);
#endif /* ifndef _UNICODE */

#if defined(_WIN32_WCE) && !defined(_UNICODE)
/*
 * CE needs wrappers for OutputDebugStringA and GetModuleFileNameA
 */

#undef OutputDebugString
#define OutputDebugString(string) NKDbgPrintfW(L"%a", string)

DWORD __GetModuleFileName(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
    /*
     * We do a best effort to convert from Wide char after GetModuleFileNameW.
     */
    int     ret;
    wchar_t lpwFilename[MAX_PATH];

    if (nSize > MAX_PATH)
        return 0;

    if ((ret = GetModuleFileNameW(hModule, lpwFilename, nSize)) == 0) {
        return 0;
    }

    if (wcstombs_s(&ret, lpFilename, nSize, lpwFilename, _TRUNCATE) != 0) {
        return 0;
    }
    
    return ret;
}

#undef GetModuleFileName
#define GetModuleFileName __GetModuleFileName

#endif // defined(_WIN32_WCE) && !defined(_UNICODE)

/***
*_assert() - Display a message and abort
*
*Purpose:
*       The assert macro calls this routine if the assert expression is
*       true.  By placing the assert code in a subroutine instead of within
*       the body of the macro, programs that call assert multiple times will
*       save space.
*
*Entry:
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

#ifdef _UNICODE
void __cdecl _wassert (
        const wchar_t *expr,
        const wchar_t *filename,
        unsigned lineno
        )
#else
void __cdecl _assert (

        const char *expr,
        const char *filename,
        unsigned lineno
        )
#endif        
{
        /*
         * Build the assertion message, then write it out. The exact form
         * depends on whether it is to be written out via stderr or the
         * MessageBox API.
         */

        if ( (_set_error_mode(_REPORT_ERRMODE)== _OUT_TO_STDERR) ||
             ((_set_error_mode(_REPORT_ERRMODE) == _OUT_TO_DEFAULT) &&
              (__app_type == _CONSOLE_APP)) )
        {
#ifdef _UNICODE
            {
#ifndef _WIN32_WCE //For CE, assert message can be printed by the _ftprintf() below this block.
                TCHAR assertbuf[ASSERTBUFSZ];
                HANDLE hErr ;
                DWORD written;

                hErr = GetStdHandle(STD_ERROR_HANDLE);

                if(hErr!=INVALID_HANDLE_VALUE && hErr!=NULL)
                {
                    if(swprintf(assertbuf, ASSERTBUFSZ,_assertstring,expr,filename,lineno) >= 0)
                    {
                        if(GetFileType(hErr) == FILE_TYPE_CHAR)
                        {
                            if(WriteConsoleW(hErr, assertbuf, (unsigned long)wcslen(assertbuf), &written, NULL))
                            {
                                abort();
                            }
                        }
                    }
                }
#endif //_WIN32_WCE
            }

#endif // _UNICODE

            /*
             * Build message and write it out to stderr. It will be of the
             * form:
             *        Assertion failed: <expr>, file <filename>, line <lineno>
             */
            if ( !anybuf(stderr) )
            /*
             * stderr is unused, hence unbuffered, as yet. set it to
             * single character buffering (to avoid a malloc() of a
             * stream buffer).
             */
             (void) setvbuf(stderr, NULL, _IONBF, 0);


            _ftprintf(stderr, _assertstring, expr, filename, lineno);
            fflush(stderr);
           
        }
        /* Display the message using a message box */
        else
        {
            
            TCHAR * pch;
            TCHAR assertbuf[ASSERTBUFSZ];
            TCHAR progname[MAX_PATH + 1];
            HMODULE hModule = NULL;

            /*
             * Line 1: box intro line
             */
            _ERRCHECK(_tcscpy_s( assertbuf, ASSERTBUFSZ, BOXINTRO ));
            _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, dblnewline ));

            /*
             * Line 2: program line
             */
            _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, PROGINTRO ));

#ifdef _WIN32_WCE
            // CE doesn't support GetModuleHandleEx
            hModule = NULL; 
#else
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) _ReturnAddress(), &hModule))
            {
                hModule = NULL;
             }
#ifdef CRTDLL
            else if (hModule == (HMODULE)&__ImageBase)
            {
                // This is CRT DLL.  Use the EXE instead
                hModule = NULL;
            }
#endif  /* CRTDLL */
#endif // _WIN32_WCE

            progname[MAX_PATH] = _T('\0');
            if ( !GetModuleFileName(hModule, progname, MAX_PATH))
                _ERRCHECK(_tcscpy_s( progname, MAX_PATH + 1, _T("<program name unknown>")));

            pch = (TCHAR *)progname;

            /* sizeof(PROGINTRO) includes the NULL terminator */
            if ( (sizeof(PROGINTRO)/sizeof(TCHAR)) + _tcslen(progname) + NEWLINESZ > MAXLINELEN )
            {
                pch += ((sizeof(PROGINTRO)/sizeof(TCHAR)) + _tcslen(progname) + NEWLINESZ) - MAXLINELEN;
                /* Only replace first (sizeof(TCHAR) * DOTDOTDOTSZ) bytes to ellipsis */
                _ERRCHECK(memcpy_s(pch, sizeof(TCHAR) * ((MAX_PATH + 1) - (pch - progname)), dotdotdot, sizeof(TCHAR) * DOTDOTDOTSZ ));

            }

            _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, pch ));
            _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, newline ));

            /*
             * Line 3: file line
             */
            _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, FILEINTRO ));

            /* sizeof(FILEINTRO) includes the NULL terminator */
            if ( (sizeof(FILEINTRO)/sizeof(TCHAR)) + _tcslen(filename) + NEWLINESZ > MAXLINELEN )
            {
                size_t p, len, ffn;

                pch = (TCHAR *) filename;
                ffn = MAXLINELEN - (sizeof(FILEINTRO)/sizeof(TCHAR)) - NEWLINESZ;

                for ( len = _tcslen(filename), p = 1;
                      pch[len - p] != _T('\\') && pch[len - p] != _T('/') && p < len;
                      p++ );

                /* keeping pathname almost 2/3rd of full filename and rest
                 * is filename
                 */
                if ( (ffn - ffn/3) < (len - p) && ffn/3 > p )
                {
                    /* too long. using first part of path and the 
                       filename string */
                    _ERRCHECK(_tcsncat_s( assertbuf, ASSERTBUFSZ, pch, ffn - DOTDOTDOTSZ - p ));
                    _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, dotdotdot ));
                    _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, pch + len - p ));
                }
                else if ( ffn - ffn/3 > len - p )
                {
                    /* pathname is smaller. keeping full pathname and putting
                     * dotdotdot in the middle of filename
                     */
                    p = p/2;
                    _ERRCHECK(_tcsncat_s( assertbuf, ASSERTBUFSZ, pch, ffn - DOTDOTDOTSZ - p ));
                    _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, dotdotdot ));
                    _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, pch + len - p ));
                }
                else
                {
                    /* both are long. using first part of path. using first and
                     * last part of filename.
                     */
                    _ERRCHECK(_tcsncat_s( assertbuf, ASSERTBUFSZ, pch, ffn - ffn/3 - DOTDOTDOTSZ ));
                    _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, dotdotdot ));
                    _ERRCHECK(_tcsncat_s( assertbuf, ASSERTBUFSZ, pch + len - p, ffn/6 - 1 ));
                    _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, dotdotdot ));
                    _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, pch + len - (ffn/3 - ffn/6 - 2) ));
                }

            }
            else
                /* plenty of room on the line, just append the filename */
                _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, filename ));

            _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, newline ));

            /*
             * Line 4: line line
             */
            _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, LINEINTRO ));
            _ERRCHECK(_itot_s( lineno, assertbuf + _tcslen(assertbuf), ASSERTBUFSZ - _tcslen(assertbuf), 10 ));
            _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, dblnewline ));

            /*
             * Line 5: message line
             */
            _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, EXPRINTRO ));

            /* sizeof(HELPINTRO) includes the NULL terminator */

            if (    _tcslen(assertbuf) +
                    _tcslen(expr) +
                    2*DBLNEWLINESZ +
                    (sizeof(INFOINTRO)/sizeof(TCHAR)) - 1 +
                    (sizeof(HELPINTRO)/sizeof(TCHAR)) > ASSERTBUFSZ )
            {
                _ERRCHECK(_tcsncat_s( assertbuf, ASSERTBUFSZ, expr,
                    ASSERTBUFSZ -
                    (_tcslen(assertbuf) +
                    DOTDOTDOTSZ +
                    2*DBLNEWLINESZ +
                    (sizeof(INFOINTRO)/sizeof(TCHAR))-1 +
                    (sizeof(HELPINTRO)/sizeof(TCHAR)))));
                _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, dotdotdot ));
            }
            else
                _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, expr ));

            _ERRCHECK(_tcscat_s( assertbuf, ASSERTBUFSZ, dblnewline ));

            /*
             * Line 6, 7: info line
             */

            _ERRCHECK(_tcscat_s(assertbuf, ASSERTBUFSZ, INFOINTRO));
            _ERRCHECK(_tcscat_s(assertbuf, ASSERTBUFSZ, dblnewline ));

            /*
             * Line 8: help line
             */
            _ERRCHECK(_tcscat_s(assertbuf, ASSERTBUFSZ, HELPINTRO));

#ifndef _WIN32_WCE
            /*
             * Write out via MessageBox
             */

            int nCode = __crtMessageBox(assertbuf,
                _T("Microsoft Visual C++ Runtime Library"),
                MB_TASKMODAL|MB_ICONHAND|MB_ABORTRETRYIGNORE|MB_SETFOREGROUND);


            /* Abort: abort the program */
            if (nCode == IDABORT)
            {
                /* raise abort signal */
                raise(SIGABRT);

                /* We usually won't get here, but it's possible that
                   SIGABRT was ignored.  So exit the program anyway. */

                _exit(3);
            }

            /* Retry: call the debugger */
            if (nCode == IDRETRY)
            {
                __debugbreak();
                /* return to user code */
                return;
            }

            /* Ignore: continue execution */
            if (nCode == IDIGNORE)
                return;
#else
            /*
             * Print the assert message
             */
            OutputDebugString(assertbuf);
#endif // _WIN32_WCE
        }

        abort();
}
