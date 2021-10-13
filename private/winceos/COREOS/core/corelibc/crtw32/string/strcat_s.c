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
*strcat_s.c - contains strcat_s()
*
*Purpose:
*   strcat_s() concatenates (appends) a copy of the source string to the
*   end of the destination string.
*
*******************************************************************************/

#include <string.h>
#include <internal_securecrt.h>

#define _FUNC_PROLOGUE
#define _FUNC_NAME strcat_s
#define _CHAR char
#define _DEST _Dst
#define _SIZE _SizeInBytes
#define _SRC _Src

#include <tcscat_s.inl>
