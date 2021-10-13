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
*swscanf.c - read formatted data from wide-character string
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _swscanf() - reads formatted data from wide-character string
*
*Revision History:
*       11-21-91  ETC   Created from sscanf.c
*       05-16-92  KRS   Revised for new ISO spec.  format is wchar_t * now.
*       02-18-93  SRW   Make FILE a local and remove lock usage.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       02-07-94  CFW   POSIXify.
*       02-06-94  CFW   assert -> _ASSERTE.
*       01-06-99  GJF   Changes for 64-bit size_t.
*       09-12-00  GB    Merged with sscanf.c
*
*******************************************************************************/

#ifndef _POSIX_
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef UNICODE
#define UNICODE
#endif

#include <wchar.h>
#include "sscanf.c"
#endif /* _POSIX_ */
