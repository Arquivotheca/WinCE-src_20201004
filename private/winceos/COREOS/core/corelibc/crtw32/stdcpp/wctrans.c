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
/* towctrans/wctrans functions for Microsoft */
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#ifndef _YVALS
#include <yvals.h>
#endif

 #pragma warning(disable:4244)

 #ifndef _WCTYPE_T_DEFINED
typedef wchar_t wint_t;
typedef wchar_t wctype_t;
 #endif

typedef wchar_t wctrans_t;

_C_STD_BEGIN

static const struct wctab {
	const char *s;
	wctype_t val;
	} tab[] = {
	{"tolower", 2},
	{"toupper", 1},
	{(const char *)0, 0}};

_MRTIMP2 wint_t (__cdecl towctrans)(wint_t c, wctrans_t val)
	{	/* translate wide character */
	return (val == 1 ? towupper(c) : towlower(c));
	}

_MRTIMP2 wctrans_t (__cdecl wctrans)(const char *name)
	{	/* find translation for wide character */
	int n;

	for (n = 0; tab[n].s != 0; ++n)
		if (strcmp(tab[n].s, name) == 0)
			return (tab[n].val);
	return (0);
	}
_C_STD_END

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
