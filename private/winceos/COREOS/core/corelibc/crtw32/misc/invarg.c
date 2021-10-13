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
/***
*invarg.c - stub for invalid argument handler
*
*Purpose:
*   defines _invalid_parameter() and _set_invalid_parameter_handler()
*
*******************************************************************************/

#include <stdlib.h>
#include <windows.h>
#include <errorrep.h>
#include <corecrt.h>

#pragma warning(disable: 4054 4055) // 'type cast' : to/from function pointer 'pfn' from/to data pointer 'void *'

#ifdef  __cplusplus
extern "C" {
#endif

/* internal helper functions for encoding and decoding pointers */
_CRTIMP void * __cdecl _encode_pointer(void *);
_CRTIMP void * __cdecl _decode_pointer(void *);

void __cdecl __crt_unrecoverable_error(
    const wchar_t *pszExpression,
    const wchar_t *pszFunction,
    const wchar_t *pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    );

/* global variable which stores the user handler */

_invalid_parameter_handler __pInvalidArgHandler;

void _initp_misc_invarg(void* enull)
{
    __pInvalidArgHandler = (_invalid_parameter_handler) enull;
}

/***
*void _invalid_arg() -
*
*Purpose:
*   Called when an invalid argument is passed into a CRT function
*
*Entry:
*   pszExpression - validation expression (can be NULL)
*   pszFunction - function name (can be NULL)
*   pszFile - file name (can be NULL)
*   nLine - line number
*   pReserved - reserved for future use
*
*Exit:
*   return from this function results in error being returned from the function
*   that called the handler
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP void __cdecl _invalid_parameter(
    const wchar_t *pszExpression,
    const wchar_t *pszFunction,
    const wchar_t *pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    )
{
    _invalid_parameter_handler pHandler = __pInvalidArgHandler;

    pszExpression;
    pszFunction;
    pszFile;

    pHandler = (_invalid_parameter_handler) _decode_pointer((PVOID)pHandler);
    if (pHandler != NULL)
    {
        pHandler(pszExpression, pszFunction, pszFile, nLine, pReserved);
        return;
    }

    __crt_unrecoverable_error(pszExpression, pszFunction, pszFile, nLine, pReserved);
}

#ifndef _DEBUG

/* wrapper which passes no debug info; not available in debug
 * used in inline code: we don't pass the null params, so we gain some
 * speed and space in code generation
 */
_CRTIMP void __cdecl _invalid_parameter_noinfo(void)
{
    _invalid_parameter(NULL, NULL, NULL, 0, 0);
}

#endif

/***
*void _set_invalid_parameter_handler(void) -
*
*Purpose:
*       Establish a handler to be called when a CRT detects a invalid parameter
*
*       This function is not thread-safe
*
*Entry:
*   New handler
*
*Exit:
*   Old handler
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP _invalid_parameter_handler __cdecl
_set_invalid_parameter_handler( _invalid_parameter_handler pNew )
{
    _invalid_parameter_handler pOld = NULL;

    pOld = __pInvalidArgHandler;
    pOld = (_invalid_parameter_handler) _decode_pointer((PVOID)pOld);
    pNew = (_invalid_parameter_handler) _encode_pointer((PVOID)pNew);

    __pInvalidArgHandler = pNew;

    return pOld;
}

_CRTIMP _invalid_parameter_handler __cdecl
_get_invalid_parameter_handler( )
{
    _invalid_parameter_handler pOld = NULL;

    pOld = __pInvalidArgHandler;
    pOld = (_invalid_parameter_handler) _decode_pointer((PVOID)pOld);

    return pOld;
}

#ifdef  __cplusplus
}
#endif


