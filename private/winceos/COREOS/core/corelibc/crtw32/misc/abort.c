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
*abort.c - abort a program by raising SIGABRT
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines abort() - print a message and raise SIGABRT.
*
*Revision History:
*       06-30-89  PHG   module created, based on asm version
*       03-13-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and fixed the copyright. Also, cleaned
*                       up the formatting a bit.
*       07-26-90  SBM   Removed bogus leading underscore from _NMSG_WRITE
*       10-04-90  GJF   New-style function declarator.
*       10-11-90  GJF   Now does raise(SIGABRT). Also changed _NMSG_WRITE()
*                       interface.
*       04-10-91  PNT   Added _MAC_ conditional
*       08-26-92  GJF   Include unistd.h for POSIX build.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       01-05-94  CFW   Removed _MAC_ conditional.
*       03-29-95  BWT   Include stdio.h for POSIX to get fflush prototype.
*       08-25-03  AC    Added support for Watson.
*       04-05-04  AC    Improved how we invoke Watson. The code to fill up 
*                       the EXCEPTION_RECORD comes from XP SP2 codebase.
*                       VSW#172535
*       04-29-08  HG    Remove abort message and refactor fault reporting
*
*******************************************************************************/

#include <cruntime.h>
#ifdef  _POSIX_
#include <unistd.h>
#include <stdio.h>
#endif
#include <stdlib.h>
#include <internal.h>
#include <awint.h>
#include <rterr.h>
#include <signal.h>
#include <oscalls.h>
#include <mtdll.h>
#include <dbgint.h>

#ifdef _DEBUG
#define _INIT_ABORT_BEHAVIOR _WRITE_ABORT_MSG
#else
#define _INIT_ABORT_BEHAVIOR _CALL_REPORTFAULT
#endif

unsigned int __abort_behavior = _INIT_ABORT_BEHAVIOR;

/***
*void abort() - abort the current program by raising SIGABRT
*
*Purpose:
*   print out an abort message and raise the SIGABRT signal.  If the user
*   hasn't defined an abort handler routine, terminate the program
*   with exit status of 3 without cleaning up.
*
*   Multi-thread version does not raise SIGABRT -- this isn't supported
*   under multi-thread.
*
*Entry:
*   None.
*
*Exit:
*   Does not return.
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

void __cdecl abort (
        void
        )
{ 
#ifndef _POSIX_
    _PHNDLR sigabrt_act = SIG_DFL;
#endif

#ifdef _DEBUG
    if (__abort_behavior & _WRITE_ABORT_MSG)
    {
        /* write the abort message */
        _NMSG_WRITE(_RT_ABORT);
    }
#endif

#ifdef _POSIX_

    {
        sigset_t set;

        fflush(NULL);

        signal(SIGABRT, SIG_DFL);

        sigemptyset(&set);
        sigaddset(&set, SIGABRT);
        sigprocmask(SIG_UNBLOCK, &set, NULL);
    }

#else

    /* Check if the user installed a handler for SIGABRT.
     * We need to read the user handler atomically in the case
     * another thread is aborting while we change the signal
     * handler.
     */
    sigabrt_act = __get_sigabrt();
    if (sigabrt_act != SIG_DFL)
    {
        raise(SIGABRT);
    }

    /* If there is no user handler for SIGABRT or if the user
     * handler returns, then exit from the program anyway
     */
#ifndef _WIN32_WCE  // CE doesn't support __fastfail
    if (__abort_behavior & _CALL_REPORTFAULT)
    {
#if defined(_M_ARM)
        __fastfail(FAST_FAIL_FATAL_APP_EXIT);
#else
        if (IsProcessorFeaturePresent(PF_FASTFAIL_AVAILABLE))
            __fastfail(FAST_FAIL_FATAL_APP_EXIT);

        _call_reportfault(_CRT_DEBUGGER_ABORT, STATUS_FATAL_APP_EXIT, EXCEPTION_NONCONTINUABLE);
#endif
    }
#elif defined(_M_IX86)
    if (__abort_behavior & _CALL_REPORTFAULT)
    {
        _call_reportfault(_CRT_DEBUGGER_ABORT, STATUS_FATAL_APP_EXIT, EXCEPTION_NONCONTINUABLE);
    }
#endif // _WIN32_WCE

#endif  /* _POSIX_ */

    /* If we don't want to call ReportFault, then we call _exit(3), which is the
     * same as invoking the default handler for SIGABRT
     */

#ifdef _POSIX_
    /* SIGABRT was ignored or handled, and the handler returned
    normally.  We need open streams to be flushed here. */

    exit(3);
#else   /* not _POSIX_ */

    _exit(3);
#endif  /* _POSIX */
}

/***
*unsigned int _set_abort_behavior(unsigned int, unsigned int) - set the behavior on abort
*
*Purpose:
*
*Entry:
*   unsigned int flags - the flags we want to set
*   unsigned int mask - mask the flag values
*
*Exit:
*   Return the old behavior flags
*
*Exceptions:
*   None
*
*******************************************************************************/

unsigned int __cdecl _set_abort_behavior(unsigned int flags, unsigned int mask)
{
    unsigned int oldflags = __abort_behavior;
    __abort_behavior = oldflags & (~mask) | flags & mask;
    return oldflags;
}
