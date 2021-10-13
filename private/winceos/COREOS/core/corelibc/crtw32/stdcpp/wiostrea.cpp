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
// wiostream -- _Winit members, dummy for Microsoft
#include <iostream>
_STD_BEGIN

		// OBJECT DECLARATIONS
__PURE_APPDOMAIN_GLOBAL int _Winit::_Init_cnt = -1;

_CRTIMP2_PURE __thiscall _Winit::_Winit()
	{	// initialize standard wide streams first time
	if (0 <= _Init_cnt)
		++_Init_cnt;
	else
		_Init_cnt = 1;
	}

_CRTIMP2_PURE __thiscall _Winit::~_Winit() _NOEXCEPT
	{	// flush standard wide streams last time
	if (--_Init_cnt == 0)
		{	// flush standard wide streams
		if (_Ptr_wcout != 0)
			_Ptr_wcout->flush();
		if (_Ptr_wcerr != 0)
			_Ptr_wcerr->flush();
		if (_Ptr_wclog != 0)
			_Ptr_wclog->flush();
		}
_STD_END
	}

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
