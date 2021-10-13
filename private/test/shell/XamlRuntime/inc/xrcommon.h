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

//
//     XRCommon.h
//

#include <tux.h>
#include <kato.h>
#include <math.h>

////////////////////////////////////////////////////////////////////////////////
// Suggested log verbosities

#define LOG_EXCEPTION       0
#define LOG_FAIL            2
#define LOG_ABORT           4
#define LOG_SKIP            6
#define LOG_NOT_IMPLEMENTED 8
#define LOG_PASS            10
#define LOG_DETAIL          12
#define LOG_COMMENT         14
#define LOG_VERBOSE         16

#define RELEASE_DIR  L"\\release\\"

#define countof(x) \
    (sizeof(x)/sizeof((x)[0]))

#define CLEAN_PTR(x) \
    if(x)            \
    {                \
        delete(x);   \
        (x) = NULL;  \
    }

#if !defined(SAFE_RELEASE)
    #define SAFE_RELEASE(x) \
        if(x)               \
        {                   \
            (x)->Release(); \
            (x) = NULL;     \
        }
#endif

// Start - macros to get file, function, line (wide string)
#define WIDEN_INTERNAL(charString) L ## charString
#define WIDENXR(charString) WIDEN_INTERNAL(charString)
#define __WFILEXR__ WIDENXR(__FILE__)
#define __WFUNCTION__ WIDENXR(__FUNCTION__)
#define INT2CHARSTRING_INTERNAL(integral) #integral
#define INTEGRAL2WSTRING(integral) WIDENXR(INT2CHARSTRING_INTERNAL(integral))
#define VAR2WSTRING(var) WIDENXR(INT2CHARSTRING_INTERNAL(var))
#define __WLINE__ INTEGRAL2WSTRING(__LINE__)

#define LOG_LOCATION() \
    g_pKato->Log(LOG_COMMENT, L"File: %s, Function: %s, Line: %s", __WFILEXR__, __WFUNCTION__, __WLINE__)
// End - macros to get file, function, line (wide string)

#define FAIL(x)         \
    {                   \
        x = TPR_FAIL;   \
        LOG_LOCATION(); \
        goto Exit;      \
    }

#define SUCCEED(x)    \
    {                 \
        x = TPR_PASS; \
        goto Exit;    \
    }

#define CHK_PTR(pElement, Name, Result)                                          \
    if(pElement==NULL)                                                           \
    {                                                                            \
        g_pKato->Log(LOG_COMMENT, L"   ***Xamlruntime Error***   %s is NULL", Name); \
        FAIL(Result);                                                            \
    }

#define CHK_DO(pElement, Name, Result) \
    CHK_PTR(pElement, Name, Result)

#define CHK_HR(Func, FuncName, Result)                                                                     \
    if(FAILED(hr = Func))                                                                                  \
    {                                                                                                      \
        g_pKato->Log(LOG_COMMENT, L"   ***Xamlruntime Error***   %s failed with HRESULT: 0x%x", FuncName, hr); \
        FAIL(Result);                                                                                      \
    }

// Function is required to return TPR_PASS / TPR_FAIL / TPR_SKIP / TPR_ABORT
#define CHK_TESTPROCAPI(Function, FuncName)                                                                                                \
    {                                                                                                                                      \
        Result = Function;                                                                                                                 \
        if(Result!=TPR_PASS)                                                                                                               \
        {                                                                                                                                  \
            switch(Result)                                                                                                                 \
            {                                                                                                                              \
                case TPR_SKIP:                                                                                                             \
                    g_pKato->Log(LOG_COMMENT, L"   ***Xamlruntime Error***   %s failed with TESTPROCAPI result TPR_SKIP.", FuncName); break;   \
                case TPR_FAIL:                                                                                                             \
                    g_pKato->Log(LOG_COMMENT, L"   ***Xamlruntime Error***   %s failed with TESTPROCAPI result TPR_FAIL.", FuncName); break;   \
                case TPR_ABORT:                                                                                                            \
                    g_pKato->Log(LOG_COMMENT, L"   ***Xamlruntime Error***   %s failed with TESTPROCAPI result TPR_ABORT.", FuncName); break;  \
                default:                                                                                                                   \
                    g_pKato->Log(LOG_COMMENT, L"   ***Xamlruntime Error***   %s failed with TESTPROCAPI result %d.", FuncName, Result); break; \
            };                                                                                                                             \
            LOG_LOCATION();                                                                                                                \
            goto Exit;                                                                                                                     \
        }                                                                                                                                  \
    }

#define SAFE_FREEXRVALUE(x)                 \
    if(x)                                   \
    {                                       \
        (x)->ShouldFreeValuePointer = TRUE; \
        FreeXRValue(x);                     \
    }

// inconsistent casing between CHK_VAL_bool and CHK_BOOL
#define CHK_BOOL(bval, Text, Result)                                        \
    if((bval)!=true)                                                        \
    {                                                                       \
        g_pKato->Log(LOG_COMMENT, L"   ***Xamlruntime Error***   [%s] ", Text); \
        FAIL(Result);                                                       \
    }

#define CHK_VAL_int(ActualValue, ExpectedValue, Result)                                                              \
    if(ActualValue!=ExpectedValue)                                                                                   \
    {                                                                                                                \
        g_pKato->Log(LOG_COMMENT, L"   ***Xamlruntime Error***   Actual: %d, Expected: %d", ActualValue, ExpectedValue); \
        FAIL(Result);                                                                                                \
    }
