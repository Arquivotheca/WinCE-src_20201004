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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Defines a set of global functions for easily and safely converting
// strings between Unicode and ASCII.Since all the functions have the
// same name, the compiler distinguished between them based on their
// usage signature. Hence, functions automatically translate between a
// given, fixed, character-type and TCHAR.
//
// For example, to convert from wide-character to TCHAR and thence to
// ASCII, just do this:
//
//      WCHAR *pWCString = GetWideString();
//      int     wcChars  = GetWideStringChars();
//      TCHAR   tcBuffer[1024];
//      char    mbBuffer[1024];
//
//      strconv(tcBuffer, pWCString, COUNTOF(tcBuffer), wcChars);
//      strconv(mbBuffer, tcBuffer, COUNTOF(mbBuffer));
//
// One of these functions is, essentially, a copy and the other converts
// from wide-character to multi-byte. Which is which depends on the
// current definition of UNICODE but the complier takes care of that
// automatically.
//
// As a further example, here is the same process done using dynamic
// string objects:
//
//      ce::wstring wcString = GetWideString();
//      ce::tstring tcString;
//      ce::string  mbString;
//
//      strconv(tcString, wcString, wc.String.length(), CP_UTF8);
//      strconv(mbString, tcString, -1, CP_UTF8);
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_StrConv_H_
#define _DEFINED_StrConv_H_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>
#include <string.hxx>

// ----------------------------------------------------------------------------
//
// Macros:
// 
#undef  COUNTOF
#define COUNTOF(a) (sizeof(a) / sizeof((a)[0]))

// ----------------------------------------------------------------------------
//
// tstring: a generic Unicode or ASCII dynamic-string class:
//
namespace ce {
#ifndef CE_TSTRING_DEFINED
#define CE_TSTRING_DEFINED
#ifdef UNICODE
typedef ce::wstring tstring;
#else
typedef ce::string  tstring;
#endif
#endif

// Translates from Unicode to ASCII or vice-versa:
// The input may be NULL or empty and, if the optional InChars
// argument is supplied, needs no null-termination.
// The output will ALWAYS be null-terminated and -padded.
HRESULT
strconv(
  __out_ecount(OutChars) OUT char        *pOut,
                         IN  const char  *pIn,
                         IN  int          OutChars,
                         IN  int          InChars  = -1,
                         IN  UINT         CodePage = CP_ACP,
                         OUT char      **ppOutEnd  = NULL);
HRESULT
strconv(
  __out_ecount(OutChars) OUT WCHAR       *pOut,
                         IN  const char  *pIn,
                         IN  int          OutChars,
                         IN  int          InChars  = -1,
                         IN  UINT         CodePage = CP_ACP,
                         OUT WCHAR     **ppOutEnd  = NULL);
HRESULT
strconv(
  __out_ecount(OutChars) OUT char        *pOut,
                         IN  const WCHAR *pIn,
                         IN  int          OutChars,
                         IN  int          InChars  = -1,
                         IN  UINT         CodePage = CP_ACP,
                         OUT char      **ppOutEnd  = NULL);
HRESULT
strconv(
  __out_ecount(OutChars) OUT WCHAR       *pOut,
                         IN  const WCHAR *pIn,
                         IN  int          OutChars,
                         IN  int          InChars  = -1,
                         IN  UINT         CodePage = CP_ACP,
                         OUT WCHAR     **ppOutEnd  = NULL);
HRESULT
strconv(
    OUT ce::string  *pOut,
    IN  const char  *pIn,
    IN  int          InChars  = -1,
    IN  UINT         CodePage = CP_ACP);
HRESULT
strconv(
    OUT ce::wstring *pOut,
    IN  const char  *pIn,
    IN  int          InChars  = -1,
    IN  UINT         CodePage = CP_ACP);
HRESULT
strconv(
    OUT ce::string  *pOut,
    IN  const WCHAR *pIn,
    IN  int          InChars  = -1,
    IN  UINT         CodePage = CP_ACP);
HRESULT
strconv(
    OUT ce::wstring *pOut,
    IN  const WCHAR *pIn,
    IN  int          InChars  = -1,
    IN  UINT         CodePage = CP_ACP);

};
#endif /* _DEFINED_StrConv_H_ */
// ----------------------------------------------------------------------------
