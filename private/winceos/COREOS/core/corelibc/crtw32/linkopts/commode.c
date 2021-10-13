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
*commode.c - set global file commit mode to commit
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Sets the global file commit mode flag to commit.  Linking with
*	this file sets all files to be opened in commit mode by default.
*
*Revision History:
*	07-11-90  SBM	Module created, based on asm version.
*	08-27-92  GJF	Don't build for POSIX.
*	11-07-04  MSL   Support pure
*
*******************************************************************************/

#ifndef _POSIX_
#define SPECIAL_CRTEXE

#include <cruntime.h>
#include <file2.h>
#include <internal.h>

/* set default file commit mode to commit */
int _commode = _IOCOMMIT;


#endif
