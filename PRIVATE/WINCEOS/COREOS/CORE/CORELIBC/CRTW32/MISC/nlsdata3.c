//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
