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
*binmode.c - set global file mode to binary
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Sets the global file mode flag to binary.  Linking with this file
*	sets all files to be opened in binary mode.
*
*Revision History:
*	06-08-89  PHG	Module created, based on asm version.
*	04-04-90  GJF	Added #include <cruntime.h>. Also, fixed the copyright.
*	01-17-91  GJF	ANSI naming.
*	01-23-92  GJF	Added #include <stdlib.h> (contains decl of _fmode).
*	08-27-92  GJF	Don't build for POSIX.
*	11-07-04  MSL   Support pure
*
*******************************************************************************/

#ifndef _POSIX_
#define SPECIAL_CRTEXE

#include <cruntime.h>
#include <fcntl.h>
#include <stdlib.h>

/* set default file mode */
int _fmode = _O_BINARY;

#endif
