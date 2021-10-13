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
*wildcard.c - define the CRT internal variable _dowildcard
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	This variable is not public to users but is defined outside the
*	start-up code (CRTEXE.C) to reduce duplicate definitions.
*
*Revision History:
*	03-04-94  SKS	Initial version
*   04-30-01  BWT   Remove _NTSDK
*
*******************************************************************************/

#if !defined(_POSIX_) && (defined(CRTDLL)|| defined(MRTDLL))

#ifdef CRTDLL
#undef CRTDLL
#endif

#ifdef MRTDLL
#undef MRTDLL
#endif

#include <internal.h>

int _dowildcard = 0;	/* should be in <internal.h> */

#endif /* !_NTSDK && CRTDLL && !_POSIX_ */
