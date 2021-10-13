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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <corecrt.h>

_PVFV *__onexitbegin;
_PVFV *__onexitend;

#define ONEXITSTART   4
#define ONEXITMAXINCR 512

LPCRITICAL_SECTION g_pcsExit;

static
void lockexit (void)
{
    if (!g_pcsExit) {
        // thread safe initialization of the critical section
        LPCRITICAL_SECTION pcs = (LPCRITICAL_SECTION) malloc (sizeof (CRITICAL_SECTION));
        if (pcs) {
            InitializeCriticalSection (pcs);
            if (InterlockedCompareExchangePointer (&g_pcsExit, pcs, NULL) != NULL) {
                // other threads intialized the cs already
                DeleteCriticalSection (pcs);
                free (pcs);
            }
        }
    }
    if (g_pcsExit) {
        EnterCriticalSection (g_pcsExit);
    }
}

static
void unlockexit (void)
{
    if (g_pcsExit) {
        LeaveCriticalSection (g_pcsExit);
    }
}

static
_onexit_t
_onexit_nolock(_onexit_t func)
{
    _PVFV * p = NULL;
    _PVFV * begin = __onexitbegin;
    _PVFV * end = __onexitend;
    ptrdiff_t olddiff;
    size_t oldsize = 0;
    size_t newsizemin = 0;
    size_t newsize;

    olddiff = (char*)end - (char*)begin;

    if (olddiff < 0)
    {
        return NULL;
    }

    newsizemin = (size_t)olddiff + sizeof(*begin);

    if (begin != NULL)
    {
        oldsize = _msize(begin);
    }

    /*
     * Make sure the table has room for a new entry
     */
    if (oldsize < newsizemin)
    {
        /*
         * Not enough room; try to grow the table
         */
        if (begin == NULL)
        {
            /*
             * Begin with ONEXITSTART entries
             */
            p = (_PVFV *)malloc(ONEXITSTART * sizeof(*begin));
        }
        else
        {
            /*
             * Attempt to double the size up to the maximum increment
             */
            newsize = oldsize * 2;
            if (oldsize > ONEXITMAXINCR)
            {
                newsize = oldsize + ONEXITMAXINCR;
            }
            if (newsize > oldsize)
            {
                p = realloc(begin, newsize);
            }
            if (p == NULL)
            {
                /*
                 * Attempt to allocate the minimum instead
                 */
                if (newsizemin > oldsize)
                {
                    p = realloc(begin, newsizemin);
                }
            }
        }

        if (p == NULL)
        {
            return NULL;
        }

        end = p + (end - begin);
        begin = p;
    }

    *(end++) = (_PVFV)func;

    __onexitend = end;
    __onexitbegin = begin;

    return func;
}

_onexit_t
_onexit(_onexit_t func)
{
    lockexit ();
    __try {
        func = _onexit_nolock (func);
    } __finally {
        unlockexit ();
    }
    return func;
}

int atexit(_PVFV func)
{
    return (_onexit((_onexit_t)func) ? 0 : -1);
}

