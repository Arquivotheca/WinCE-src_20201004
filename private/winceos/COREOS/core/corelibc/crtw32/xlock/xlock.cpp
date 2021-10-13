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
*xlock.cpp - thread lock class
*
*Purpose:
*       Define lock class used to make STD C++ Library thread-safe.
*
*       This is only here to allow users to link with STL if _MT is defined
*       when they build.
*
*******************************************************************************/

#ifndef _MT
#define _MT
#endif

#include <xstddef>
#include <windows.h>
_STD_BEGIN

static CRITICAL_SECTION _CritSec;

static long _InitFlag = 0L;

static void _CleanUp()
{
    if (InterlockedExchange(&_InitFlag, 3L) == 2L)
        // Should be okay to delete critical section
        DeleteCriticalSection(&_CritSec);
}


_Lockit::_Lockit()
{
    // Most common case - just enter the critical section

    if (_InitFlag == 2L) {
        EnterCriticalSection(&_CritSec);
        return;
    }

    // Critical section either needs to be initialized.

    if (_InitFlag == 0L) {

        long InitFlagVal;

        if ((InitFlagVal = InterlockedExchange(&_InitFlag, 1L)) == 0L) {
            InitializeCriticalSection(&_CritSec);
            atexit(_CleanUp);
            _InitFlag = 2L;
        }
        else if (InitFlagVal == 2L)
            _InitFlag = 2L;
    }

    // If necessary, wait while another thread finishes initializing the
    // critical section

    while (_InitFlag == 1L)
        Sleep(1);

    if (_InitFlag == 2L)
        EnterCriticalSection(&_CritSec);
}


_Lockit::~_Lockit()
{
    if (_InitFlag == 2L) 
        LeaveCriticalSection(&_CritSec);
}

_STD_END

