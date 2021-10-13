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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <windows.h>
#include <tchar.h>
#include <kato.h>

//
// Maximum allowable length for OutputDebugString text
//
#define MAX_DEBUG_OUT 256
#define countof(x)  (sizeof(x)/sizeof(*(x)))

CKato * g_pDebugLogKato = NULL;

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

static void vDebugOut(LPCTSTR szFormat, va_list pArgs)
{
    static  TCHAR   szHeader[] = TEXT("D3DMQA: ");
    TCHAR   szBuffer[MAX_DEBUG_OUT];
    
    _tcscpy(szBuffer, szHeader);

    _vsntprintf(
        &szBuffer[countof(szHeader)-1],
		MAX_DEBUG_OUT - countof(szHeader),
        szFormat,
        pArgs);

	szBuffer[MAX_DEBUG_OUT-1] = '\0'; // Guarantee NULL termination, in worst case

    if (g_pDebugLogKato)
    {
        g_pDebugLogKato->Log(0, szBuffer);
    }
    else
    {
    	OutputDebugString(szBuffer);
    }

}

//
// DebugOut
//
//   Printf-like debug logging. Uses either OutputDebugString or a Kato logger
//   object. By default, OutputDebugString is called. If SetKato is called with
//   a pointer to a Kato logger, DebugOut will use that Kato logger object.
//
// Parameters:
//
//   szFormat        Formatting string (as in printf).
//   ...             Additional arguments.
//
// Return value:
//
//   void  
//
void DebugOut(LPCTSTR szFormat, ...)
{
    va_list pArgs;
    va_start(pArgs, szFormat);
    vDebugOut(szFormat, pArgs);
    va_end(pArgs);
}

//
// DebugOut
//
//   Printf-like debug logging. Uses either OutputDebugString or a Kato logger
//   object. By default, OutputDebugString is called. If SetKato is called with
//   a pointer to a Kato logger, DebugOut will use that Kato logger object.
//   This version will first print out the debug string given in szFormat, then
//   print out the file name and line number.
//   (Can be used with macro DO_ERROR: DebugOut(DO_ERROR, L"DebugMessage");
//
// Parameters:
//
//   szFile          Filename (such as provided with __FILE__ macro)
//   iLine           Line number (such as provided with __LINE__ macro)
//   szFormat        Formatting string (as in printf).
//   ...             Additional arguments.
//
// Return value:
//
//   void  
//
void DebugOut(int iLine, LPCTSTR szFile, LPCTSTR szFormat, ...)
{
    va_list pArgs;
    va_start(pArgs, szFormat);

    vDebugOut(szFormat, pArgs);

    va_end(pArgs);

    DebugOut(L"File: %s : %d", szFile, iLine);
}


