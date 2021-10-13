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
#include <corecrt.h>
#include <cruntime.h>

extern "C" {

extern _PVFV __xp_a[], __xp_z[];    /* C pre-terminators */
extern _PVFV __xt_a[], __xt_z[];    /* C terminators */

extern _PVFV *__onexitbegin;
extern _PVFV *__onexitend;

void _initterm(_PVFV *, _PVFV *);

char _exitflag;

static
void
doexit(
    int code,
    int quick,
    int retcaller
    )
{
    /* save callable exit flag (for use by terminators) */
    _exitflag = (char)retcaller; /* 0 = term, !0 = callable exit */

    if (!quick)
    {
        if (__onexitbegin)
        {
            /*
             * Note: do not cache the values of the global pointers
             *       across the call inside the loop because they
             *       can be changed by the call, e.g. if something
             *       called atexit with a function that calls atexit.
             */
            while (--__onexitend >= __onexitbegin)
            {
                if (*__onexitend)
                {
                    (**__onexitend)();
                }
            }

            free(__onexitbegin);
            __onexitbegin = __onexitend = NULL;         
        }

        _initterm(__xp_a, __xp_z);
    }

    _initterm(__xt_a, __xt_z);

    if (!retcaller)
    {
        __crtExitProcess(code);
    }
}

void
exit(int code)
{
    doexit(code, 0, 0); /* full term, kill process */
}

void
_exit(int code)
{
    doexit(code, 1, 0); /* quick term, kill process */
}

void
_cexit(void)
{
    doexit(0, 0, 1);    /* full term, return to caller */
}

void
_c_exit(void)
{
    doexit(0, 1, 1);    /* quick term, return to caller */
}

} // extern "C"

