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

#ifndef ALPHABLEND_H
#define ALPHABLEND_H

struct sTestColors5 {
        COLORREF crSource;
        COLORREF crDest;
        // for 16, 24, and 32bpp runs.
        COLORREF crExpected;
        COLORREF crExpected16;
        // for paletted.
        COLORREF crExpected8;
        COLORREF crExpected4;
        COLORREF crExpected2;
        COLORREF crExpected1;
        UINT alpha;
};

struct sTestColors {
        COLORREF crSource;
        COLORREF crDest;
        // for 16, 24, and 32bpp runs.
        COLORREF crExpected;
        UINT alpha;
};
 
struct sTestColors5 g_stcConstAlpha[] = { 
                                        // source           dest            expected    expected16   expected8   expected4   expected2     expected1     const alpha
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x00 }, // 0
                                         { 0x00123456, 0x00abcdef, 0x00aaccee, 0x00a8cce8, 0x00cccccc, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x01}, // 1
                                         { 0x00123456, 0x00abcdef, 0x00aaccee, 0x00a8cce8, 0x00cccccc, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x02}, // 2
                                         { 0x00123456, 0x00abcdef, 0x005f81a3, 0x006080a0, 0x00818181, 0x00808080, 0x00aaaaaa, 0x00ffffff, 0x7f}, // 3
                                         { 0x00123456, 0x00abcdef, 0x00133557, 0x00103450, 0x00353535, 0x00303030, 0x00555555, 0x00000000, 0xfe}, // 4
                                         { 0x00123456, 0x00abcdef, 0x00123456, 0x00103450, 0x00343434, 0x00303030, 0x00555555, 0x00000000, 0xff }, // 5
                                         };

struct sTestColors5 g_stcPPAlphaPrimary[] = {
                                        // source           dest            expected    expected16    expected8   expected4   expected2     expected1     const alpha
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x00 }, // 0
                                         { 0x01123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x00 }, // 1
                                         { 0x02123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x00 }, // 2
                                         { 0x7f123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x00 }, // 3
                                         { 0xfe123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x00 }, // 4
                                         { 0xff123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x00 }, // 5
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x01 }, // 6
                                         { 0x01123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x01 }, // 7
                                         { 0x02123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x01 }, // 8
                                         { 0x7f123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x01 }, // 9
                                         { 0xfe123456, 0x00abcdef, 0x00aaccee, 0x00a8cce8, 0x00cccccc, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x01 }, // 10
                                         { 0xff123456, 0x00abcdef, 0x00abccee, 0x00a8cce8, 0x00cccccc, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x01 }, // 11
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x02 }, // 12
                                         { 0x01123456, 0x00abcdef, 0x00accdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x02 }, // 13
                                         { 0x02123456, 0x00abcdef, 0x00accdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x02 }, // 14
                                         { 0x7f123456, 0x00abcdef, 0x00abccee, 0x00a8cce8, 0x00cccccc, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x02 }, // 15
                                         { 0xfe123456, 0x00abcdef, 0x00abcbed, 0x00a8cce8, 0x00cbcbcb, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x02 }, // 16
                                         { 0xff123456, 0x00abcdef, 0x00aacced, 0x00a8cce8, 0x00cccccc, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x02 }, // 17
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0x7f }, // 18
                                         { 0x01123456, 0x00abcdef, 0x00d6e7f8, 0x00d0e4f0, 0x00e7e7e7, 0x00e0e0e0, 0x00aaaaaa, 0x00ffffff, 0x7f }, // 19
                                         { 0x02123456, 0x00abcdef, 0x00d5e6f7, 0x00d0e4f0, 0x00e6e6e6, 0x00e0e0e0, 0x00aaaaaa, 0x00ffffff, 0x7f }, // 20
                                         { 0x7f123456, 0x00abcdef, 0x00acb4bd, 0x00a8b0b8, 0x00b4b4b4, 0x00b0b0b0, 0x00aaaaaa, 0x00ffffff, 0x7f }, // 21
                                         { 0xfe123456, 0x00abcdef, 0x00818181, 0x00808080, 0x00818181, 0x00808080, 0x00555555, 0x00ffffff, 0x7f }, // 22
                                         { 0xff123456, 0x00abcdef, 0x00818181, 0x00808080, 0x00818181, 0x00808080, 0x00aaaaaa, 0x00ffffff, 0x7f }, // 23
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0xfe }, // 24
                                         { 0x01123456, 0x00abcdef, 0x00ffffff, 0x00f8fcf8, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xfe }, // 25
                                         { 0x02123456, 0x00abcdef, 0x00ffffff, 0x00f8fcf8, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xfe }, // 26
                                         { 0x7f123456, 0x00abcdef, 0x00ac9b8a, 0x00a89c88, 0x009b9b9b, 0x00909090, 0x00aaaaaa, 0x00ffffff, 0xfe }, // 27
                                         { 0xfe123456, 0x00abcdef, 0x00573614, 0x00503410, 0x00363636, 0x00303030, 0x00555555, 0x00000000, 0xfe }, // 28
                                         { 0xff123456, 0x00abcdef, 0x00563513, 0x00503410, 0x00353535, 0x00303030, 0x00555555, 0x00000000, 0xfe }, // 29
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0x00a8cce8, 0x00cdcdcd, 0x00d0d0d0, 0x00aaaaaa, 0x00ffffff, 0xff }, // 30
                                         { 0x01123456, 0x00abcdef, 0x00ffffff, 0x00f8fcf8, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff }, // 31
                                         { 0x02123456, 0x00abcdef, 0x00ffffff, 0x00f8fcf8, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff }, // 32
                                         { 0x7f123456, 0x00abcdef, 0x00ac9b8a, 0x00a89c88, 0x009b9b9b, 0x00909090, 0x00aaaaaa, 0x00ffffff, 0xff }, // 33
                                         { 0xfe123456, 0x00abcdef, 0x00573513, 0x00503410, 0x00353535, 0x00303030, 0x00555555, 0x00000000, 0xff }, // 34
                                         { 0xff123456, 0x00abcdef, 0x00563412, 0x00503410, 0x00343434, 0x00303030, 0x00555555, 0x00000000, 0xff }, // 35
                                        };


struct sTestColors g_stcPPAlpha[] = {
                                            // source     dest              result           const alpha
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0x00 }, // 0
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0x01 }, // 1
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0x02 }, // 2
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0x7f }, // 3
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0xfe }, // 4
                                         { 0x00123456, 0x00abcdef, 0x00abcdef, 0xff }, // 5
                                         { 0x00123456, 0x01abcdef, 0x01abcdef, 0x00 }, // 6
                                         { 0x00123456, 0x01abcdef, 0x01abcdef, 0x01 }, // 7
                                         { 0x00123456, 0x01abcdef, 0x01abcdef, 0x02 }, // 8
                                         { 0x00123456, 0x01abcdef, 0x01abcdef, 0x7f }, // 9
                                         { 0x00123456, 0x01abcdef, 0x01abcdef, 0xfe }, // 10
                                         { 0x00123456, 0x01abcdef, 0x01abcdef, 0xff }, // 11
                                         { 0x00123456, 0x02abcdef, 0x02abcdef, 0x00 }, // 12
                                         { 0x00123456, 0x02abcdef, 0x02abcdef, 0x01 }, // 13
                                         { 0x00123456, 0x02abcdef, 0x02abcdef, 0x02 }, // 14
                                         { 0x00123456, 0x02abcdef, 0x02abcdef, 0x7f }, // 15
                                         { 0x00123456, 0x02abcdef, 0x02abcdef, 0xfe }, // 16
                                         { 0x00123456, 0x02abcdef, 0x02abcdef, 0xff }, // 17
                                         { 0x00123456, 0x7fabcdef, 0x7fabcdef, 0x00 }, // 18
                                         { 0x00123456, 0x7fabcdef, 0x7fabcdef, 0x01 }, // 19
                                         { 0x00123456, 0x7fabcdef, 0x7fabcdef, 0x02 }, // 20
                                         { 0x00123456, 0x7fabcdef, 0x7fabcdef, 0x7f }, // 21
                                         { 0x00123456, 0x7fabcdef, 0x7fabcdef, 0xfe }, // 22
                                         { 0x00123456, 0x7fabcdef, 0x7fabcdef, 0xff }, // 23
                                         { 0x00123456, 0xfeabcdef, 0xfeabcdef, 0x00 }, // 24
                                         { 0x00123456, 0xfeabcdef, 0xfeabcdef, 0x01 }, // 25
                                         { 0x00123456, 0xfeabcdef, 0xfeabcdef, 0x02 }, // 26
                                         { 0x00123456, 0xfeabcdef, 0xfeabcdef, 0x7f }, // 27
                                         { 0x00123456, 0xfeabcdef, 0xfeabcdef, 0xfe }, // 28
                                         { 0x00123456, 0xfeabcdef, 0xfeabcdef, 0xff }, // 29
                                         { 0x00123456, 0xffabcdef, 0xffabcdef, 0x00 }, // 30
                                         { 0x00123456, 0xffabcdef, 0xffabcdef, 0x01 }, // 31
                                         { 0x00123456, 0xffabcdef, 0xffabcdef, 0x02 }, // 32
                                         { 0x00123456, 0xffabcdef, 0xffabcdef, 0x7f }, // 33
                                         { 0x00123456, 0xffabcdef, 0xffabcdef, 0xfe }, // 34
                                         { 0x00123456, 0xffabcdef, 0xffabcdef, 0xff }, // 35
                                         { 0x01123456, 0x00abcdef, 0x00abcdef, 0x00 }, // 36
                                         { 0x01123456, 0x00abcdef, 0x00abcdef, 0x01 }, // 37
                                         { 0x01123456, 0x00abcdef, 0x00abcdf0, 0x02 }, // 38
                                         { 0x01123456, 0x00abcdef, 0x00b4e7ff, 0x7f }, // 39
                                         { 0x01123456, 0x00abcdef, 0x01bcffff, 0xfe }, // 40
                                         { 0x01123456, 0x00abcdef, 0x01bcffff, 0xff }, // 41
                                         { 0x01123456, 0x01abcdef, 0x01abcdef, 0x00 }, // 42
                                         { 0x01123456, 0x01abcdef, 0x01abcdef, 0x01 }, // 43
                                         { 0x01123456, 0x01abcdef, 0x01abcdf0, 0x02 }, // 44
                                         { 0x01123456, 0x01abcdef, 0x01b4e7ff, 0x7f }, // 45
                                         { 0x01123456, 0x01abcdef, 0x02bcffff, 0xfe }, // 46
                                         { 0x01123456, 0x01abcdef, 0x02bcffff, 0xff }, // 47
                                         { 0x01123456, 0x02abcdef, 0x02abcdef, 0x00 }, // 48
                                         { 0x01123456, 0x02abcdef, 0x02abcdef, 0x01 }, // 49
                                         { 0x01123456, 0x02abcdef, 0x02abcdf0, 0x02 }, // 50
                                         { 0x01123456, 0x02abcdef, 0x02b4e7ff, 0x7f }, // 51
                                         { 0x01123456, 0x02abcdef, 0x03bcffff, 0xfe }, // 52
                                         { 0x01123456, 0x02abcdef, 0x03bcffff, 0xff }, // 53
                                         { 0x01123456, 0x7fabcdef, 0x7fabcdef, 0x00 }, // 54
                                         { 0x01123456, 0x7fabcdef, 0x7fabcdef, 0x01 }, // 55
                                         { 0x01123456, 0x7fabcdef, 0x7fabcdf0, 0x02 }, // 56
                                         { 0x01123456, 0x7fabcdef, 0x7fb4e7ff, 0x7f }, // 57
                                         { 0x01123456, 0x7fabcdef, 0x80bcffff, 0xfe }, // 58
                                         { 0x01123456, 0x7fabcdef, 0x80bcffff, 0xff }, // 59
                                         { 0x01123456, 0xfeabcdef, 0xfeabcdef, 0x00 }, // 60
                                         { 0x01123456, 0xfeabcdef, 0xfeabcdef, 0x01 }, // 61
                                         { 0x01123456, 0xfeabcdef, 0xfeabcdf0, 0x02 }, // 62
                                         { 0x01123456, 0xfeabcdef, 0xfeb4e7ff, 0x7f }, // 63
                                         { 0x01123456, 0xfeabcdef, 0xfebcffff, 0xfe }, // 64
                                         { 0x01123456, 0xfeabcdef, 0xfebcffff, 0xff }, // 65
                                         { 0x01123456, 0xffabcdef, 0xffabcdef, 0x00 }, // 66
                                         { 0x01123456, 0xffabcdef, 0xffabcdef, 0x01 }, // 67
                                         { 0x01123456, 0xffabcdef, 0xffabcdf0, 0x02 }, // 68
                                         { 0x01123456, 0xffabcdef, 0xffb4e7ff, 0x7f }, // 69
                                         { 0x01123456, 0xffabcdef, 0xffbcffff, 0xfe }, // 70
                                         { 0x01123456, 0xffabcdef, 0xffbcffff, 0xff }, // 71
                                         { 0x02123456, 0x00abcdef, 0x00abcdef, 0x00 }, // 72
                                         { 0x02123456, 0x00abcdef, 0x00abcdef, 0x01 }, // 73
                                         { 0x02123456, 0x00abcdef, 0x00abcdf0, 0x02 }, // 74
                                         { 0x02123456, 0x00abcdef, 0x01b3e6ff, 0x7f }, // 75
                                         { 0x02123456, 0x00abcdef, 0x02bcffff, 0xfe }, // 76
                                         { 0x02123456, 0x00abcdef, 0x02bcffff, 0xff }, // 77
                                         { 0x02123456, 0x01abcdef, 0x01abcdef, 0x00 }, // 78
                                         { 0x02123456, 0x01abcdef, 0x01abcdef, 0x01 }, // 79
                                         { 0x02123456, 0x01abcdef, 0x01abcdf0, 0x02 }, // 80
                                         { 0x02123456, 0x01abcdef, 0x02b3e6ff, 0x7f }, // 81
                                         { 0x02123456, 0x01abcdef, 0x03bcffff, 0xfe }, // 82
                                         { 0x02123456, 0x01abcdef, 0x03bcffff, 0xff }, // 83
                                         { 0x02123456, 0x02abcdef, 0x02abcdef, 0x00 }, // 84
                                         { 0x02123456, 0x02abcdef, 0x02abcdef, 0x01 }, // 85
                                         { 0x02123456, 0x02abcdef, 0x02abcdf0, 0x02 }, // 86
                                         { 0x02123456, 0x02abcdef, 0x03b3e6ff, 0x7f }, // 87
                                         { 0x02123456, 0x02abcdef, 0x04bcffff, 0xfe }, // 88
                                         { 0x02123456, 0x02abcdef, 0x04bcffff, 0xff }, // 89
                                         { 0x02123456, 0x7fabcdef, 0x7fabcdef, 0x00 }, // 90
                                         { 0x02123456, 0x7fabcdef, 0x7fabcdef, 0x01 }, // 91
                                         { 0x02123456, 0x7fabcdef, 0x7fabcdf0, 0x02 }, // 92
                                         { 0x02123456, 0x7fabcdef, 0x80b3e6ff, 0x7f }, // 93
                                         { 0x02123456, 0x7fabcdef, 0x80bcffff, 0xfe }, // 94
                                         { 0x02123456, 0x7fabcdef, 0x80bcffff, 0xff }, // 95
                                         { 0x02123456, 0xfeabcdef, 0xfeabcdef, 0x00 }, // 96
                                         { 0x02123456, 0xfeabcdef, 0xfeabcdef, 0x01 }, // 97
                                         { 0x02123456, 0xfeabcdef, 0xfeabcdf0, 0x02 }, // 98
                                         { 0x02123456, 0xfeabcdef, 0xfeb3e6ff, 0x7f }, // 99
                                         { 0x02123456, 0xfeabcdef, 0xfebcffff, 0xfe }, // 100
                                         { 0x02123456, 0xfeabcdef, 0xfebcffff, 0xff }, // 101
                                         { 0x02123456, 0xffabcdef, 0xffabcdef, 0x00 }, // 102
                                         { 0x02123456, 0xffabcdef, 0xffabcdef, 0x01 }, // 103
                                         { 0x02123456, 0xffabcdef, 0xffabcdf0, 0x02 }, // 104
                                         { 0x02123456, 0xffabcdef, 0xffb3e6ff, 0x7f }, // 105
                                         { 0x02123456, 0xffabcdef, 0xffbcffff, 0xfe }, // 106
                                         { 0x02123456, 0xffabcdef, 0xffbcffff, 0xff }, // 107
                                         { 0x7f123456, 0x00abcdef, 0x00abcdef, 0x00 }, // 108
                                         { 0x7f123456, 0x00abcdef, 0x00abcdef, 0x01 }, // 109
                                         { 0x7f123456, 0x00abcdef, 0x01aaccef, 0x02 }, // 110
                                         { 0x7f123456, 0x00abcdef, 0x3f8ab4df, 0x7f }, // 111
                                         { 0x7f123456, 0x00abcdef, 0x7f689bce, 0xfe }, // 112
                                         { 0x7f123456, 0x00abcdef, 0x7f689bce, 0xff }, // 113
                                         { 0x7f123456, 0x01abcdef, 0x01abcdef, 0x00 }, // 114
                                         { 0x7f123456, 0x01abcdef, 0x01abcdef, 0x01 }, // 115
                                         { 0x7f123456, 0x01abcdef, 0x02aaccef, 0x02 }, // 116
                                         { 0x7f123456, 0x01abcdef, 0x408ab4df, 0x7f }, // 117
                                         { 0x7f123456, 0x01abcdef, 0x80689bce, 0xfe }, // 118
                                         { 0x7f123456, 0x01abcdef, 0x80689bce, 0xff }, // 119
                                         { 0x7f123456, 0x02abcdef, 0x02abcdef, 0x00 }, // 120
                                         { 0x7f123456, 0x02abcdef, 0x02abcdef, 0x01 }, // 121
                                         { 0x7f123456, 0x02abcdef, 0x03aaccef, 0x02 }, // 122
                                         { 0x7f123456, 0x02abcdef, 0x418ab4df, 0x7f }, // 123
                                         { 0x7f123456, 0x02abcdef, 0x80689bce, 0xfe }, // 124
                                         { 0x7f123456, 0x02abcdef, 0x80689bce, 0xff }, // 125
                                         { 0x7f123456, 0x7fabcdef, 0x7fabcdef, 0x00 }, // 126
                                         { 0x7f123456, 0x7fabcdef, 0x7fabcdef, 0x01 }, // 127
                                         { 0x7f123456, 0x7fabcdef, 0x80aaccef, 0x02 }, // 128
                                         { 0x7f123456, 0x7fabcdef, 0x9f8ab4df, 0x7f }, // 129
                                         { 0x7f123456, 0x7fabcdef, 0xbf689bce, 0xfe }, // 130
                                         { 0x7f123456, 0x7fabcdef, 0xbf689bce, 0xff }, // 131
                                         { 0x7f123456, 0xfeabcdef, 0xfeabcdef, 0x00 }, // 132
                                         { 0x7f123456, 0xfeabcdef, 0xfeabcdef, 0x01 }, // 133
                                         { 0x7f123456, 0xfeabcdef, 0xfeaaccef, 0x02 }, // 134
                                         { 0x7f123456, 0xfeabcdef, 0xfe8ab4df, 0x7f }, // 135
                                         { 0x7f123456, 0xfeabcdef, 0xfe689bce, 0xfe }, // 136
                                         { 0x7f123456, 0xfeabcdef, 0xfe689bce, 0xff }, // 137
                                         { 0x7f123456, 0xffabcdef, 0xffabcdef, 0x00 }, // 138
                                         { 0x7f123456, 0xffabcdef, 0xffabcdef, 0x01 }, // 139
                                         { 0x7f123456, 0xffabcdef, 0xffaaccef, 0x02 }, // 140
                                         { 0x7f123456, 0xffabcdef, 0xff8ab4df, 0x7f }, // 141
                                         { 0x7f123456, 0xffabcdef, 0xff689bce, 0xfe }, // 142
                                         { 0x7f123456, 0xffabcdef, 0xff689bce, 0xff }, // 143
                                         { 0xfe123456, 0x00abcdef, 0x00abcdef, 0x00 }, // 144
                                         { 0xfe123456, 0x00abcdef, 0x01aaccee, 0x01 }, // 145
                                         { 0xfe123456, 0x00abcdef, 0x02aacbee, 0x02 }, // 146
                                         { 0xfe123456, 0x00abcdef, 0x7f5f81a3, 0x7f }, // 147
                                         { 0xfe123456, 0x00abcdef, 0xfd133658, 0xfe }, // 148
                                         { 0xfe123456, 0x00abcdef, 0xfe133557, 0xff }, // 149
                                         { 0xfe123456, 0x01abcdef, 0x01abcdef, 0x00 }, // 150
                                         { 0xfe123456, 0x01abcdef, 0x02aaccee, 0x01 }, // 151
                                         { 0xfe123456, 0x01abcdef, 0x03aacbee, 0x02 }, // 152
                                         { 0xfe123456, 0x01abcdef, 0x805f81a3, 0x7f }, // 153
                                         { 0xfe123456, 0x01abcdef, 0xfd133658, 0xfe }, // 154
                                         { 0xfe123456, 0x01abcdef, 0xfe133557, 0xff }, // 155
                                         { 0xfe123456, 0x02abcdef, 0x02abcdef, 0x00 }, // 156
                                         { 0xfe123456, 0x02abcdef, 0x03aaccee, 0x01 }, // 157
                                         { 0xfe123456, 0x02abcdef, 0x04aacbee, 0x02 }, // 158
                                         { 0xfe123456, 0x02abcdef, 0x805f81a3, 0x7f }, // 159
                                         { 0xfe123456, 0x02abcdef, 0xfd133658, 0xfe }, // 160
                                         { 0xfe123456, 0x02abcdef, 0xfe133557, 0xff }, // 161
                                         { 0xfe123456, 0x7fabcdef, 0x7fabcdef, 0x00 }, // 162
                                         { 0xfe123456, 0x7fabcdef, 0x80aaccee, 0x01 }, // 163
                                         { 0xfe123456, 0x7fabcdef, 0x80aacbee, 0x02 }, // 164
                                         { 0xfe123456, 0x7fabcdef, 0xbf5f81a3, 0x7f }, // 165
                                         { 0xfe123456, 0x7fabcdef, 0xfe133658, 0xfe }, // 166
                                         { 0xfe123456, 0x7fabcdef, 0xfe133557, 0xff }, // 167
                                         { 0xfe123456, 0xfeabcdef, 0xfeabcdef, 0x00 }, // 168
                                         { 0xfe123456, 0xfeabcdef, 0xfeaaccee, 0x01 }, // 169
                                         { 0xfe123456, 0xfeabcdef, 0xfeaacbee, 0x02 }, // 170
                                         { 0xfe123456, 0xfeabcdef, 0xfe5f81a3, 0x7f }, // 171
                                         { 0xfe123456, 0xfeabcdef, 0xff133658, 0xfe }, // 172
                                         { 0xfe123456, 0xfeabcdef, 0xff133557, 0xff }, // 173
                                         { 0xfe123456, 0xffabcdef, 0xffabcdef, 0x00 }, // 174
                                         { 0xfe123456, 0xffabcdef, 0xffaaccee, 0x01 }, // 175
                                         { 0xfe123456, 0xffabcdef, 0xffaacbee, 0x02 }, // 176
                                         { 0xfe123456, 0xffabcdef, 0xff5f81a3, 0x7f }, // 177
                                         { 0xfe123456, 0xffabcdef, 0xff133658, 0xfe }, // 178
                                         { 0xfe123456, 0xffabcdef, 0xff133557, 0xff }, // 179
                                         { 0xff123456, 0x00abcdef, 0x00abcdef, 0x00 }, // 180
                                         { 0xff123456, 0x00abcdef, 0x01aaccee, 0x01 }, // 181
                                         { 0xff123456, 0x00abcdef, 0x02aaccee, 0x02 }, // 182
                                         { 0xff123456, 0x00abcdef, 0x7f5f81a3, 0x7f }, // 183
                                         { 0xff123456, 0x00abcdef, 0xfe133557, 0xfe }, // 184
                                         { 0xff123456, 0x00abcdef, 0xff123456, 0xff }, // 185
                                         { 0xff123456, 0x01abcdef, 0x01abcdef, 0x00 }, // 186
                                         { 0xff123456, 0x01abcdef, 0x02aaccee, 0x01 }, // 187
                                         { 0xff123456, 0x01abcdef, 0x03aaccee, 0x02 }, // 188
                                         { 0xff123456, 0x01abcdef, 0x805f81a3, 0x7f }, // 189
                                         { 0xff123456, 0x01abcdef, 0xfe133557, 0xfe }, // 190
                                         { 0xff123456, 0x01abcdef, 0xff123456, 0xff }, // 191
                                         { 0xff123456, 0x02abcdef, 0x02abcdef, 0x00 }, // 192
                                         { 0xff123456, 0x02abcdef, 0x03aaccee, 0x01 }, // 193
                                         { 0xff123456, 0x02abcdef, 0x04aaccee, 0x02 }, // 194
                                         { 0xff123456, 0x02abcdef, 0x805f81a3, 0x7f }, // 195
                                         { 0xff123456, 0x02abcdef, 0xfe133557, 0xfe }, // 196
                                         { 0xff123456, 0x02abcdef, 0xff123456, 0xff }, // 197
                                         { 0xff123456, 0x7fabcdef, 0x7fabcdef, 0x00 }, // 198
                                         { 0xff123456, 0x7fabcdef, 0x80aaccee, 0x01 }, // 199
                                         { 0xff123456, 0x7fabcdef, 0x80aaccee, 0x02 }, // 200
                                         { 0xff123456, 0x7fabcdef, 0xbf5f81a3, 0x7f }, // 201
                                         { 0xff123456, 0x7fabcdef, 0xfe133557, 0xfe }, // 202
                                         { 0xff123456, 0x7fabcdef, 0xff123456, 0xff }, // 203
                                         { 0xff123456, 0xfeabcdef, 0xfeabcdef, 0x00 }, // 204
                                         { 0xff123456, 0xfeabcdef, 0xfeaaccee, 0x01 }, // 205
                                         { 0xff123456, 0xfeabcdef, 0xfeaaccee, 0x02 }, // 206
                                         { 0xff123456, 0xfeabcdef, 0xfe5f81a3, 0x7f }, // 207
                                         { 0xff123456, 0xfeabcdef, 0xff133557, 0xfe }, // 208
                                         { 0xff123456, 0xfeabcdef, 0xff123456, 0xff }, // 209
                                         { 0xff123456, 0xffabcdef, 0xffabcdef, 0x00 }, // 210
                                         { 0xff123456, 0xffabcdef, 0xffaaccee, 0x01 }, // 211
                                         { 0xff123456, 0xffabcdef, 0xffaaccee, 0x02 }, // 212
                                         { 0xff123456, 0xffabcdef, 0xff5f81a3, 0x7f }, // 213
                                         { 0xff123456, 0xffabcdef, 0xff133557, 0xfe }, // 214
                                         { 0xff123456, 0xffabcdef, 0xff123456, 0xff }, // 215
                                        };
#endif
