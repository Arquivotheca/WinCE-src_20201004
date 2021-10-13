//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
    FTH("Read/Write All Sectors"   , 0 )
    FTE(   "1 Sector at a time"    , 1,   1,       BASE+  1, WriteAllSectors )
    FTE(   "4 Sectors at a time"   , 1,   4,       BASE+  2, WriteAllSectors )
    FTE(   "16 Sectors at a time"  , 1,  16,       BASE+  3, WriteAllSectors )
    FTE(   "32 Sectors at a time"  , 1,  32,       BASE+  4, WriteAllSectors )
    FTE(   "64 Sectors at a time"  , 1,  64,       BASE+  5, WriteAllSectors )
    FTE(   "128 Sectors at a time" , 1, 128,       BASE+  6, WriteAllSectors )
    FTE(   "5 Sectors at a time"   , 1,   5,       BASE+  7, WriteAllSectors )
    FTE(   "13 Sectors at a time"  , 1,  13,       BASE+  8, WriteAllSectors )
    FTE(   "129 Sectors at a time" , 1, 129,       BASE+  9, WriteAllSectors )
END_FTE

#endif
