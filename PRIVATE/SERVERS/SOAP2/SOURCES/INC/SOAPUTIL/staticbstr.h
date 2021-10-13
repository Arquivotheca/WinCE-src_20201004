//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      StaticBstr.h
//
// Contents:
//
//      Static BSTR class template
//
//----------------------------------------------------------------------------------

#ifndef __STATICBSTR_H_INCLUDED__
#define __STATICBSTR_H_INCLUDED__

#define DECLAR_STATIC_BSTR(n, s)   CStaticBstr<sizeof(s)> n
#define DEFINE_STATIC_BSTR(n, s)   CStaticBstr<sizeof(s)> n = { sizeof(s) - 2, s }

#pragma pack(push , 1)

template <int S> class CStaticBstr
{
public:
    DWORD m_Size;
    WCHAR m_String[ (S) >> 1 ];

    operator BSTR();
};

template <int S> inline CStaticBstr<S>::operator BSTR()
{
    return m_String;
}

#pragma pack(pop)

#endif  // __STATICBSTR_H_INCLUDED__
