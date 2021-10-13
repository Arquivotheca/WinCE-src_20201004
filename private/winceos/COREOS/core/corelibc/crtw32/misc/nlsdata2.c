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
