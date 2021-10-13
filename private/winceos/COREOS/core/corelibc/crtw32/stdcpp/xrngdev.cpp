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
// xrngdev: random device for TR1 random number generators
#define _CRT_RAND_S	// for rand_s
#include <stdexcept>	// for out_of_range

//  #include <random>
_STD_BEGIN
_CRTIMP2_PURE unsigned int __CLRCALL_PURE_OR_CDECL _Random_device();

_CRTIMP2_PURE unsigned int __CLRCALL_PURE_OR_CDECL _Random_device()
	{	// return a random value
	unsigned int ans;
	if (_CSTD rand_s(&ans))
		_Xout_of_range("invalid random_device value");
	return (ans);
	}

_STD_END

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
