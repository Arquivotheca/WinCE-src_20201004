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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// newop3: operator new[](size_t, const nothrow_t&) for Microsoft C++

// Disable "C++ Exception Specification ignored" warning in OS build (-W3)
#pragma warning( disable : 4290 )  

#include <new>
#include <corecrt.h>
#include <rtcsup.h>

void * operator new[]( size_t cb, const std::nothrow_t&) throw()
{
    void *res = operator new(cb);

    RTCCALLBACK(_RTC_Allocate_hook, (res, cb, 0));

    return res;
}
