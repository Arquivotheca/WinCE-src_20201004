//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

// newop2 operator new(size_t, const nothrow_t&) for Microsoft C++

// Disable "C++ Exception Specification ignored" warning in OS build (-W3)
#pragma warning( disable : 4290 )  

#include <cstdlib>
#include <new>

extern "C" {
int _callnewh(size_t size);
}

void *operator new(size_t size, const std::nothrow_t&) throw()
	{	// try to allocate size bytes
	void *p;
	while ((p = malloc(size)) == 0)
		{	// buy more memory or return null pointer
			try {
				if (_callnewh(size) == 0)
					break;
			} catch (std::bad_alloc) {
				return (0);
			}
		}
	return (p);
	}

/*
 * Copyright (c) 1995 by P.J. Plauger.  ALL RIGHTS RESERVED. 
 * Consult your license regarding permissions and restrictions.
 */
