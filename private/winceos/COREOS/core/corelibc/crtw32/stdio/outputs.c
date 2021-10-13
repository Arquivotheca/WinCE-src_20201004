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
*outputs.c - Secure version of printf & the printf family
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   This has format validations & positional parameters as compared to printf
*
*Revision History:
*   10-28-03   SJ   Stub module created.
*   01-30-04   SJ   VSW#228233 - splitting printf_s into 2 functions.
*
*******************************************************************************/

#ifndef _POSIX_

#define FORMAT_VALIDATIONS 1
#include "output.c"

#endif /* _POSIX_ */
