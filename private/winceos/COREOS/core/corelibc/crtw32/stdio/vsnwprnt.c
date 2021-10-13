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
*vsnwprnt.c - "Count" version of vswprintf
*
*	Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*	The _vsnwprintf() flavor takes a count argument that is
*	the max number of wide characters that should be written to the
*	user's buffer.
*
*Revision History:
*	05-16-91  KRS	Created from vsnprint.c
*       02-07-94  CFW   POSIXify.
*
*******************************************************************************/

#ifndef _POSIX_

#define _COUNT_ 1
#include "vswprint.c"

#endif /* _POSIX_ */
