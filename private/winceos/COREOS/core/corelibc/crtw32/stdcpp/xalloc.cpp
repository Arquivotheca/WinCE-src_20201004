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
// MINITHREADS
#include <stdexcept>
#include <windows.h>

namespace stdext {
	namespace threads {

_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Mtx_new(void *& _Ptr)
	{
	_Ptr = new CRITICAL_SECTION;
#ifdef _WIN32_WCE // no spin count in CE
	InitializeCriticalSection(static_cast<CRITICAL_SECTION *>(_Ptr));
#else
	InitializeCriticalSectionEx(static_cast<CRITICAL_SECTION *>(_Ptr),
		4000, 0);
#endif // _WIN32_WCE
	}

_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Mtx_delete(void * _Ptr)
	{
	DeleteCriticalSection(static_cast<CRITICAL_SECTION *>(_Ptr));
	delete static_cast<CRITICAL_SECTION *>(_Ptr);
	}

_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Mtx_lock(void * _Ptr)
	{
	EnterCriticalSection(static_cast<CRITICAL_SECTION *>(_Ptr));
	}

_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Mtx_unlock(void * _Ptr)
	{
	LeaveCriticalSection(static_cast<CRITICAL_SECTION *>(_Ptr));
	}

	} // namespace threads
} // namespace stdext

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
