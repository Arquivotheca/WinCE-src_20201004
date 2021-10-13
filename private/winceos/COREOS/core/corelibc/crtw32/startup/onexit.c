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
#include <corecrt.h>

_PVFV *__onexitbegin;
_PVFV *__onexitend;

#define ONEXITSTART   4
#define ONEXITMAXINCR 512

_onexit_t
_onexit(_onexit_t func)
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

int atexit(_PVFV func)
{
    return (_onexit((_onexit_t)func) ? 0 : -1);
}

