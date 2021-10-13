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
// newop operator new(size_t) for Microsoft C++
#include <cstdlib>
#include <new>
#include <xutility>

_C_LIB_DECL
int __cdecl _callnewh(size_t count) _THROW1(_STD bad_alloc);
_END_C_LIB_DECL

void *__CRTDECL operator new(size_t count) _THROW1(_STD bad_alloc)
	{	// try to allocate size bytes
	void *p;
	while ((p = malloc(count)) == 0)
		if (_callnewh(count) == 0)
			{	// report no memory
			_STD _Xbad_alloc();
			}
	return (p);
	}

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
