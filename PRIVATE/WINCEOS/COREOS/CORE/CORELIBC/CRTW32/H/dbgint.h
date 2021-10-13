//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#define _expand_crt                     _expand
#define _free_crt                       free
#define _msize_crt                      _msize

#ifdef __cplusplus
}
#endif

#endif /* _INC_DBGINT */

