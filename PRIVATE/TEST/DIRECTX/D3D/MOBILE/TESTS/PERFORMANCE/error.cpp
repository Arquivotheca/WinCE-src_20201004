//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


Module Name:

    Error.cpp

Abstract:

   This file contains error handling support for the Optimal Sample

-------------------------------------------------------------------*/

// ++++ Include Files +++++++++++++++++++++++++++++++++++++++++++++++
#include "Optimal.hpp"

// ++++ Global Variables ++++++++++++++++++++++++++++++++++++++++++++

#ifdef FORCE_DEBUG_OUTPUT
toutputlevel g_outputlevel = OUTPUT_ALL;
#else
toutputlevel g_outputlevel = OUTPUT_FAILURESONLY;
#endif

terr g_errLast;             // Error return code of last function
terr errOK = 0;

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    CheckError

Description:

    Checks if an error occurred in the previous function

Arguments:


    TCHAR *tszErr         - TEXT chString to output if an error occurred

Return Value:

    FALSE if an error occurred  

-------------------------------------------------------------------*/
BOOL
CheckError(TCHAR *tszErr)
{
    TCHAR tszErr2[256];

    if (g_errLast == errOK)
    {
        if (g_outputlevel == OUTPUT_ALL)
        {
            wsprintf (tszErr2, TEXT("%s succeeded.\r\n"), tszErr);
            Output(LOG_PASS, tszErr2);          
        }
    }
    else
    {
        wsprintf(tszErr2, TEXT("****%s failed (Error # = 0x%08x).\r\n"), tszErr, g_errLast);
        Output(LOG_FAIL, tszErr2);          
    }

    return (g_errLast != errOK);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    DebugOutput

Description:

    Simple Debug Output mechanism.  If DEBUG is TRUE, then the function
    outputs the passed-in String and variables to the debug output
    Stream.  Otherwise, the function does nothing

Arguments:

    TCHAR *tszDest       - TEXT String to output if an error occurred

    ... (variable arg)   - List of variable arguments to embed in output
                           (similar to printf format)

Return Value:

    None

-------------------------------------------------------------------*/
#ifdef DEBUG
void DebugOutput(TCHAR *tszErr, ...)
{
    TCHAR tszErrOut[256];

    va_list valist;

    va_start (valist,tszErr);
    wvsprintf (tszErrOut, tszErr, valist);
    Output (LOG_COMMENT, tszErrOut);

    va_end (valist);
}
#endif

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    RetailOutput

Description:

    Simple Retail Output mechanism.  If the function outputs
    the passed-in String and variables to the debug output stream.

Arguments:

    TCHAR *tszDest       - TEXT String to output

    ... (variable arg)   - List of variable arguments to embed in output
                           (similar to printf format)

Return Value:

    None

-------------------------------------------------------------------*/
void RetailOutput(TCHAR *tszErr, ...)
{
    TCHAR tszErrOut[256];

    va_list valist;

    va_start (valist,tszErr);
    wvsprintf (tszErrOut, tszErr, valist);
    Output (LOG_DETAIL, tszErrOut);

    va_end (valist);
}
