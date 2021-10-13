//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
