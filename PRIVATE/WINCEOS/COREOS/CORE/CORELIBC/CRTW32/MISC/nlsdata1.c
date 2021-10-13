//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*nlsdata1.c - globals for international library - small globals
*
*
*Purpose:
*	This module contains the globals:  __mb_cur_max, _decimal_point,
*	_decimal_point_length.  This module is always required.
*	This module is separated from nlsdatax.c for granularity.
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

#endif	/* DLL_FOR_WIN32S */
