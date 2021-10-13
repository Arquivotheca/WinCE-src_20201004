//
// Copyright (c) 2009 Microsoft Corporation.  All rights reserved.
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  Deleters.h
//    Provides basic RAII mechanisms for standard pointers and arrays.
//
//  Revision History:
//      Jonathan Leonard (a-joleo) : 11/17/2009 - Created.
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

template<typename T>
struct DeleteFunctor
{
    void operator()(T* p){ delete p; }
};

template<typename T>
struct ArrayDeleteFunctor
{
    void operator()(T* p){ delete [] p; }
};

template<typename T, typename FREE_FUNCTOR>
struct CGenericDeleter
{
    T*& p;
    CGenericDeleter(T*& ptr) : p(ptr) {}
    ~CGenericDeleter(){ FREE_FUNCTOR ff; ff(p); p = NULL; }
private:
    CGenericDeleter(const CGenericDeleter& right);
    CGenericDeleter& operator=(const CGenericDeleter& right);
};

template<typename T>
struct CDeleter
{
    typedef CGenericDeleter<T, DeleteFunctor<T>> type;
};

template<typename T>
struct CArrayDeleter
{
    typedef CGenericDeleter<T, ArrayDeleteFunctor<T>> type;
};