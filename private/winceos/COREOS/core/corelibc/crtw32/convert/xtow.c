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
*xtow.c - convert integers/longs to wide char string
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       The module has code to convert integers/longs to wide char strings.
*
*Revision History:
*       09-10-93  CFW   Module created, based on ASCII version.
*       02-07-94  CFW   POSIXify.
*       01-19-96  BWT   Add __int64 versions.
*       05-13-96  BWT   Fix _NTSUBSET_ version
*       08-21-98  GJF   Bryan's _NTSUBSET_ version is the correct
*                       implementation.
*       04-26-02  PML   Fix buffer overrun on _itow for radix 2 (VS7#525627)
*       05-11-02  BTW   Normalize the code in xtoa and just call it.  The "convert to 
*                       ansi, then run through mbstowcs" model used before is a
*                       waste of time.
*
*******************************************************************************/

#define _UNICODE
#include "xtoa.c"
