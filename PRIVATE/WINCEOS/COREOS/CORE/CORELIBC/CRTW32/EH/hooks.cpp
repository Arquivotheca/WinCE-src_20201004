//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#include <mtdll.h>

#include <stdlib.h>
#include <exception>
#include <ehhooks.h>
#include <eh.h>

//#include <excpt.h>

#pragma hdrstop

/////////////////////////////////////////////////////////////////////////////
//
// The global variables:
//

#ifndef _MT
#ifdef _WIN32
_se_translator_function __pSETranslator = NULL;
#endif
std::terminate_handler      __pTerminate    = NULL;
std::unexpected_handler     __pUnexpected   = &std::terminate;
#endif // !_MT

_inconsistency_function __pInconsistency= &std::terminate;


////////////////////////////////////////////////////////////////////////////
// terminate - call the terminate handler (presumably we went south).
//              THIS MUST NEVER RETURN!
//
// Open issues:
//      * How do we guarantee that the whole process has stopped, and not just
//        the current thread?
//

_CRTIMP void __cdecl std::terminate(void) {

    __try {
      //
      // Let the user wrap things up their way.
      //
        if ( __pTerminate ) {
            __try {
            
              __pTerminate();
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
            //
            // Intercept ANY exception from the terminate handler
            //
            }
        } else {
            TerminateProcess(GetCurrentProcess(),3) ;
        }
    }
    __finally {
      //
      // If the terminate handler returned, faulted, or otherwise failed to
      // halt the process/thread, we'll do it.
      //
#if defined(_NTSUBSET_)
      KeBugCheck( (ULONG) STATUS_UNHANDLED_EXCEPTION );
#else
      TerminateProcess(GetCurrentProcess(),3) ;
#endif
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
        // Let the user wrap things up their way.
        //
        if ( __pUnexpected )
            __pUnexpected();

        //
        // If the unexpected handler returned, we'll give the terminate handler a chance.
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
#if 0   // def _WIN32 (SjB)
        __try {
            //
            // Let the user wrap things up their way.
            //
            if ( __pInconsistency )
                __try {
#endif
                    __pInconsistency();
#if 0   // def _WIN32 (SjB)
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                    //
                    // Intercept ANY exception from the terminate handler
                    //
                }
        }
        __finally {
            //
            // If the inconsistency handler returned, faulted, or otherwise
            // failed to halt the process/thread, we'll do it.
            //
            std::terminate();
        }
#else // !_WIN32
            std::terminate();
#endif
}
