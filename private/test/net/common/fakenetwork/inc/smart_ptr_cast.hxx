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
#include <auto_xxx.hxx>

//hack to be able to cast smart_ptr's of derived types ( might want to find a better place for this to live )
template <typename T>
class smart_ptr_static_cast : public ce::smart_ptr<T>
{
public:
    template <typename Y>
    smart_ptr_static_cast(const ce::smart_ptr<Y>& p)
        : ce::smart_ptr<T>(reinterpret_cast<const ce::smart_ptr<T>&>(p))
    {
        x = static_cast<Y*>(p);
    }
};


template <typename T>
class smart_ptr_reinterpret_cast : public ce::smart_ptr<T>
{
public:
    template <typename Y>
    smart_ptr_reinterpret_cast(const ce::smart_ptr<Y>& p)
        : ce::smart_ptr<T>(reinterpret_cast<const ce::smart_ptr<T>&>(p))
    {
        x = reinterpret_cast<T*>(static_cast<Y*>(p));
    }
};
