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
*strset_s.c - contains strset_s()
*
*Purpose:
*   strset_s() sets all of the characters in a string equal to a given character.
*
*******************************************************************************/

#include <string.h>
#include <internal_securecrt.h>

#define _FUNC_PROLOGUE
#define _FUNC_NAME _strset_s
#define _CHAR char
#define _CHAR_INT int
#define _DEST _Dst
#define _SIZE _SizeInBytes

#include <tcsset_s.inl>
