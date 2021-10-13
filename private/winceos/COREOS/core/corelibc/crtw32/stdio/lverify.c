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
*lverify.c - verify and lock stream
*
*
*Purpose:
*   defines _lock_validate_str() - verify and lock a stream
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <mtdll.h>
#include <internal.h>
#include <dbgint.h>

int _lock_validate_str (
    _FILEX *stream
    )
{
    int i;
    int fFound = FALSE;

    if((!CheckStdioInit()) || (!stream))
        return FALSE;

    EnterCriticalSection(&csIobScanLock); // protects the __piob table

    for ( i = 0 ; i < _nstream ; i++ ) {

        if ( __piob[i] == stream ) {
            _lock_str2(i, __piob[i]);
            fFound = TRUE;
            break;
        }
    }

    LeaveCriticalSection(&csIobScanLock);

    return fFound;
}

