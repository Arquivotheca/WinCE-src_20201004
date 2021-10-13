//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

