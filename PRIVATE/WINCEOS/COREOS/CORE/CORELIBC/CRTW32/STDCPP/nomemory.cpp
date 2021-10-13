//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

// Disable "C++ Exception Specification ignored" warning in OS build (-W3)
#pragma warning( disable : 4290 )  

#include <libdefs>
#include <new>

_STDDEFS_BEGIN

_CRTIMP void __cdecl _Nomemory()
{   // report out of memory
    static const bad_alloc nomem;
    throw nomem;
}

const nothrow_t nothrow;

_STDDEFS_END

/*
 * Copyright (c) 1994 by P.J. Plauger.  ALL RIGHTS RESERVED. 
 * Consult your license regarding permissions and restrictions.
 */
