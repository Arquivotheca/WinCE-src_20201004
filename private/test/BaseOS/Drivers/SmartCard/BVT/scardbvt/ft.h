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
/*-------------------------------------------------------------------------------


Module Name:

    ft.h

Description:

    Declares the TUX function table and test function prototypes, except when
    included by globals.cpp; in that case, it defines the function table.

-------------------------------------------------------------------------------*/

#ifndef __FT_H__

#define __FT_H__


// ----------------------------------------------------------------------------
// Local Macros
// ----------------------------------------------------------------------------
    #define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    #define FTH(a, b) { TEXT(a), b, 0, 0, NULL },
    #define FTE(a, b, c, d, e) { TEXT(a), b, c, d, e },
    #define END_FTE { NULL, 0, 0, 0, NULL } };


#if 0
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
#endif

#define BASE 1000
#define SPECIAL_CASE 2000
#define ASYNC_TEST 100



#endif
