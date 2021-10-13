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
/*++

Module Name:

    muldiv.c

Abstract:

    This module implements the "Multiply and Divide" operation

--*/

#include "windows.h"

int
WINAPI
MulDiv (
    int nNumber,
    int nNumerator,
    int nDenominator
    )
/*++

Routine Description:

    This function multiples two 32-bit numbers forming a 64-bit product.
    The 64-bit product is rounded and then divided by a 32-bit divisor
    yielding a 32-bit result.

Arguments:

    nNumber - Supplies the multiplier.

    nNumerator - Supplies the multiplicand.

    nDenominator - Supplies the divisor.

Return Value:

    If the divisor is zero or an overflow occurs, then a value of -1 is
    returned as the function value. Otherwise, the rounded quotient is
    returned as the function value.

--*/
{

    LONG Negate;
    union {
        LARGE_INTEGER Product;
        struct {
            ULONG Quotient;
            ULONG Remainder;
        };
    } u;

    //
    // Compute the size of the result.
    //

    Negate = nNumber ^ nNumerator ^ nDenominator;

    //
    // Get the absolute value of the operand values.
    //

    if (nNumber < 0) {
        nNumber = - nNumber;
    }

    if (nNumerator < 0) {
        nNumerator = - nNumerator;
    }

    if (nDenominator < 0) {
        nDenominator = - nDenominator;
    }

    //
    // Compute the 64-bit product of the multiplier and multiplicand
    // values and round.
    //

    u.Product.QuadPart = 
        (__int64)nNumber * (__int64) nNumerator + ((ULONG)nDenominator / 2);

    //
    // If there are any high order product bits, then the quotient has
    // overflowed.
    //

    if ((ULONG)nDenominator > u.Remainder) {

        //
        // Divide the 64-bit product by the 32-bit divisor forming a 32-bit
        // quotient
        //

        u.Quotient = (ULONG)((ULONGLONG)u.Product.QuadPart / (ULONG)nDenominator);

        //
        // Compute the final signed result.
        //

        if ((int)u.Quotient >= 0) {
            if (Negate >= 0) {
                return (int)u.Quotient;

            } else {
                return -(int)u.Quotient;
            }
        }
    }

    return -1;
}

