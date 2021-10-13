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
#pragma once

#include <windows.h>
#include <utbase.h>

extern int g_ErrorCount;

#define RETURN_HRESULT_FROM_ERRORCOUNT() \
{\
    int iCurrentErrorCount = UTLogGetErrorCount();\
    if (iCurrentErrorCount > g_ErrorCount)\
    {\
        g_ErrorCount = iCurrentErrorCount;\
        return E_FAIL;\
    }\
    return S_OK;\
}\

#define ASSERTFAILURECLEANUP(expression) \
    if (!AssertFailure(__LINE__, (expression), L#expression)) \
    { \
        goto Cleanup; \
    } \

#define ASSERTFAILURECONTINUE(expression) \
    AssertFailure(__LINE__, (expression), L#expression) \

#define ASSERTSUCCESSCLEANUP(expression) \
    if (!AssertSuccess(__LINE__, (expression), L#expression)) \
    { \
        goto Cleanup; \
    } \

#define ASSERTSUCCESSCONTINUE(expression) \
    AssertSuccess(__LINE__, (expression), L#expression) \

#define ASSERTEQUAL(value, expected)  \
AssertEqual( \
    __LINE__, \
    (value), \
    (expected), \
    L#value) \

#define ASSERTNOTEQUAL(value, expected)  \
AssertNotEqual( \
    __LINE__, \
    (value), \
    (expected), \
    L#value) \

template<class _Tv, class _Te>
void AssertEqual(int line, _Tv value, _Te expected, const WCHAR * message)
{
    if (value == expected)
    {
        return;
    }

    UTLogError(L"AssertEqual failed on line %d for %s: expected %#x; found %#x",
        line,
        message,
        expected,
        value);
}
template<class _Tv, class _Te>
void AssertNotEqual(int line, _Tv value, _Te expected, const WCHAR * message)
{
    if (value != expected)
    {
        return;
    }

    UTLogError(L"AssertNotEqual failed on line %d for %s: found %#x",
        line,
        message,
        value);
}
void AssertEqual(int line, const WCHAR * value, const WCHAR * expected, const WCHAR * message);
void AssertNotEqual(int line, const WCHAR * value, const WCHAR * expected, const WCHAR * message);
bool AssertSuccess(int line, const HRESULT hr, const WCHAR * message);
bool AssertFailure(int line, const HRESULT hr, const WCHAR * message);

template<class _T>
inline int SafeRelease(_T & ptr)
{
    // if the pointer is null, then there are no objects
    int iRet = 0;
    if ((ptr))
    {
        iRet = (ptr)->Release();
        (ptr) = NULL;
    }
    return iRet;
}

