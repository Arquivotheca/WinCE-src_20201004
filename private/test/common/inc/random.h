//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++

Module Name:

    random.h

Abstract:

    This module provides functions to generate random data.

--*/


#ifndef  RANDOM_h
#define  RANDOM_h



#ifdef __cplusplus
extern "C" {
#endif

// Random Object
//
typedef struct tagRND
{
    int     table[55];
    int     j;
    int     k;
    int     init;
}
RND, *PRND;

// Definition of bit flags for RandString() and RND_string()
//
#define RS_LEN_TERM     0x1        // null terminated string.
#define RS_NO_UPPER     0x2        // No upper case characters
#define RS_NO_LOWER     0x4        // No lower case characters
#define RS_NO_NUM       0x8        // No numbers
#define RS_NO_SYM       0x10       // No printable symbols
#define RS_HIBIT        0x20       // All byte values accepted [0, 255].

extern RND      _DEFAULT_RANDOM_OBJECT;

extern PRND  RND_new(unsigned int seed);
extern int   RND_rand(PRND prnd);
extern void  RND_srand(PRND prnd, unsigned int seed);
extern int   RND_int(PRND prnd, int high, int low);
extern void  RND_string(PRND prnd, unsigned int flags, LPTSTR lpBuf, int  cnt);
extern DWORD RND_DWORD(PRND prnd, DWORD dwHigh, DWORD dwLow);
extern void  RND_del(PRND prnd);
extern int   RandInt(int high, int low);
extern DWORD RandDWORD(DWORD dwhigh, DWORD dwlow);
extern void  RandString(unsigned int flags, LPTSTR lpBuf, int  cnt);

#ifdef __cplusplus
}
#endif

#endif  // RANDOM_h
