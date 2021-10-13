//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@doc LIBRARY


Module Name:
    cmnwrap.h

Abstract:
    Declaration of QA wrapper functions for the NetCmn functions

Revision History:
     26-Feb-2001		Created

-------------------------------------------------------------------*/
#ifndef _NET_CMN_WRAPPER_H_
#define _NET_CMN_WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////
//  Command Line Parsing Routines
/////////////////////////////////////////////////////////////////////////////////////////////////
void QASetOptionChars(int NumChars, TCHAR *CharArray);
BOOL QAWasOption(LPCTSTR pszOption);
BOOL QAGetOption(LPCTSTR pszOption, LPTSTR *ppszArgument);
BOOL QAGetOptionAsInt(LPCTSTR pszOption, int *piArgument);
BOOL QAGetOptionAsDWORD(LPCTSTR pszOption, DWORD *pdwArgument);

#ifdef __cplusplus
}
#endif

#endif