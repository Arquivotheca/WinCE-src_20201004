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

// string -- template string support functions
#include <string>
#include <stdexcept>

_STDDEFS_BEGIN

// report a length_error
_CRTIMP void __cdecl _Xlen()
    {throw length_error("string too long"); }

// report an out_of_range error
_CRTIMP void __cdecl _Xran()
    {throw out_of_range("invalid string position"); }

_STDDEFS_END

/*
 * Copyright (c) 1995 by P.J. Plauger.  ALL RIGHTS RESERVED. 
 * Consult your license regarding permissions and restrictions.
 */
