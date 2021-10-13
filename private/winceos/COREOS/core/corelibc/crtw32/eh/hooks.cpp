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
*hooks.cpp - global (per-thread) variables and functions for EH callbacks
*
*
*Purpose:
*       global (per-thread) variables for assorted callbacks, and
*       the functions that do those callbacks.
*
*       Entry Points:
*
*       * terminate()
*       * unexpected()
*       * _inconsistency()
*
*       External Names: (only for single-threaded version)
*
*       * __pSETranslator
*       * __pTerminate
*       * __pUnexpected
*       * __pInconsistency
*
****/

#include <windows.h>
#include <corecrt.h>
#include <cruntime.h>
#include <mtdll.h>

#include <stdlib.h>
#include <exception>
#include <ehhooks.h>
#include <eh.h>

#pragma hdrstop

/////////////////////////////////////////////////////////////////////////////
//
// The global variables:
//

#if 1 // ndef _MT
std::terminate_handler  __pTerminate     = NULL;
std::unexpected_handler __pUnexpected    = &std::terminate;
_inconsistency_function __pInconsistency = &std::terminate;
#endif // !_MT

////////////////////////////////////////////////////////////////////////////
// terminate - call the terminate handler (presumably we went south).
//              THIS MUST NEVER RETURN!
//
// Open issues:
//      * How do we guarantee that the whole process has stopped, and not just
//        the current thread?
//

void __cdecl std::terminate(void)
{
    __try
    {
        //
        // Let the user wrap things up his or her way.
        //
        if (__pTerminate)
        {
            __try
            {
                __pTerminate();
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }
    __finally
    {
        //
        // If the terminate handler returned, faulted, or otherwise failed to
        // halt the process/thread, we'll do it.
        //
        __crtExitProcess(3);
    }
}

/////////////////////////////////////////////////////////////////////////////
//
// unexpected - call the unexpected handler (presumably we went south, or nearly).
//              THIS MUST NEVER RETURN!
//
// Open issues:
//      * How do we guarantee that the whole process has stopped, and not just
//        the current thread?
//

void __cdecl std::unexpected(void)
{
    //
    // Let the user wrap things up his or her way.
    //
    if (__pUnexpected)
    {
        __pUnexpected();
    }

    //
    // If the unexpected handler returned, give the terminate handler a chance.
    //
    std::terminate();
}

/////////////////////////////////////////////////////////////////////////////
//
// _inconsistency - call the inconsistency handler (Run-time processing error!)
//                THIS MUST NEVER RETURN!
//
// Open issues:
//      * How do we guarantee that the whole process has stopped, and not just
//        the current thread?
//

void __cdecl _inconsistency(void)
{
    //
    // Let the user wrap things up his or her way.
    //
    if (__pInconsistency)
    {
        __pInconsistency();
    }

    //
    // If the inconsistency handler returned, give the terminate handler a chance.
    //
    std::terminate();
}

