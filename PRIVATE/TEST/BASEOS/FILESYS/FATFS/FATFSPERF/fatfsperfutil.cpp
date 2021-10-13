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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#include <main.h>
//-----------------------------------------------   
DWORD NearestPowerOfTwo(DWORD value )
//
//  Returns the nearest even power of two that is less than or equal to 
//  the input value
//-----------------------------------------------   
{
    DWORD powerOfTwo = 1;

    if(0x10000000 < value)
        return 0x10000000;

    while( powerOfTwo <= value ) 
        powerOfTwo <<= 1;

    return (powerOfTwo >> 1);
}

DWORD NearestIntervalOf33(DWORD value )
//
//  Returns the nearest even power of two that is less than or equal to 
//  the input value
//-----------------------------------------------   
{
    return value - (value % 33);

}