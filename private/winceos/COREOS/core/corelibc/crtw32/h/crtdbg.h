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
#ifndef _INC_CRTDBG
#define _INC_CRTDBG

#include "cruntime.h"


#ifndef DEBUG

 /****************************************************************************
 *
 * Debug OFF
 * Debug OFF
 * Debug OFF
 *
 ***************************************************************************/

/*  We allow our basic _ASSERT macros to be overridden by pre-existing definitions. 
    This is not the ideal mechanism, but is helpful in some scenarios and helps avoid
    multiple definition problems */

#ifndef _ASSERT
#define _ASSERT(expr)                   ((void)0)
#endif

#ifndef _ASSERTE
#define _ASSERTE(expr)                  ((void)0)
#endif

#ifndef _ASSERT_EXPR
#define _ASSERT_EXPR(expr, expr_str)    ((void)0)
#endif


#else   /* DEBUG */

 /****************************************************************************
 *
 * Debug ON
 * Debug ON
 * Debug ON
 *
 ***************************************************************************/

 #include <dbgapi.h>
 
#define __ASSERT_PRINT(msg, file, line) \
    RETAILMSG(1, (L"\r\n*** ASSERTION FAILED in " L##file L":" L## #line L"\r\n" msg L"\r\n"))
    
#define __ASSERT_AT(msg, file, line) \
    (__ASSERT_PRINT(msg, file, line), __debugbreak(), 0)
    

/* We use !! below to ensure that any overloaded operators used to evaluate expr do not end up at operator || */
#define _ASSERT_EXPR(expr, msg) \
    (void) ((!!(expr)) || __ASSERT_AT(msg, __FILE__, __LINE__))

#ifndef _ASSERT
#define _ASSERT(expr)   _ASSERT_EXPR((expr), L"")
#endif

#ifndef _ASSERTE
#define _ASSERTE(expr)  _ASSERT_EXPR((expr), L## #expr)
#endif


#endif /* DEBUG */

#endif /* _INC_CRTDBG */


