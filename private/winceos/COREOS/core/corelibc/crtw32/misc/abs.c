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






