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
* invalidcontinue.c - Set the invalid parameter handler to an empty function.
*
*   Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*
*Revision History:
*   05-07-04  AC    created file
*                   VSW#299599
*   01-09-05  AC    Moved to section D, so that this is invoked after
*                   the C startup section
*                   VSW#422266
*   02-11-05  AC    Remove redundant #pragma section
*                   VSW#445138
*
*******************************************************************************/
#include <stdlib.h>
#include <sect_attribs.h>
#include <internal.h>

int __set_emptyinvalidparamhandler(void);

void __empty_invalid_parameter_handler(
    const wchar_t *pszExpression,
    const wchar_t *pszFunction,
    const wchar_t *pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    )
{
    (pszExpression);
    (pszFunction);
    (pszFile);
    (nLine);
    (pReserved);
    return;
}

#if defined(__cplusplus)
extern "C++"
{
#if defined(_M_CEE_PURE)
    typedef void (__clrcall *_invalid_parameter_handler)(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t);
    typedef _invalid_parameter_handler _invalid_parameter_handler_m;
    _MRTIMP _invalid_parameter_handler __cdecl _set_invalid_parameter_handler(_In_opt_ _invalid_parameter_handler _Handlerh);
#endif
}
#endif

int __set_emptyinvalidparamhandler(void)
{
    _set_invalid_parameter_handler(__empty_invalid_parameter_handler);
    return 0;
}

#ifndef _M_CEE_PURE
_CRTALLOC(".CRT$XID") static _PIFV pinit = __set_emptyinvalidparamhandler;
#else
#pragma warning(disable:4074)
class __set_emptyinvalidparamhandler_class
{
public:
	__set_emptyinvalidparamhandler_class() { __set_emptyinvalidparamhandler(); }
};
#pragma init_seg(compiler)
static __set_emptyinvalidparamhandler_class __set_emptyinvalidparamhandler_instance;
#endif
