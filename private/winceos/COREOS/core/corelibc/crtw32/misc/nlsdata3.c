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
*nlsdata3.c - globals for international library - locale id's
*
*
*Purpose:
*   This module contains the definition of locale id's.  These id's and
*   this file should only be visible to the _init_(locale category)
*   functions.  This module is separated from nlsdatax.c for granularity.
*   
*******************************************************************************/

#if !defined(DLL_FOR_WIN32S)

#include <locale.h>
#include <setlocal.h>

/*
 *  Locale id's.
 */
/* UNDONE: define struct consisting of LCID/LANGID, CTRY ID, and CP. */
LC_ID __lc_id[LC_MAX-LC_MIN+1] = {
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 }
};

#endif /* ! DLL_FOR_WIN32S */
