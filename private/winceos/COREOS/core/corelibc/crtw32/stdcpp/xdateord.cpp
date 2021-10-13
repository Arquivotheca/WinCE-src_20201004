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
// xdateord -- look up date ordering for Microsoft
#include <cruntime.h>
#include <locale>
#include <mtdll.h>
#include <setlocal.h>

_C_STD_BEGIN
#ifndef MRTDLL
extern "C"
#endif
int __CLRCALL_PURE_OR_CDECL _Getdateorder()
	{	// return date order for current locale
	wchar_t buf[2] = {0};
	GetLocaleInfoEx(___lc_locale_name_func()[LC_TIME], LOCALE_ILDATE,
		buf, sizeof (buf) / sizeof (buf[0]));
	return (buf[0] == L'0' ? std::time_base::mdy
		: buf[0] == L'1' ? std::time_base::dmy
		: buf[0] == L'2' ? std::time_base::ymd
		: std::time_base::no_order);
	}
_C_STD_END

/*
 * Copyright (c) 1992-2009 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V5.20:0009 */
