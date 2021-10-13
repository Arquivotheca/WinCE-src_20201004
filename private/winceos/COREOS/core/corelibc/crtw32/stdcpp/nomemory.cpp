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
