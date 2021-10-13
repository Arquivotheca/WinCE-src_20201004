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
*nlsdata1.c - globals for international library - small globals
*
*
*Purpose:
*    This module contains the globals:  __mb_cur_max, _decimal_point,
*    _decimal_point_length.  This module is always required.
*    This module is separated from nlsdatax.c for granularity.
*
*******************************************************************************/

#ifndef DLL_FOR_WIN32S

#include <stdlib.h>
#include <nlsint.h>

/*
 *  Value of MB_CUR_MAX macro.
 */
int __mb_cur_max = 1;

/*
 *  Localized decimal point string.
 */
char __decimal_point[] = ".";

/*
 *  Decimal point length, not including terminating null.
 */
size_t __decimal_point_length = 1;

#endif    /* DLL_FOR_WIN32S */
