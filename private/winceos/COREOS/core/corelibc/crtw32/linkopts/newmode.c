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
*newmode.c - set new() handler mode to handle malloc failures
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Sets the global flag which controls whether the new() handler
*	is called on malloc failures.  The default behavior in Visual
*	C++ v2.0 and later is not to, that malloc failures return NULL
*	without calling the new handler.  Linking with this object changes
*	the start-up behavior to call the new handler on malloc failures.
*
*Revision History:
*	03-04-94  SKS	Original version.
*
*******************************************************************************/

#ifndef _POSIX_

#include <internal.h>

/* enable new handler calls upon malloc failures */

int _newmode = 1;	/* Malloc New Handler MODE */

#endif
