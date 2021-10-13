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
//+----------------------------------------------------------------------------
//
//
// File:
//      HResultMap.h
//
// Contents:
//
//      Declarations of generic error mapping mechanism
//
//-----------------------------------------------------------------------------

#ifndef __HRESULTMAP_H_INCLUDED__
#define __HRESULTMAP_H_INCLUDED__

struct HResultMapEntry
{
    HRESULT hrFrom;
    HRESULT hrTo;
#ifdef _DEBUG
    int     line;
#endif
};

struct HResultMap
{
    DWORD               elc;
    HResultMapEntry    *elv;
    HRESULT             dflt;
#ifdef _DEBUG
    CHAR               *file;
#endif
};

HRESULT HResultToHResult(HResultMap *pMap, HRESULT hrFrom);

#define BEGIN_INLINE_ERROR_MAP() \
    { \
        static HResultMapEntry ErrorMapEntries[] = {

#ifdef _DEBUG
#define DEFINE_HRESULT_MAP(dflt) static HResultMap ErrorMap = { countof(ErrorMapEntries), ErrorMapEntries, dflt, __FILE__};
#define MAP_ERROR(hrFrom, hrTo) { hrFrom, hrTo, __LINE__ },
#else
#define DEFINE_HRESULT_MAP(dflt) static HResultMap ErrorMap = { countof(ErrorMapEntries), ErrorMapEntries, dflt };
#define MAP_ERROR(hrFrom, hrTo) { hrFrom, hrTo },
#endif

#define PASS_THROUGH(hr) MAP_ERROR(hr, hr)

#define END_INLINE_ERROR_MAP(hr, dflt) \
        }; \
        DEFINE_HRESULT_MAP(dflt) \
        \
        if (FAILED(hr)) \
        { \
            hr = HResultToHResult(&ErrorMap, hr); \
        } \
    }

#endif  // __HRESULTMAP_H_INCLUDED__

