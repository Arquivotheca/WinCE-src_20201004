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
*cwprntfp.c - Positional version of cwprintf
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*   This has format validations & positional parameters as compared to cwprintf
*
*Revision History:
*   01-30-04   SJ   Stub module created.
*
*******************************************************************************/

#ifndef _POSIX_

#ifndef _UNICODE   /* CRT flag */
#define _UNICODE 1
#endif

#ifndef UNICODE    /* NT flag */
#define UNICODE 1
#endif

#define POSITIONAL_PARAMETERS 1
#define CPRFLAG 1
#include "output.c"

#endif /* _POSIX_ */
