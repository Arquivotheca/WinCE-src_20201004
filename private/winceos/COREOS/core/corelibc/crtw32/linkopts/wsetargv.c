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
*wsetargv.c - generic _wsetargv routine (wchar_t version)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Linking in this module replaces the normal wsetargv with the
*       wildcard wsetargv.
*
*Revision History:
*       11-23-93  CFW   Module created.
*       03-27-01  PML   _[w]setargv now returns an int (vs7#231220)
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>

/***
*_wsetargv - sets wargv by calling __wsetargv
*
*Purpose:
*       Routine directly transfers to __wsetargv.
*
*Entry:
*       See __wsetargv.
*
*Exit:
*       See __wsetargv.
*
*Exceptions:
*       See __wsetargv.
*
*******************************************************************************/

int __CRTDECL  _wsetargv (
        void
        )
{
        return __wsetargv();
}
