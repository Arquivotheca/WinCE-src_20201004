//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*nlsdata2.c - globals for international library - locale handles and code page
*
*
*Purpose:
*       This module defines the locale handles and code page.  The handles are
*       required by almost all locale dependent functions.  This module is
*       separated from nlsdatax.c for granularity.
*
*******************************************************************************/

#include <locale.h>
#include <setlocal.h>

/*
 *  Locale handles.
 */
LCID __lc_handle[LC_MAX-LC_MIN+1] = { 
        _CLOCALEHANDLE,
        _CLOCALEHANDLE,
        _CLOCALEHANDLE,
        _CLOCALEHANDLE,
        _CLOCALEHANDLE,
        _CLOCALEHANDLE
};

/*
 *  Code page.
 */
UINT __lc_codepage = _CLOCALECP;                /* CP_ACP */

/*
 * Code page for LC_COLLATE
 */
UINT __lc_collate_cp = _CLOCALECP;
