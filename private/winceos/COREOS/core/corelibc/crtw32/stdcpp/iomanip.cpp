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
// iomanip -- instantiations of iomanip
#include <iomanip>
_STD_BEGIN
		// FUNCTION resetiosflags
static void __cdecl rsfun(ios_base& iostr, ios_base::fmtflags mask)
	{	// reset specified format flags
	iostr.setf(ios_base::_Fmtzero, mask);
	}

		// FUNCTION setiosflags
static void __cdecl sifun(ios_base& iostr, ios_base::fmtflags mask)
	{	// set specified format flags
	iostr.setf(ios_base::_Fmtmask, mask);
	}

		// FUNCTION setbase
static void __cdecl sbfun(ios_base& iostr, int base)
	{	// set base
	iostr.setf(base == 8 ? ios_base::oct
		: base == 10 ? ios_base::dec
		: base == 16 ? ios_base::hex
		: ios_base::_Fmtzero,
			ios_base::basefield);
	}

		// FUNCTION setprecision
static void __cdecl spfun(ios_base& iostr, streamsize prec)
	{	// set precision
	iostr.precision(prec);
	}

		// FUNCTION setw
static void __cdecl swfun(ios_base& iostr, streamsize wide)
	{	// set width
	iostr.width(wide);
	}

 #if _IS_EMBEDDED
		// FUNCTION setfill
static void __cdecl sffun(ios& iostr, char ch)
	{	// set fill
	iostr.fill(ch);
	}

_MRTIMP2 _SMNAME(char) __cdecl setfill(char ch)
	{	// manipulator to set fill
	return (_SMNAME(char)(&sffun, ch));
	}

_MRTIMP2 _SMNAME(fmt) __cdecl resetiosflags(ios_base::fmtflags mask)
	{	// manipulator to reset format flags
	return (_SMNAME(fmt)(&rsfun, mask));
	}

_MRTIMP2 _SMNAME(fmt) __cdecl setiosflags(ios_base::fmtflags mask)
	{	// manipulator to set format flags
	return (_SMNAME(fmt)(&sifun, mask));
	}

_MRTIMP2 _SMNAME(int) __cdecl setbase(int base)
	{	// manipulator to set base
	return (_SMNAME(int)(&sbfun, base));
	}

_MRTIMP2 _SMNAME(str) __cdecl setprecision(streamsize prec)
	{	// manipulator to set precision
	return (_SMNAME(str)(&spfun, prec));
	}

_MRTIMP2 _SMNAME(str) __cdecl setw(streamsize wide)
	{	// manipulator to set width
	return (_SMNAME(str)(&swfun, wide));
	}

 #else /* _IS_EMBEDDED */
_MRTIMP2 _Smanip<ios_base::fmtflags>
	__cdecl resetiosflags(ios_base::fmtflags mask)
	{	// manipulator to reset format flags
	return (_Smanip<ios_base::fmtflags>(&rsfun, mask));
	}

_MRTIMP2 _Smanip<ios_base::fmtflags>
	__cdecl setiosflags(ios_base::fmtflags mask)
	{	// manipulator to set format flags
	return (_Smanip<ios_base::fmtflags>(&sifun, mask));
	}

_MRTIMP2 _Smanip<int> __cdecl setbase(int base)
	{	// manipulator to set base
	return (_Smanip<int>(&sbfun, base));
	}

_MRTIMP2 _Smanip<streamsize> __cdecl setprecision(streamsize prec)
	{	// manipulator to set precision
	return (_Smanip<streamsize>(&spfun, prec));
	}

_MRTIMP2 _Smanip<streamsize> __cdecl setw(streamsize wide)
	{	// manipulator to set width
	return (_Smanip<streamsize>(&swfun, wide));
	}
 #endif /* _IS_EMBEDDED */
_STD_END

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
