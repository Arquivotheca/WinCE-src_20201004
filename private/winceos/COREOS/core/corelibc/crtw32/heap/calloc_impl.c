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
*calloc_impl.c - implementation of _calloc_impl
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the _calloc_impl() function.
*
*Revision History:
*       09-26-06  WL    Module created. Moved from calloc.c
*
*******************************************************************************/

#include <malloc.h>
#include <string.h>
#include <winheap.h>
#include <windows.h>
#include <internal.h>
#include <mtdll.h>
#include <dbgint.h>
#include <rtcsup.h>

void * __cdecl _calloc_impl (size_t num, size_t size, int * errno_tmp)
{
        size_t  size_orig;
        void *  pvReturn;

        /* ensure that (size * num) does not overflow */
        if (num > 0)
        {
            _VALIDATE_RETURN_NOEXC((_HEAP_MAXREQ / num) >= size, ENOMEM, NULL);
        }
        size_orig = size = size * num;

#ifdef  HEAPHOOK
        /* call heap hook if installed */
        if (_heaphook)
        {
            if ((*_heaphook)(_HEAP_CALLOC, size, NULL, (void *)&pvReturn))
                return pvReturn;
        }
#endif  /* HEAPHOOK */

        /* force nonzero size */
        if (size == 0)
            size = 1;

#ifdef  _POSIX_
        {
            void * retp = NULL;
            if ( size <= _HEAP_MAXREQ ) {
                retp = HeapAlloc( _crtheap,
                                  HEAP_ZERO_MEMORY,
                                  size );
            }
            return retp;
        }
#else
        for (;;)
        {
            pvReturn = NULL;

            if (size <= _HEAP_MAXREQ)
            {
                if (pvReturn == NULL)
                    pvReturn = HeapAlloc(_crtheap, HEAP_ZERO_MEMORY, size);
            }

            if (pvReturn || _newmode == 0)
            {
                RTCCALLBACK(_RTC_Allocate_hook, (pvReturn, size_orig, 0));
                if (pvReturn == NULL)
                {
                    if ( errno_tmp )
                        *errno_tmp = ENOMEM;
                }
                return pvReturn;
            }

            /* call installed new handler */
            if (!_callnewh(size))
            {
                if ( errno_tmp )
                    *errno_tmp = ENOMEM;
                return NULL;
            }

            /* new handler was successful -- try to allocate again */
        }

#endif  /* _POSIX_ */
}
