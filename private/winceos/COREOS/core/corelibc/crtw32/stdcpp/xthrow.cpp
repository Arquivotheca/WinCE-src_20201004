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
// exception handling support functions
#include <new>
#include <stdexcept>

_STD_BEGIN
_CRTIMP2_PURE _NO_RETURN(__CLRCALL_PURE_OR_CDECL _Xbad_alloc())
	{	// report a bad_alloc error
	_THROW_NCEE(_XSTD bad_alloc, _EMPTY_ARGUMENT);

	}
_STD_END

#ifndef _WIN32_WCE
_STD_BEGIN
_CRTIMP2_PURE _NO_RETURN(__CLRCALL_PURE_OR_CDECL _Xinvalid_argument(_In_z_ const char *_Message))
	{	// report an invalid_argument error
	_THROW_NCEE(invalid_argument, _Message);
	}

_CRTIMP2_PURE _NO_RETURN(__CLRCALL_PURE_OR_CDECL _Xlength_error(_In_z_ const char *_Message))
	{	// report a length_error
	_THROW_NCEE(length_error, _Message);
	}

_CRTIMP2_PURE _NO_RETURN(__CLRCALL_PURE_OR_CDECL _Xout_of_range(_In_z_ const char *_Message))
	{	// report an out_of_range error
	_THROW_NCEE(out_of_range, _Message);
	}

_CRTIMP2_PURE _NO_RETURN(__CLRCALL_PURE_OR_CDECL _Xoverflow_error(_In_z_ const char *_Message))
	{	// report an overflow error
	_THROW_NCEE(overflow_error, _Message);
	}

_CRTIMP2_PURE _NO_RETURN(__CLRCALL_PURE_OR_CDECL _Xruntime_error(_In_z_ const char *_Message))
	{	// report a runtime_error
	_THROW_NCEE(runtime_error, _Message);
	}
_STD_END

 #include <functional>
 #include <regex>

_STD_BEGIN
_CRTIMP2_PURE _NO_RETURN(__CLRCALL_PURE_OR_CDECL _Xbad_function_call())
	{	// report a bad_function_call error
	_THROW_NCEE(bad_function_call, 0);
	}

_CRTIMP2_PURE _NO_RETURN(__CLRCALL_PURE_OR_CDECL _Xregex_error(regex_constants::error_type _Code))
	{	// report a regex_error
	_THROW_NCEE(regex_error, _Code);
	}
_STD_END
#endif

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
