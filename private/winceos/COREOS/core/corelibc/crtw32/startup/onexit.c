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
#include <dbgint.h>
#include <malloc.h>

_PVFV *__onexitbegin;
_PVFV *__onexitend;

#define ONEXITSTART   4
#define ONEXITMAXINCR 512

#define _malloc_crt                     malloc
#define _realloc_crt                    realloc

/***
* _lockexit - Aquire the exit code lock
*
*Purpose:
*       Makes sure only one thread is in the exit code at a time.
*       If a thread is already in the exit code, it must be allowed
*       to continue.  All other threads must pend.
*
*       Notes:
*
*       (1) It is legal for a thread that already has the lock to
*       try and get it again(!).  That is, consider the following
*       sequence:
*
*           (a) program calls exit()
*           (b) thread locks exit code
*           (c) user onexit() routine calls _exit()
*           (d) same thread tries to lock exit code
*
*       Since _exit() must ALWAYS be able to work (i.e., can be called
*       from anywhere with no regard for locking), we must make sure the
*       program does not deadlock at step (d) above.
*
*       (2) If a thread executing exit() or _exit() aquires the exit lock,
*       other threads trying to get the lock will pend forever.  That is,
*       since exit() and _exit() terminate the process, there is not need
*       for them to unlock the exit code path.
*
*       (3) Note that onexit()/atexit() routines call _lockexit/_unlockexit
*       to protect mthread access to the onexit table.
*
*       (4) The 32-bit OS semaphore calls DO allow a single thread to acquire
*       the same lock multiple times* thus, this version is straight forward.
*
*Entry: <none>
*
*Exit:
*       Calling thread has exit code path locked on return.
*
*Exceptions:
*
*******************************************************************************/
static
void lockexit (void)
{
    _mlock(_EXIT_LOCK1);
}

/***
* _unlockexit - Release exit code lock
*
*Purpose:
*       [See _lockexit() description above.]
*
*       This routine is called by _cexit(), _c_exit(), and onexit()/atexit().
*       The exit() and _exit() routines never unlock the exit code path since
*       they are terminating the process.
*
*Entry:
*       Exit code path is unlocked.
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/
static
void unlockexit (void)
{
    _munlock(_EXIT_LOCK1);
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
        oldsize = _msize_crt(begin);
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
            p = (_PVFV *)_malloc_crt(ONEXITSTART * sizeof(*begin));
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
                p = _realloc_crt(begin, newsize);
            }
            if (p == NULL)
            {
                /*
                 * Attempt to allocate the minimum instead
                 */
                if (newsizemin > oldsize)
                {
                    p = _realloc_crt(begin, newsizemin);
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

