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
// wcout -- initialize standard wide output stream
#include <fstream>
#include <iostream>

#pragma warning(disable: 4074)
#pragma init_seg(compiler)
static std::_Init_locks initlocks;

_STD_BEGIN
		// OBJECT DECLARATIONS

__PURE_APPDOMAIN_GLOBAL static wfilebuf wfout(_cpp_stdout);
#if defined(_M_CEE_PURE)
__PURE_APPDOMAIN_GLOBAL extern wostream wcout(&wfout);
#else
__PURE_APPDOMAIN_GLOBAL extern _CRTDATA2 wostream wcout(&wfout);
#endif

		// INITIALIZATION CODE
struct _Init_wcout
	{	// ensures that wcout is initialized
	__CLR_OR_THIS_CALL _Init_wcout()
		{	// initialize wcout
		_Ptr_wcout = &wcout;
		if (_Ptr_wcin != 0)
			_Ptr_wcin->tie(_Ptr_wcout);
		if (_Ptr_wcerr != 0)
			_Ptr_wcerr->tie(_Ptr_wcout);
		if (_Ptr_wclog != 0)
			_Ptr_wclog->tie(_Ptr_wcout);
		}
	};
__PURE_APPDOMAIN_GLOBAL static _Init_wcout init_wcout;

_STD_END

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
