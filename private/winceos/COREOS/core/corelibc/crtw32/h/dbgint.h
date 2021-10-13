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
*dbgint.h - Supports debugging features of the C runtime library.
*
*
*Purpose:
*       Support CRT debugging features.
*
*       [Internal]
*
****/

#ifndef _INC_DBGINT
#define _INC_DBGINT

#include <crtdbg.h>

#ifdef  __cplusplus
extern "C" {
#endif

#define _malloc_crt                     malloc
#define _calloc_crt                     calloc
#define _realloc_crt                    realloc
#define _recalloc_crt                   _recalloc
#define _expand_crt                     _expand
#define _free_crt                       free
#define _msize_crt                      _msize

#ifdef __cplusplus
}
#endif

#endif /* _INC_DBGINT */

