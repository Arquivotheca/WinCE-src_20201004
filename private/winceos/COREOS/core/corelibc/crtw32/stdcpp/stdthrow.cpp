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
// throw -- terminate on thrown exception REPLACEABLE
#define _HAS_EXCEPTIONS 0
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <crtdbg.h>

_STD_BEGIN

#ifdef _DEBUG
_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Debug_message(const wchar_t *message, const wchar_t *file, unsigned int line)
	{	// report error and die
        if(::_CrtDbgReportW(_CRT_ASSERT, file, line, NULL, L"%s", message)==1)
        {
            ::_CrtDbgBreak();
        }
	}
_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Debug_message(const unsigned short *message, const unsigned short *file, unsigned int line)
	{	// report error and die
        _Debug_message((wchar_t *) message, (wchar_t *) file, line);
	}

#endif

_STD_END

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
