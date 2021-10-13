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
*setmaxf.c - Set the maximum number of streams
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines _setmaxstdio(), a function which changes the maximum number
*       of streams (stdio-level files) which can be open simultaneously.
*
*Revision History:
*       03-08-95  GJF   Module defined (reluctantly)
*       12-28-95  GJF   Major rewrite of _setmaxstio (several bugs). Added
*                       the _getmaxstdio() function.
*       03-02-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       04-01-05  MSL   Integer overflow protection
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <malloc.h>
#include <internal.h>
#include <file2.h>
#include <mtdll.h>
#include <dbgint.h>

/***
*int _setmaxstdio(maxnum) - sets the maximum number of streams to maxnum
*
*Purpose:
*       Sets the maximum number of streams which may be simultaneously open
*       to maxnum. This is done by resizing the __piob[] array and updating
*       _nstream. Note that maxnum may be either larger or smaller than the
*       current _nstream value.
*
*Entry:
*       maxnum = new maximum number of streams
*
*Exit:
*       Returns maxnum, if successful, and -1 otherwise.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _setmaxstdio (
        int maxnum
        )
{
    void **newpiob;
    int i;
    int retval;

    /*
     * Make sure the request is reasonable.
     */
    _VALIDATE_RETURN(((maxnum >= _IOB_ENTRIES) && (maxnum <= _NHANDLE_)), EINVAL, -1);

    _mlock(_IOB_SCAN_LOCK);
    __try {
        /*
         * Try to reallocate the __piob array.
         */
        if ( maxnum > _nstream ) {
            if ( (newpiob = _recalloc_crt( __piob, maxnum, sizeof(void *) ))
                 != NULL )
            {
                /*
                 * Initialize new __piob entries to NULL
                 */
                for ( i = _nstream ; i < maxnum ; i++ ) 
                    newpiob[i] = NULL;

                retval = _nstream = maxnum;
                __piob = newpiob;
            }
            else
                retval = -1;
        }
        else if ( maxnum == _nstream )
            retval = _nstream;
        else {  /* maxnum < _nstream */
            retval = maxnum;
            /*
             * Clean up the portion of the __piob[] to be freed.
             */
            for ( i = _nstream - 1 ; i >= maxnum ; i-- ) 
                /*
                 * If __piob[i] is non-NULL, free up the _FILEX struct it
                 * points to. 
                 */
                if ( __piob[i] != NULL )
                    if ( !inuse( (FILE *)__piob[i] ) ) {
                        _free_crt( __piob[i] );
                    }
                    else {
                        /*
                         * _FILEX is still inuse! Don't free any anything and
                         * return failure to the caller.
                         */
                        retval = -1;
                        break;
                    }

            if ( retval != -1 )
                if ( (newpiob = _recalloc_crt( __piob, maxnum, sizeof(void *) ))
                     != NULL ) 
                {
                    _nstream = maxnum;      /* retval already set to maxnum */
                    __piob = newpiob;
                }
                else
                    retval = -1;
        }
    }
    __finally {
        _munlock(_IOB_SCAN_LOCK);
    }

    return retval;
}


/***
*int _getmaxstdio() - gets the maximum number of stdio files
*
*Purpose:
*       Returns the maximum number of simultaneously open stdio-level files.
*       This is the current value of _nstream.
*
*Entry:
*
*Exit:
*       Returns current value of _nstream.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _getmaxstdio (
        void
        )
{
        return _nstream;
}
