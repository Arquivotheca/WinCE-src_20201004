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