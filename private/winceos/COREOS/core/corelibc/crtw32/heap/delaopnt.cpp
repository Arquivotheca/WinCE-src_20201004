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
// delaopnt -- operator delete[](void *, nothrow_t) REPLACEABLE
#ifdef CRTDLL
#undef CRTDLL
#endif

#ifdef MRTDLL
#undef MRTDLL
#endif

#define _USE_ANSI_CPP // suppress defaultlib directive for Std C++ Lib
#include <new>

extern void __CRTDECL operator delete[](void *ptr) _THROW0();

void __CRTDECL operator delete[](void *ptr,
	const std::nothrow_t&) _THROW0()
	{	// free an allocated object
	operator delete[](ptr);
	}

/*
* Copyright (c) 1992-2001 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V3.10:0009 */
