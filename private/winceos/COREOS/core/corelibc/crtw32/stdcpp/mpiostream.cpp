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
// This file is built into msvcurt.lib
#if !defined(_M_CEE_PURE)
#error This file must be built with /clr:pure.
#endif

#include <fstream>
#include <iostream>

#pragma warning(disable: 4074)
#pragma init_seg(compiler)

_STD_BEGIN

#pragma warning(disable:4439)	// C4439: function with a managed parameter must have a __clrcall calling convention

// OBJECT DECLARATIONS
__PURE_APPDOMAIN_GLOBAL extern istream *_Ptr_cin = 0;
__PURE_APPDOMAIN_GLOBAL extern ostream *_Ptr_cout = 0;
__PURE_APPDOMAIN_GLOBAL extern ostream *_Ptr_cerr = 0;
__PURE_APPDOMAIN_GLOBAL extern ostream *_Ptr_clog = 0;

		// WIDE OBJECTS
__PURE_APPDOMAIN_GLOBAL extern wistream *_Ptr_wcin = 0;
__PURE_APPDOMAIN_GLOBAL extern wostream *_Ptr_wcout = 0;
__PURE_APPDOMAIN_GLOBAL extern wostream *_Ptr_wcerr = 0;
__PURE_APPDOMAIN_GLOBAL extern wostream *_Ptr_wclog = 0;

__PURE_APPDOMAIN_GLOBAL int ios_base::Init::_Init_cnt = -1;
_STD_END
