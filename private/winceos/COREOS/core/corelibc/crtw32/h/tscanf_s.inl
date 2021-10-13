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
*tscanf_s.inl - general implementation of scanf_s
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the general algorithm for scanf_s and its variants.
*
*Revision History:
*       07-15-04  AC    Created
*
****/
 
#pragma warning(push)
#pragma warning(disable : 4793)
_FUNC_PROLOGUE
int __cdecl _FUNC_NAME(const _CHAR *_Format, ...)
{
    va_list arglist;
 
    /* validation section */
    _VALIDATE_POINTER_ERROR(_Format, -1);
 
    va_start(arglist, _Format);
    return _INPUT_FUNC(_Format, arglist);
}
#pragma warning(pop)
