//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// delete3: operator delete(void *, const nothrow_t&) for Microsoft C++

// Disable "C++ Exception Specification ignored" warning in OS build (-W3)
#pragma warning( disable : 4290 )  

#include <new>
#include <corecrt.h>
#include <rtcsup.h>

void operator delete( void * p, const std::nothrow_t&) throw() {
	LocalFree (p);
}


