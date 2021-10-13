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
// thread0.cpp -- C++0X thread support functions
#include <yvals.h>

 #if _HAS_CPP0X
#include <thr/xthread>
#include <thread>
#include <mutex>
#include <system_error>

 #if _HAS_EXCEPTIONS
#include <exception>
#include <string>

 #else /* _HAS_EXCEPTIONS */
#include <cstdio>
 #endif /* _HAS_EXCEPTIONS */

static const char *const msgs[] =
	{	// error messages
	"device or resource busy",
	"invalid argument",
	"no such process",
	"not enough memory",
	"operation not permitted",
	"resource deadlock would occur",
	"resource unavailable try again"
	};

static const int codes[] =
	{	// system_error codes
	(int)_STD errc::device_or_resource_busy,
	(int)_STD errc::invalid_argument,
	(int)_STD errc::no_such_process,
	(int)_STD errc::not_enough_memory,
	(int)_STD errc::operation_not_permitted,
	(int)_STD errc::resource_deadlock_would_occur,
	(int)_STD errc::resource_unavailable_try_again
	};

_STD_BEGIN

 #if _HAS_EXCEPTIONS
_CRTIMP2_PURE void __cdecl _Throw_Cpp_error(int code)
	{	// throw error object
	throw _STD system_error(codes[code],
		_STD generic_category(), msgs[code]);
	}

 #else /* _HAS_EXCEPTIONS */
_CRTIMP2_PURE void __cdecl _Throw_Cpp_error(int code)
	{	// report system error
	_CSTD fputs(msgs[code], stderr);
	_CSTD abort();
	}
 #endif /* _HAS_EXCEPTIONS */

_CRTIMP2_PURE void __cdecl _Throw_C_error(int code)
	{	// throw error object for C error
	switch (code)
		{	// select the exception
		case _Thrd_nomem:
		case _Thrd_timedout:
			_Throw_Cpp_error(_RESOURCE_UNAVAILABLE_TRY_AGAIN);

		case _Thrd_busy:
			_Throw_Cpp_error(_DEVICE_OR_RESOURCE_BUSY);

		case _Thrd_error:
			_Throw_Cpp_error(_INVALID_ARGUMENT);
		}
	}

_EXTERN_C
_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Do_call(void *_Tgt)
	{	// call the target object
	static_cast<const _Once_pad *>(_Tgt)->_Call();
	}
_END_EXTERN_C

extern _CRTIMP2_PURE const adopt_lock_t adopt_lock = adopt_lock_t();
extern _CRTIMP2_PURE const defer_lock_t defer_lock = defer_lock_t();
extern _CRTIMP2_PURE const try_to_lock_t try_to_lock = try_to_lock_t();
_STD_END
 #endif /* _HAS_CPP0X */

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
