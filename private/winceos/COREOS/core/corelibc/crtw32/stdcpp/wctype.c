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
/***
*wctype.c - wctype function
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*Revision History:
*       03-26-04  AC    Added history
*       03-26-04  AC    Fixed isprint family
*       03-30-04  AC    Reverted the isprint fix
*
****/

#include <string.h>
#include <wctype.h>
#ifndef _YVALS
#include <yvals.h>
#endif

_C_STD_BEGIN

static const struct wctab {
	const char *s;
	wctype_t val;
	} tab[] = {
	{"alnum", _ALPHA|_DIGIT},
	{"alpha", _ALPHA},
	{"cntrl", _CONTROL},
	{"digit", _DIGIT},
	{"graph", _PUNCT|_ALPHA|_DIGIT},
	{"lower", _LOWER},
	{"print", _BLANK|_PUNCT|_ALPHA|_DIGIT},
	{"punct", _PUNCT},
	{"space", _SPACE},
	{"upper", _UPPER},
	{"xdigit", _HEX},
	{(const char *)0, 0}};

#pragma warning(disable:4273)	/* inconsistent with Microsoft header */
_MRTIMP2 wctype_t (__cdecl wctype)(const char *name)
	{	/* find classification for wide character */
	int n;

	for (n = 0; tab[n].s != 0; ++n)
		if (strcmp(tab[n].s, name) == 0)
			return (tab[n].val);
	return (0);
	}
#pragma warning(default:4273)
_C_STD_END

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
