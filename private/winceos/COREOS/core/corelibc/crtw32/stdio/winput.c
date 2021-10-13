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
*winput.c - wscanf style input from a FILEX (wchar_t version)
*
*
*Purpose:
*   This file defines the symbol UNICODE and then #includes the file
*   "input.c" in order to implement _winput(), the helper for the
*   wide character versions of the *wscanf() family of functions.
*
*******************************************************************************/

#define CRT_UNICODE
#include "input.c"
