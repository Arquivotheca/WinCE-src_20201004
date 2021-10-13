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
/*
-------------------------------------------------------------------------------

Module Name:

    ft.h

Description:

    Declares the TUX function table and test function prototypes, except when
    included by globals.cpp; in that case, it defines the function table.

-------------------------------------------------------------------------------
*/

#if (!defined(__FT_H__) || defined(__GLOBALS_CPP__))

#ifndef __FT_H__
#define __FT_H__
#endif


// ----------------------------------------------------------------------------
// Local Macros
// ----------------------------------------------------------------------------

#ifdef __DEFINE_FTE__

    #undef BEGIN_FTE
    #undef FTE
    #undef FTH
    #undef END_FTE
    #define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    #define FTH(a, b) { TEXT(a), b, 0, 0, NULL },
    #define FTE(a, b, c, d, e) { TEXT(a), b, c, d, e },
    #define END_FTE { NULL, 0, 0, 0, NULL } };

#else // __DEFINE_FTE__

    #ifdef __GLOBALS_CPP__

        #define BEGIN_FTE

    #else // __GLOBALS_CPP__

        #define BEGIN_FTE extern FUNCTION_TABLE_ENTRY g_lpFTE[];

    #endif // __GLOBALS_CPP__

    #define FTH(a, b)
    #define FTE(a, b, c, d, e) TESTPROCAPI e(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
    #define END_FTE

#endif // __DEFINE_FTE__


/*
-------------------------------------------------------------------------------
To create the function table and function prototypes, two macros are available:

    FTH(description, level)
        (Function Table Header) Used for entries that don't have functions,
        entered only as headers (or comments) into the function table.

    FTE(description, level, param, code, function)
        (Function Table Entry) Used for all functions. DON'T use this macro
        if the "function" field is NULL. In that case, use the FTH macro.

You must not use the TEXT or _T macros here. This is done by the FTH and FTE
macros.

In addition, the table must be enclosed by the BEGIN_FTE and END_FTE macros.

The global function table will only be defined once (when it is included by
globals.cpp). The function table will be declared as 'external' when this
file is included by any other source file.

-------------------------------------------------------------------------------
*/


// ----------------------------------------------------------------------------
// TUX Function Table
// ----------------------------------------------------------------------------

#define BASE 1000

BEGIN_FTE
    FTH("Multi-Thread Read/Write Test",   0)
    FTE(   "Two threads, 1000 writes/thread, 1 sector/thread, starting sector: 100", 1,   2, 1001, TestProc )
    FTE(   "Three threads",                             1,   3, 1002, TestProc )
    FTE(   "Four threads",                              1,   4, 1003, TestProc )
    FTH("Multiple Scatter/Gather Buffers",0 )
    FTE(   "Buffer Sizes: (1X) Sector Size",            1,   1, 2001, MultiSG )
    FTE(   "Buffer Sizes: (1X, 2X) Sector Size",        1,   2, 2002, MultiSG )
    FTE(   "Buffer Sizes: (1X, 2X, 4x, 8x) Sector Size",1,   3, 2003, MultiSG )
    FTE(   "Buffer Sizes: (32X) Sector Size",           1,   4, 2004, MultiSG )
    FTH("Read/Write BVT", 0 )
    FTE(   "R/W Sectors 0, 1 and 2",                    1,   1, 3001, BVT )
    FTE(   "R/W Around (Total Sectors / 2)",            1,   2, 3002, BVT )
END_FTE

#endif
