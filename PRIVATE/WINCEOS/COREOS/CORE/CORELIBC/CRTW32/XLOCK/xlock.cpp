//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
        long InitFlagValue;

        if ( InitFlagValue = InterlockedExchange( &_InitFlag, 3L ) == 2L )
            // Should be okay to delete critical section
            DeleteCriticalSection( &_CritSec );
}

_Lockit::_Lockit()
{

        // Most common case - just enter the critical section

        if ( _InitFlag == 2L ) {
            EnterCriticalSection( &_CritSec );
            return;
        }

        // Critical section either needs to be initialized.

        if ( _InitFlag == 0L ) {

            long InitFlagVal;

            if ( (InitFlagVal = InterlockedExchange( &_InitFlag, 1L )) == 0L ) {
                InitializeCriticalSection( &_CritSec );
                atexit( _CleanUp );
                _InitFlag = 2L;
            }
            else if ( InitFlagVal == 2L )
                _InitFlag = 2L;
        }

        // If necessary, wait while another thread finishes initializing the
        // critical section

        while ( _InitFlag == 1L )
            Sleep( 1 );

        if ( _InitFlag == 2L )
            EnterCriticalSection( &_CritSec );
}

_Lockit::~_Lockit()
{
        if ( _InitFlag == 2L ) 
            LeaveCriticalSection( &_CritSec );
}

_STD_END

