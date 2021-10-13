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
*_flswbuf.c - flush buffer and output wide character.
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _flswbuf() - flush a file buffer and output a wide character.
*	If no buffer, make one.
*
*Revision History:
*	04-27-93  CFW	Module Created.
*	02-07-94  CFW	POSIXify.
*	02-16-95  GJF	Removed obsolete WPRFLAG.
*
*******************************************************************************/

#ifndef _POSIX_


#ifndef _UNICODE   /* CRT flag */
#define _UNICODE 1
#endif

#ifndef UNICODE	   /* NT flag */
#define UNICODE 1
#endif

#include "_flsbuf.c"

#endif /* _POSIX_ */
