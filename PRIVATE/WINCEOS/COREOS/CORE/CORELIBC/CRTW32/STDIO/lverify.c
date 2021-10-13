//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
    REG2 int count = 0;
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


