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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  

FunctionTrace.h

Abstract:
Useful functions to enable function entry/exit and call information to debug
zone output.


Notes: 
Depends upon the debug zone ZONE_FUNCTION being defined for proper
compilation.

--*/


#ifndef FUNCTION_TRACE_HEADER
#define FUNCTION_TRACE_HEADER


#ifndef ZONE_FUNCTION
#error The debug zone ZONE_FUNCTION must be defined to use the FunctionTrace.h utilities.
#endif // ZONE_FUNCTION


// Include some enums that need to be present even when DEBUG is not defined
#include <TCHAR.h>
#include <stdio.h>


/*
Disable common (and harmless) warning-level 4 compiler warnings
C4512 - 'class' : assignment operator could not be generated
C4514 - unreferenced inline/local function has been removed
C4710 - 'function' : function not inlined
C4201 - nonstandard extension used : nameless struct/union
From Toolbox header: "ToolBoxW4Settings.h"
*/
#pragma warning(disable: 4512 4514 4710 4201)

// Include Windows.h at compiler warning-level 3
// From Toolbox header: "Windows_W3.h"
#pragma warning(push, 3)
#include <Windows.h>
#pragma warning(pop)

// Macro to simplify determining the number of elements in an array (do *not*
// use this macro for pointers)

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(x) (sizeof(x)/sizeof((x)[0]))
#endif


#if defined(DEBUG)
#define FUNCTION_TRACE_DEBUG
#endif

// WinCE's SHIP_BUILD define overrides FUNCTION_TRACE_DEBUG
#if defined(SHIP_BUILD)
#if defined(FUNCTION_TRACE_DEBUG)
#undef FUNCTION_TRACE_DEBUG
#endif // defined(FUNCTION_TRACE_DEBUG)
#endif // defined(SHIP_BUILD)




// Defining FUNCTION_TRACE_DEBUG enables Function Trace Support
#if defined(FUNCTION_TRACE_DEBUG)

// Tracks entry to a function - FUNCTION_TRACE provides greater functionality
#define FUNCTION_TRACE_ENTER(n) DEBUGMSG(ZONE_FUNCTION, (TEXT(#n) TEXT("+\r\n"))); DWORD dwTBDFunctionEnterTime = GetTickCount()
// Tracks exit of a function - FUNCTION_TRACE provides greater functionality
#define FUNCTION_TRACE_EXIT(n) DEBUGMSG(ZONE_FUNCTION, (TEXT(#n) TEXT("- (%dms elapsed)\r\n"), GetTickCount()-dwTBDFunctionEnterTime))

// Tracks entry and exit of a function
class FUNCTION_TRACE_Implementation
    {
    public:
    FUNCTION_TRACE_Implementation(const TCHAR* const psFileName, LONG& rlTotalFunctionTime, LONG& rlNumberFunctionCalls) : m_rlTotalFunctionTime(rlTotalFunctionTime), m_rlNumberFunctionCalls(rlNumberFunctionCalls)
        {
        _tcsncpy(m_szFunctionName, psFileName, (ARRAY_LENGTH(m_szFunctionName)-1) );
        m_szFunctionName[ARRAY_LENGTH(m_szFunctionName)-1] = TEXT('\0');
        DEBUGMSG(ZONE_FUNCTION, (TEXT("%s+\r\n"), m_szFunctionName)); 
        m_dwTBDFunctionEnterTime = GetTickCount();
        }
    ~FUNCTION_TRACE_Implementation()
        {
        const DWORD dwFunctionTime = GetTickCount()-m_dwTBDFunctionEnterTime;
        InterlockedIncrement(&m_rlNumberFunctionCalls);
#if defined(UNDER_CE)
        InterlockedExchange(&m_rlTotalFunctionTime, m_rlTotalFunctionTime+dwFunctionTime);
#else // defined(UNDER_CE)
        InterlockedExchangeAdd(&m_rlTotalFunctionTime, dwFunctionTime);
#endif // defined(UNDER_CE)
        DEBUGMSG(ZONE_FUNCTION, (TEXT("%s- (Duration of this function call=%dms - Total time spent in this function=%dms - Total calls to this function=%d)\r\n"),
            m_szFunctionName, dwFunctionTime, m_rlTotalFunctionTime, m_rlNumberFunctionCalls));
        }
    private:
    TCHAR m_szFunctionName[MAX_PATH];
    DWORD m_dwTBDFunctionEnterTime;
    LONG& m_rlTotalFunctionTime;
    LONG& m_rlNumberFunctionCalls;
    };

#define FUNCTION_TRACE(n) static LONG lFUNCTION_TRACE_Number_Function_Calls = 0; static LONG lFUNCTION_TRACE_Total_Function_Time = 0; FUNCTION_TRACE_Implementation FuncTrace(TEXT(#n), lFUNCTION_TRACE_Total_Function_Time, lFUNCTION_TRACE_Number_Function_Calls)


#else // defined(FUNCTION_TRACE_DEBUG)

#define FUNCTION_TRACE_ENTER(n)
#define FUNCTION_TRACE_EXIT(n)
#define FUNCTION_TRACE(n)

#endif // defined(FUNCTION_TRACE_DEBUG)

#endif // FUNCTION_TRACE_HEADER

