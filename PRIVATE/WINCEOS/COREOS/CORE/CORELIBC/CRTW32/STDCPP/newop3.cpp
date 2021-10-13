//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
