/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@doc LIBRARY

Copyright (c) 1999-2000 Microsoft Corporation

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