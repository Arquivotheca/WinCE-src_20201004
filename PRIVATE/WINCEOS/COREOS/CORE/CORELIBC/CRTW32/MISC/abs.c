//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>
#include <cmnintrin.h>

#pragma warning(disable:4163)
#pragma function(abs)

int abs (int number) {
	return( number>=0 ? number : -number );
}


#pragma function (_abs64)


__int64 _abs64 (__int64 number) {
        return( number>=0 ? number : -number );
} 






