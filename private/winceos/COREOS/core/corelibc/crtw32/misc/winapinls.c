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
/***
* nlsapi.c - helper functions for Win32 NLS Apis
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains functions to work with Win32 NLS-specific APIs
*
*******************************************************************************/

#include <internal.h>
#include <Windows.h>

#ifndef _WCE_BOOTCRT
/*******************************************************************************
*__crtCompareStringEx() - Wrapper for CompareStringEx().
*
*******************************************************************************/
int __cdecl __crtCompareStringEx(
    LPCWSTR lpLocaleName,
    DWORD dwCmpFlags,
    LPCWSTR lpString1,
    int cchCount1,
    LPCWSTR lpString2,
    int cchCount2)
{
    return CompareStringEx(
        lpLocaleName,
        dwCmpFlags,
        lpString1,
        cchCount1,
        lpString2,
        cchCount2,
        NULL,
        NULL,
        0);
}

/*******************************************************************************
*__crtEnumSystemLocalesEx() - Wrapper for EnumSystemLocalesEx().
*
*******************************************************************************/
BOOL __cdecl __crtEnumSystemLocalesEx(
    LOCALE_ENUMPROCEX lpLocaleEnumProcEx,
    DWORD dwFlags,
    LPARAM lParam)
{
    return EnumSystemLocalesEx(
        lpLocaleEnumProcEx,
        dwFlags,
        lParam,
        NULL);
}

/*******************************************************************************
*__crtGetDateFormatEx() - Wrapper for GetDateFormatEx().
*
*******************************************************************************/
int __cdecl __crtGetDateFormatEx(
    LPCWSTR lpLocaleName,
    DWORD dwFlags,
    const SYSTEMTIME *lpDate,
    LPCWSTR lpFormat,
    LPWSTR lpDateStr,
    int cchDate)
{
    return GetDateFormatEx(
        lpLocaleName,
        dwFlags,
        lpDate,
        lpFormat,
        lpDateStr,
        cchDate,
        NULL);
}

/*******************************************************************************
*__crtGetLocaleInfoEx() - Wrapper for GetLocaleInfoEx().
*
*******************************************************************************/
int  __cdecl __crtGetLocaleInfoEx(
    LPCWSTR lpLocaleName,
    LCTYPE LCType,
    LPWSTR lpLCData,
    int cchData)
{
    return GetLocaleInfoEx(
        lpLocaleName,
        LCType,
        lpLCData,
        cchData);
}

/*******************************************************************************
*__crtGetTimeFormatEx() - Wrapper for GetTimeFormatEx().
*
*******************************************************************************/
int  __cdecl __crtGetTimeFormatEx(
    LPCWSTR lpLocaleName,
    DWORD dwFlags,
    const SYSTEMTIME *lpTime,
    LPCWSTR lpFormat,
    LPWSTR lpTimeStr,
    int cchTime)
{
    return GetTimeFormatEx(
        lpLocaleName,
        dwFlags,
        lpTime,
        lpFormat,
        lpTimeStr,
        cchTime);
}

/*******************************************************************************
*__crtGetUserDefaultLocaleName() - Wrapper for GetUserDefaultLocaleName().
*
*******************************************************************************/
int  __cdecl __crtGetUserDefaultLocaleName(
    LPWSTR lpLocaleName,
    int cchLocaleName)
{
    return GetUserDefaultLocaleName(
        lpLocaleName,
        cchLocaleName);
}

/*******************************************************************************
*__crtIsValidLocaleName() - Wrapper for IsValidLocaleName().
*
*******************************************************************************/
BOOL __cdecl __crtIsValidLocaleName(
    LPCWSTR lpLocaleName)
{
    return IsValidLocaleName(
        lpLocaleName);
}

#endif

/*******************************************************************************
*__crtLCMapStringEx() - Wrapper for LCMapStringEx().
*
*******************************************************************************/
int __cdecl __crtLCMapStringEx(
    LPCWSTR lpLocaleName,
    DWORD dwMapFlags,
    LPCWSTR lpSrcStr,
    int cchSrc,
    LPWSTR lpDestStr,
    int cchDest)
{
    return LCMapStringEx(
        lpLocaleName,
        dwMapFlags,
        lpSrcStr,
        cchSrc,
        lpDestStr,
        cchDest,
        NULL,
        NULL,
        0);
}

