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
*dbgsec.c - Debug CRT Security Check Helper
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines alternative version of _CrtSetCheckCount helper.
*
*Revision History:
*       05-14-05  JL    Module created.
*
*******************************************************************************/

#ifdef _DEBUG

/***
*int _CrtSetCheckCount(int fCheckCount)
*       - Redefine import version of _CrtSetCheckCount
*
*Purpose:
*       Deliberately overwrite the default behavior of MSVCRXXD.DLL import
*       version of _CrtSetCheckCount. This file is linked into MSVCMXX.DLL and
*       MSVCPXX.DLL. The purpose is to allow backwards compatibility for apps
*       compiled with older CRT to be able to execute against newer CRT without
*       causing asserts. Without this change, a managed app can load up new
*       MSVCMXXD.DLL which sets the buffer checking flag in MSVCRXXD.DLL, and
*       triggers checkings inside the 'n'versions of the secure CRT.
*
*Entry:
*       int fCheckCount - not used
*
*Exit:
*       return 0
*
*******************************************************************************/

int _CrtSetCheckCount(int fCheckCount)
{
    return 0;
}

#ifdef _M_IX86
int (__cdecl * _imp___CrtSetCheckCount)(int) = &_CrtSetCheckCount;
#else
int (__cdecl * __imp__CrtSetCheckCount)(int) = &_CrtSetCheckCount;
#endif

#endif  /* _DEBUG */
