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
*setargv.c - generic _setargv routine
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Linking in this module replaces the normal setargv with the
*       wildcard setargv.
*
*Revision History:
*       06-28-89  PHG   Module created, based on asm version.
*       04-09-90  GJF   Added #include <cruntime.h>. Made calling type
*                       _CALLTYPE1. Also, fixed the copyright.
*       10-08-90  GJF   New-style function declarator.
*       04-06-93  SKS   Replace _CRTAPI* with _cdecl
*       03-27-01  PML   _[w]setargv now returns an int (vs7#231220)
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>

/***
*_setargv - sets argv by calling __setargv
*
*Purpose:
*       Routine directly transfers to __setargv (defined in stdargv.asm).
*
*Entry:
*       See __setargv.
*
*Exit:
*       See __setargv.
*
*Exceptions:
*       See __setargv.
*
*******************************************************************************/

int __CRTDECL _setargv (
        void
        )
{
        return __setargv();
}
