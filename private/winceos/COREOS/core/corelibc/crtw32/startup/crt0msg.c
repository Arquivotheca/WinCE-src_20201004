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
*crt0msg.c - startup error messages
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Prints out banner for runtime error messages.
*
*Revision History:
*       06-27-89  PHG   Module created, based on asm version
*       04-09-90  GJF   Added #include <cruntime.h>. Made calling type
*                       _CALLTYPE1. Also, fixed the copyright.
*       04-10-90  GJF   Fixed compiler warnings (-W3).
*       06-04-90  GJF   Revised to be more compatible with old scheme.
*                       nmsghdr.c merged in.
*       10-08-90  GJF   New-style function declarators.
*       10-11-90  GJF   Added _RT_ABORT, _RT_FLOAT, _RT_HEAP.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       02-04-91  SRW   Changed to call WriteFile (_WIN32_)
*       02-25-91  MHL   Adapt to ReadFile/WriteFile changes (_WIN32_)
*       04-10-91  PNT   Added _MAC_ conditional
*       09-09-91  GJF   Added _RT_ONEXIT error.
*       09-18-91  GJF   Added 3 math errors, also corrected comments for
*                       errors that were changed in rterr.h, cmsgs.h.
*       03-31-92  DJM   POSIX support.
*       10-23-92  GJF   Added _RT_PUREVIRT.
*       04-05-93  JWM   Added _GET_RTERRMSG().
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-29-93  GJF   Removed rterrs[] entries for _RT_STACK, _RT_INTDIV,
*                       _RT_NONCONT and _RT_INVALDISP.
*       09-06-94  CFW   Remove Cruiser support.
*       09-06-94  GJF   Revised to use MessageBox for GUI apps.
*       01-10-95  JCF   Remove __app_type and __error_mode check for _MAC_.
*       02-14-95  CFW   write -> _write, error messages to debug reporting.
*       02-15-95  CFW   Make all CRT message boxes look alike.
*       02-24-95  CFW   Use __crtMessageBoxA.
*       02-27-95  CFW   Change __crtMessageBoxA params.
*       03-07-95  GJF   Added _RT_STDIOINIT.
*       03-21-95  CFW   Add _CRT_ASSERT report type.
*       06-06-95  CFW   Remove _MB_SERVICE_NOTIFICATION.
*       06-19-95  CFW   Avoid STDIO calls.
*       06-20-95  GJF   Added _RT_LOWIOINIT.
*       04-23-96  GJF   Added _RT_HEAPINIT. Also, revised _NMSG_WRITE to
*                       allow for ioinit() having not been invoked.
*       05-05-97  GJF   Changed call to WriteFile, in _NMSG_WRITE, so that
*                       it does not reference _pioinfo. Also, a few cosmetic
*                       changes.
*       01-04-99  GJF   Changes for 64-bit size_t.
*       05-17-99  PML   Remove all Macintosh support.
*       01-24-99  PML   Fix buffer overrun in _NMSG_WRITE.
*       05-10-00  GB    Fix call of _CrtDbgReport for _RT_BANNER in _NMSG_WRITE
*       03-28-01  PML   Protect against GetModuleFileName overflow (vs7#231284)
*       12-07-01  BWT   Protect against NULL handle for stderr (ntbug: 504230)
*       06-05-02  BWT   Switch from alloca to static buffer for error message
*                       and remove POSIX.
*       09-30-02  GB    Added error message for unitialized crt calls.
*       10-09-04  MSL   Locale init error
*       01-14-05  AC    Prefast (espx) fixes
*       03-23-05  MSL   Buffer error fix
*       04-20-05  MSL   Better validation after GetStdHandle
*       05-13-05  MSL   Remove dead FORTRAN error handling
*       06-10-05  AC    Added error message for manifest check.
*       06-10-05  PML   Added _RT_COOKIE_INIT comment so R6035 clearly taken
*       06-17-08  GM    Removed Fusion.
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <rterr.h>
#include <cmsgs.h>
#include <awint.h>
#include <windows.h>
#include <dbgint.h>

/*
 * __app_type, together with __error_mode, determine how error messages
 * are written out.
 */
int __app_type = _UNKNOWN_APP;

/* struct used to lookup and access runtime error messages */

struct rterrmsgs {
        int rterrno;        /* error number */
        const wchar_t *rterrtxt;     /* text of error message */
};

/* runtime error messages */

static const struct rterrmsgs rterrs[] = {

        /* 2 */
        { _RT_FLOAT, _RT_FLOAT_TXT },

        /* 8 */
        { _RT_SPACEARG, _RT_SPACEARG_TXT },

        /* 9 */
        { _RT_SPACEENV, _RT_SPACEENV_TXT },

        /* 10 */
        { _RT_ABORT, _RT_ABORT_TXT },

        /* 16 */
        { _RT_THREAD, _RT_THREAD_TXT },

        /* 17 */
        { _RT_LOCK, _RT_LOCK_TXT },

        /* 18 */
        { _RT_HEAP, _RT_HEAP_TXT },

        /* 19 */
        { _RT_OPENCON, _RT_OPENCON_TXT },

        /* 22 */
        /* { _RT_NONCONT, _RT_NONCONT_TXT }, */

        /* 23 */
        /* { _RT_INVALDISP, _RT_INVALDISP_TXT }, */

        /* 24 */
        { _RT_ONEXIT, _RT_ONEXIT_TXT },

        /* 25 */
        { _RT_PUREVIRT, _RT_PUREVIRT_TXT },

        /* 26 */
        { _RT_STDIOINIT, _RT_STDIOINIT_TXT },

        /* 27 */
        { _RT_LOWIOINIT, _RT_LOWIOINIT_TXT },

        /* 28 */
        { _RT_HEAPINIT, _RT_HEAPINIT_TXT },

        ///* 29 */
        //{ _RT_BADCLRVERSION, _RT_BADCLRVERSION_TXT },

        /* 30 */
        { _RT_CRT_NOTINIT, _RT_CRT_NOTINIT_TXT },

        /* 31 */
        { _RT_CRT_INIT_CONFLICT, _RT_CRT_INIT_CONFLICT_TXT},

        /* 32 */
        { _RT_LOCALE, _RT_LOCALE_TXT},

        /* 33 */
        { _RT_CRT_INIT_MANAGED_CONFLICT, _RT_CRT_INIT_MANAGED_CONFLICT_TXT},

        /* 34 */
        { _RT_ONEXIT_VAR, _RT_ONEXIT_VAR_TXT },

        ///* 35 - not for _NMSG_WRITE, text passed directly to FatalAppExit */
        //{ _RT_COOKIE_INIT, _RT_COOKIE_INIT_TXT},

        /* 120 */
        { _RT_DOMAIN, _RT_DOMAIN_TXT },

        /* 121 */
        { _RT_SING, _RT_SING_TXT },

        /* 122 */
        { _RT_TLOSS, _RT_TLOSS_TXT },

        /* 252 */
        { _RT_CRNL, _RT_CRNL_TXT },

        /* 255 */
        { _RT_BANNER, _RT_BANNER_TXT }

};

/***
*_FF_MSGBANNER - writes out first part of run-time error messages
*
*Purpose:
*       This routine writes "\r\nrun-time error " to standard error.
*
*Entry:
*       No arguments.
*
*Exit:
*       Nothing returned.
*
*Exceptions:
*       None handled.
*
*******************************************************************************/

void __cdecl _FF_MSGBANNER (
        void
        )
{

        if ( (_set_error_mode(_REPORT_ERRMODE) == _OUT_TO_STDERR) ||
             ((_set_error_mode(_REPORT_ERRMODE) == _OUT_TO_DEFAULT) &&
              (__app_type == _CONSOLE_APP)) )
        {
            _NMSG_WRITE(_RT_CRNL);  /* new line to begin error message */
            _NMSG_WRITE(_RT_BANNER); /* run-time error message banner */
        }
}

/***
*_GET_RTERRMSG(message) - returns ptr to error text for given runtime error
*
*Purpose:
*       This routine returns the message associated with rterrnum
*
*Entry:
*       int rterrnum - runtime error number
*
*Exit:
*       no return value
*
*Exceptions:
*       none
*
*******************************************************************************/

const wchar_t * __cdecl _GET_RTERRMSG (
        int rterrnum
        )
{
        int i;

        for ( i = 0 ; i < _countof(rterrs); i++ )
            if ( rterrnum == rterrs[i].rterrno )
                return rterrs[i].rterrtxt;

        return NULL;
}

/***
*_NMSG_WRITE(message) - write a given message to handle 2 (stderr)
*
*Purpose:
*       This routine writes the message associated with rterrnum
*       to stderr.
*
*Entry:
*       int rterrnum - runtime error number
*
*Exit:
*       no return value
*
*Exceptions:
*       none
*
*******************************************************************************/

void __cdecl _NMSG_WRITE (
        int rterrnum
        )
{
#if defined(_WIN32_WCE) && !defined(_WCE_FULLCRT)
        return;
#else
        const wchar_t * const error_text = _GET_RTERRMSG(rterrnum);

        if (error_text)
        {
            int msgshown = 0;
#ifdef  _DEBUG
            /*
             * Report error.
             *
             * If _CRT_ERROR has _CRTDBG_REPORT_WNDW on, and user chooses
             * "Retry", call the debugger.
             *
             * Otherwise, continue execution.
             *
             */

            if (rterrnum != _RT_CRNL && rterrnum != _RT_BANNER && rterrnum != _RT_CRT_NOTINIT)
            {
                switch (_CrtDbgReportW(_CRT_ERROR, NULL, 0, NULL, L"%s", error_text))
                {
                case 1: _CrtDbgBreak(); msgshown = 1; break;
                case 0: msgshown = 1; break;
                }
            }
#endif
            if (!msgshown)
            {          
                if ( (_set_error_mode(_REPORT_ERRMODE) == _OUT_TO_STDERR) ||
                     ((_set_error_mode(_REPORT_ERRMODE) == _OUT_TO_DEFAULT) &&
                      (__app_type == _CONSOLE_APP)) )
                {
                    HANDLE hStdErr = INVALID_HANDLE_VALUE;
#ifndef _WIN32_WCE
                    hStdErr = GetStdHandle(STD_ERROR_HANDLE);
#else
                    int fh = STD_ERROR_HANDLE;
                    
                    _CHECK_IO_INIT_NOERRNO();
                    _lock_fh(fh);
                    if (_osfhnd(fh) == (intptr_t)INVALID_HANDLE_VALUE) {
                        BOOL is_dev = TRUE;
                        _osfhnd(fh) = hStdErr = OpenStdConsole(fh, &is_dev);
                        if (!is_dev)
                            _osfile(fh) &= (char)~FDEV;
                    } else {
                        hStdErr = _osfhnd(fh);
                    }
                    _unlock_fh(fh);
#endif


                    if (hStdErr && hStdErr != INVALID_HANDLE_VALUE)
                    {
                        /*
                         * This purposely truncates wchar_t to char.
                         * error_text doesn't contain the application's name.
                         * Currently, error_text contains only ASCII,
                         * so this truncation preserves VC9's behavior.
                         * If error_text is ever localized, this will have to be changed.
                         *
                         */

                        char buffer[500];
                        int i;
                        DWORD bytes_written;

                        for (i = 0; i < _countof(buffer); ++i)
                        {
                            buffer[i] = (char) error_text[i];

                            if (error_text[i] == L'\0')
                            {
                                break;
                            }
                        }

                        buffer[_countof(buffer) - 1] = '\0';

                        WriteFile( hStdErr,
                                      buffer,
                                      (unsigned long)strlen(buffer),
                                      &bytes_written,
                                      NULL );
                    }
                }
                else if (rterrnum != _RT_CRNL)
                {
                    #define MSGTEXTPREFIX L"Runtime Error!\n\nProgram: "
                    static wchar_t outmsg[sizeof(MSGTEXTPREFIX) / sizeof(wchar_t) + _MAX_PATH + 2 + 500];
                        // runtime error msg + progname + 2 newline + runtime error text.
                    wchar_t * progname = &outmsg[sizeof(MSGTEXTPREFIX) / sizeof(wchar_t) - 1];
                    size_t progname_size = _countof(outmsg) - (progname - outmsg);
                    wchar_t * pch = progname;

                    _ERRCHECK(wcscpy_s(outmsg, _countof(outmsg), MSGTEXTPREFIX));

                    progname[MAX_PATH] = L'\0';
                    if (!GetModuleFileNameW(NULL, progname, MAX_PATH))
                        _ERRCHECK(wcscpy_s(progname, progname_size, L"<program name unknown>"));

                    #define MAXLINELEN 60
                    if (wcslen(pch) + 1 > MAXLINELEN)
                    {
                        pch += wcslen(progname) + 1 - MAXLINELEN;
                        _ERRCHECK(wcsncpy_s(pch, progname_size - (pch - progname), L"...", 3));
                    }

                    _ERRCHECK(wcscat_s(outmsg, _countof(outmsg), L"\n\n"));
                    _ERRCHECK(wcscat_s(outmsg, _countof(outmsg), error_text));

                    __crtMessageBoxW(outmsg,
                            L"Microsoft Visual C++ Runtime Library",
#ifdef _WIN32_WCE // CE doesn't support TASKMODAL, so use the MB_APPLMODAL as default button1
                            MB_OK|MB_ICONHAND|MB_SETFOREGROUND|MB_APPLMODAL);
#else
                            MB_OK|MB_ICONHAND|MB_SETFOREGROUND|MB_TASKMODAL);
#endif

                }
            }
        }
#endif // defined(_WIN32_WCE) && !defined(_WCE_FULLCRT)
}
