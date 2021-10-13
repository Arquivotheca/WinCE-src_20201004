//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <tchar.h>

//
// Maximum allowable length for OutputDebugString text
//
#define MAX_DEBUG_OUT 256
#define countof(x)  (sizeof(x)/sizeof(*(x)))


//
// Debug
//
//   Printf-like wrapping around OutputDebugString.
//
// Arguments:  
//   
//   szFormat        Formatting string (as in printf).
//   ...             Additional arguments.
//   
// Return Value:
//  
//   void
//
void DebugOut(LPCTSTR szFormat, ...)
{
    static  TCHAR   szHeader[] = TEXT("D3DMQA: ");
    TCHAR   szBuffer[MAX_DEBUG_OUT];

    va_list pArgs;
    va_start(pArgs, szFormat);
    _tcscpy(szBuffer, szHeader);

    _vsntprintf(
        &szBuffer[countof(szHeader)-1],
		MAX_DEBUG_OUT - countof(szHeader),
        szFormat,
        pArgs);

	szBuffer[MAX_DEBUG_OUT-1] = '\0'; // Guarantee NULL termination, in worst case

	OutputDebugString(szBuffer);

    va_end(pArgs);

}


