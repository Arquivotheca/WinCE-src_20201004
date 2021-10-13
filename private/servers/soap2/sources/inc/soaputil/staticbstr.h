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
