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
*natstart.c - native startup tracking variable.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This module contains native startup tracking variable.
*
*Revision History:
*       05-09-04  GB    Module created.
*       11-07-04  MSL   Locking for native initialisation
*       03-08-05  AC    Make locking for native initialisation fiber-safe
*
*******************************************************************************/

#include <crtdefs.h>
#include <internal.h>

/*
 * __native_startup_state :
 * 0 - No initialization code executed yet.
 * 1 - In process of initialization.
 * 2 - Native initialization done.
 */
_PGLOBAL
#if defined(__cplusplus)
extern "C"
{
#endif
volatile unsigned int __native_dllmain_reason = __NO_REASON;
volatile __enative_startup_state __native_startup_state; /* process-wide state of native init */
void * volatile __native_startup_lock; /* fiber ID currently doing native init */
#if defined(__cplusplus)
}
#endif

