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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//
//  Module: globals.cpp
//          Causes the header globals.h to actually define the global variables.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "logging.h"
#include <kato.h>

// By defining the symbol __GLOBALS_CPP__, we force the file globals.h to
// define, instead of declare, all global variables.
#define __GLOBALS_CPP__
#include "globals.h"

////////////////////////////////////////////////////////////////////////////////
// GetTestResult
//  Checks the kato object to see if failures, aborts, or skips were logged
//  and returns the result accordingly
//
// Parameters:
//  None.
//
// Return value:
//  TESTPROCAPI valud of either TPR_FAIL, TPR_ABORT, TPR_SKIP, or TPR_PASS.

CKato * g_pDebugLogKato = NULL;

TESTPROCAPI GetTestResult(void) 
{   
    // Check to see if we had any LOG_EXCEPTIONs or LOG_FAILs and, if so,
    // return TPR_FAIL
    //
    for(int i = 0; i <= LOG_FAIL; i++) 
    {
        if(g_pDebugLogKato->GetVerbosityCount(i)) 
        {
            return TPR_FAIL;
        }
    }

    // Check to see if we had any LOG_ABORTs and, if so, 
    // return TPR_ABORT
    //
    for( ; i <= LOG_ABORT; i++) 
    {
        if(g_pDebugLogKato->GetVerbosityCount(i)) 
        {
            return TPR_ABORT;
        }
    }

    // Check to see if we had LOG_SKIPs or LOG_NOT_IMPLEMENTEDs and, if so,
    // return TPR_SKIP
    //
    for( ; i <= LOG_NOT_IMPLEMENTED; i++) 
    {
        if (g_pDebugLogKato->GetVerbosityCount(i)) 
        {
            return TPR_SKIP;
        }
    }

    // If we got here, we only had LOG_PASSs, LOG_DETAILs, and LOG_COMMENTs
    // return TPR_PASS
    //
    return TPR_PASS;
}

//
// SetKato
//
//   Use this to cause DebugLog to output to a Kato logger instead of
//   OutputDebugString. Kato objects provide integration with Tux parameters
//   for logging (such as outputting to debugger, to a file, appending to a 
//   file, etc.). Use SetKato as soon as the Kato logger has been created.
//   SetKato can also be used to reset DebugOut to use OutputDebugString:
//   pass NULL to do this.
//
// Parameters:
//
//   pKato           Pointer to a Kato logger. NULL to reset to default logging.
//
// Return value:
//
//   void  
//
void SetKato(CKato * pKato)
{
    g_pDebugLogKato = pKato;
    return;
}

////////////////////////////////////////////////////////////////////////////////
// Debug
//  Printf-like debug logging. Uses either OutputDebugString or a Kato logger
//   object. By default, OutputDebugString is called. If SetKato is called with
//   a pointer to a Kato logger, DebugOut will use that Kato logger object.
//
// Parameters:
//  szFormat        Formatting string (as in printf).
//  ...             Additional arguments.
//
// Return value:
//  None.

void DebugDetailed(DWORD verbosity, LPCTSTR szFormat, ...)
{
    va_list pArgs;
    va_start(pArgs, szFormat);
    
    if(g_pDebugLogKato) {
        g_pDebugLogKato->LogV(verbosity, szFormat, pArgs);
    }
    else {
        static  TCHAR   szHeader[] = TEXT("");
        static const int iBUFSIZE = 1024;
        TCHAR   szBuffer[iBUFSIZE];
        errno_t  Error;
        int nChars =0;
        Error = _tcscpy_s (szBuffer,countof(szBuffer),szHeader);
        nChars = _vstprintf_s(
        szBuffer + countof(szHeader) - 1,countof(szBuffer),szFormat,
        pArgs);
        Error = _tcscat_s (szBuffer,countof(szBuffer), TEXT("\r\n"));

        OutputDebugString(szBuffer);        
    }
    va_end(pArgs);
}

////////////////////////////////////////////////////////////////////////////////
// Debug
//  Printf-like debug logging. Uses either OutputDebugString or a Kato logger
//   object. By default, OutputDebugString is called. If SetKato is called with
//   a pointer to a Kato logger, DebugOut will use that Kato logger object.
//
// Parameters:
//  szFormat        Formatting string (as in printf).
//  ...             Additional arguments.
//
// Return value:
//  None.

void Debug(LPCTSTR szFormat, ...)
{
    va_list pArgs;
    va_start(pArgs, szFormat);
    
    if(g_pDebugLogKato) {
        g_pDebugLogKato->LogV( LOG_DETAIL, szFormat, pArgs);
    }
    else {
        static  TCHAR   szHeader[] = TEXT("");
        static const int iBUFSIZE = 1024;
        TCHAR   szBuffer[iBUFSIZE];
        
        _tcscpy_s (szBuffer,countof(szBuffer),szHeader);
        int length = _vscwprintf(szFormat,pArgs) + 1; // terminating '\0'
        _vstprintf_s(
        szBuffer + countof(szHeader) - 1,countof(szBuffer),szFormat,pArgs);
        _tcscat_s (szBuffer,countof(szBuffer), TEXT("\r\n"));

        OutputDebugString(szBuffer);        
    }
    va_end(pArgs);

}
