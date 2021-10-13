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
// xrngabort: abort on precondition failure in TR1 random number generators
#include <yvals.h>

  #include <stdlib.h>
  #include <stdio.h>
//  #include <random>

_STD_BEGIN
_CRTIMP2_PURE _NO_RETURN(__CLRCALL_PURE_OR_CDECL _Rng_abort(_In_z_ const char *_Msg));

void __CLRCALL_PURE_OR_CDECL _Rng_abort(const char *msg)
	{	/* abort on precondition failure */
	_CSTD fputs(msg, stderr);
	_CSTD fputc('\n', stderr);
	_CSTD abort();
	}
_STD_END

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
