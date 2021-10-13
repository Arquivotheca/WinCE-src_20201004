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

// This file disables unnecessary or extraneous compiler warnings
// It's necessary in order to get the windows headers and generated
// proxy code to compile at warning level 4

// For Windows.h:
#pragma warning(disable:4201)	// warning C4201: nameless struct/union
#pragma warning(disable:4514)	// warning C4514: unreferenced inline function has been removed
#ifndef DEBUG
#pragma warning(disable:4710)	// warning C4710: function 'XXXXX' not inlined
#endif

// For STL Headers
#pragma warning(disable:4097)	// warning C4097: typedef-name 'XXX' used as synonym for class-name 'YYY'
#pragma warning(disable:4100)	// warning C4100: unreferenced formal parameter
#pragma warning(disable:4512)	// warning C4512: 'XXX' : assignment operator could not be generated
#pragma warning(disable:4663)	// warning C4663: C++ language change: to explicitly specialize class template 'XXX' use the following syntax:
#pragma warning(disable:4786)	// warning C4786: 'XXX' : identifier was truncated to '255' characters in the debug information

// Some other common warning pragmas

// Feel free to paste these into your own code to avoid these warnings, but don't uncomment
// these in this global header file without a VERY good reason.
//#pragma warning(disable:4115)	// warning C4115: named type definition in parentheses
//#pragma warning(disable:4127)	// warning C4127: conditional expression is constant
//#pragma warning(disable:4152)	// warning C4152: function/data pointer conversion in expression
//#pragma warning(disable:4211)	// warning C4211: redefined extern to static
//#pragma warning(disable:4214)	// warning C4214: bit field types other than int
//#pragma warning(disable:4232)	// warning C4232: address of dllimport 'XXX' is not static, identity not guaranteed
//#pragma warning(disable:4239)	// warning C4239: nonstandard extension used : 'argument' : conversion from 'XXX' to 'XXX&'
//#pragma warning(disable:4310)	// warning C4310: cast truncates constant value

