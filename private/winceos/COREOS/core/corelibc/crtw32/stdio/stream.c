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
*stream.c - find a stream not in use
*
*
*Purpose:
*   defines _getstream() - find a stream not in use
*
*******************************************************************************/


#include <cruntime.h>
#include <windows.h>
#include <malloc.h>
#include <file2.h>
#include <internal.h>
#include <stdio.h>
#include <mtdll.h>
#include <dbgint.h>

/***
*FILEX *_getstream() - find a stream not in use
*
*Purpose:
*   Find a stream not in use and make it available to caller. Intended
*   for use inside library only
*
*Entry:
*   None. Scans __piob[]
*
*Exit:
*   Returns a pointer to a free stream, or NULL if all are in use.  A
*   stream becomes allocated if the caller decided to use it by setting
*   any r, w, r/w mode.
*
*   [Multi-thread note: If a free stream is found, it is returned in a
*   LOCKED state.  It is the caller's responsibility to unlock the stream.]
*
*Exceptions:
*
*******************************************************************************/

FILEX * __cdecl _getstream (
    void
    )
{
    REG2 FILEX * retval = NULL;
    REG1 int i;

    /* Get the iob[] scan lock */
    EnterCriticalSection(&csIobScanLock); // protects the __piob table

    /*
     * Loop through the __piob table looking for a free stream, or the
     * first NULL entry.
     */
    for (i = 0; i < _nstream; i++) 
    {

        if (__piob[i] != NULL) 
        {
            //if the stream is not inuse, return it.
            if (!inuse(__piob[i])) 
            {
                _lock_str2(i, __piob[i]);

                if (inuse(__piob[i])) 
                {
                    _unlock_str2(i, __piob[i]);
                    continue;
                }
                retval = __piob[i];
                break;
            }
        }
        else 
        {
            // allocate a new _FILEX, set _piob[i] to it and return a pointer to it.
            __piob[i] = _malloc_crt(sizeof(_FILEX));
            if (__piob[i])
            {
                memset(__piob[i], 0, sizeof(_FILEX));
                InitializeCriticalSection(&(__piob[i]->lock));
                __piob[i]->fd = i;
                EnterCriticalSection(&(__piob[i]->lock));
                retval = __piob[i];
            }
            break;
        }
    }

    // if the __piob array is full, grow it
    if (i == _nstream)
    {
        FILEX** newpiob;

        _ASSERTE(retval == NULL);

        if (_nstream <= (INT_MAX - NSTREAM_INCREMENT))
        {
            newpiob = _realloc_crt(__piob, (_nstream + NSTREAM_INCREMENT) * sizeof(FILEX *));
            if (newpiob)
            {
                __piob = newpiob;
                _nstream += NSTREAM_INCREMENT;

                // zero the newly allocated part of the __piob or we'll get GPFs on flsall & other scans
                // __piob[i] now points to the first newly allocated slot
                memset(&(__piob[i]), 0, NSTREAM_INCREMENT*sizeof(FILEX*));
            
                // i now indexes the first free element
                // allocate a new _FILEX, set _piob[i] to it and return a pointer to it.
                __piob[i] = _malloc_crt(sizeof(_FILEX));
                if (__piob[i])
                {
                    memset(__piob[i], 0, sizeof(_FILEX));
                    InitializeCriticalSection(&(__piob[i]->lock));
                    __piob[i]->fd = i;
                    EnterCriticalSection(&(__piob[i]->lock));
                    retval = __piob[i];
                }
            }
        }
    }     

    // Initialize the return stream.
    if (retval != NULL) 
    {
        retval->_flag = retval->_cnt = 0;
        retval->_ptr = retval->_base = NULL;
        retval->osfhnd = INVALID_HANDLE_VALUE;
        retval->peekch = '\n';
        retval->osfile = 0;
    }

    LeaveCriticalSection(&csIobScanLock);

    return(retval);
}
