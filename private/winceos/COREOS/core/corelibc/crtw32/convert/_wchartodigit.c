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

#include <wchar.h>

/***
*_wchartodigit(wchar_t) converts unicode character to its corresponding digit
*
*Purpose:
*   Convert unicode character to its corresponding digit
*
*Entry:
*   ch char to convert
*
*Exit:
*   good result: int 0-9
*
*   bad result: -1
*
*Exceptions:
*
*******************************************************************************/

int _wchartodigit(wchar_t ch)
{
#define DIGIT_RANGE_TEST(zero)  \
    if (ch < zero)              \
        return -1;              \
    if (ch < zero + 10)         \
    {                           \
        return ch - zero;       \
    }

    DIGIT_RANGE_TEST(0x0030)        // 0030;DIGIT ZERO
    if (ch < 0xFF10)                // FF10;FULLWIDTH DIGIT ZERO
    {
        DIGIT_RANGE_TEST(0x0660)    // 0660;ARABIC-INDIC DIGIT ZERO
        DIGIT_RANGE_TEST(0x06F0)    // 06F0;EXTENDED ARABIC-INDIC DIGIT ZERO
        DIGIT_RANGE_TEST(0x0966)    // 0966;DEVANAGARI DIGIT ZERO
        DIGIT_RANGE_TEST(0x09E6)    // 09E6;BENGALI DIGIT ZERO
        DIGIT_RANGE_TEST(0x0A66)    // 0A66;GURMUKHI DIGIT ZERO
        DIGIT_RANGE_TEST(0x0AE6)    // 0AE6;GUJARATI DIGIT ZERO
        DIGIT_RANGE_TEST(0x0B66)    // 0B66;ORIYA DIGIT ZERO
        DIGIT_RANGE_TEST(0x0C66)    // 0C66;TELUGU DIGIT ZERO
        DIGIT_RANGE_TEST(0x0CE6)    // 0CE6;KANNADA DIGIT ZERO
        DIGIT_RANGE_TEST(0x0D66)    // 0D66;MALAYALAM DIGIT ZERO
        DIGIT_RANGE_TEST(0x0E50)    // 0E50;THAI DIGIT ZERO
        DIGIT_RANGE_TEST(0x0ED0)    // 0ED0;LAO DIGIT ZERO
        DIGIT_RANGE_TEST(0x0F20)    // 0F20;TIBETAN DIGIT ZERO
        DIGIT_RANGE_TEST(0x1040)    // 1040;MYANMAR DIGIT ZERO
        DIGIT_RANGE_TEST(0x17E0)    // 17E0;KHMER DIGIT ZERO
        DIGIT_RANGE_TEST(0x1810)    // 1810;MONGOLIAN DIGIT ZERO


        return -1;
    }
#undef DIGIT_RANGE_TEST

                                    // FF10;FULLWIDTH DIGIT ZERO
    if (ch < 0xFF10 + 10)
    {
        return ch - 0xFF10;
    }
    return -1;

}
