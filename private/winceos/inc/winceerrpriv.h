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
/*******************************************************************************
*                                                                              *
*   WinCeErrPriv.h --  private error code ranges used with FACILITY_WINDOWS_CE *
*                                                                              *
*                                                                              *
*******************************************************************************/

#ifndef _WINCEERRPRIV_
#define _WINCEERRPRIV_

#if defined (_MSC_VER) && (_MSC_VER >= 1020) && !defined(__midl)
#pragma once
#endif

#include <winerror.h>

// Unless otherwise specified, ranges should be considered to cover 0x40 codes

#define OSAXS_ERR_BASE      (WINCEPRIV_BASE+0x0000) // See Private\winceos\coreos\nk\osaxs\osaxsprotocol.h

#define HDSTUB_ERR_BASE     (WINCEPRIV_BASE+0x0040) // See Private\winceos\coreos\nk\inc\hdstub_common.h

#define GAL_BASE            (WINCEPRIV_BASE+0x0080) // See Private\apps\tele\tpcutil\gal.h

#define GALSEARCH_BASE      (WINCEPRIV_BASE+0x00C0) // See Private\apps\sync\test\GALSearch\Search.h

#define UPDATEBIN_BASE      (WINCEPRIV_BASE+0x0100) // See Private\apps\mds\inc\updatebin.h

#define DLAGENT_BASE        (WINCEPRIV_BASE+0x0140) // See Private\apps\mds\dlagent\dlacmn\basecmdwnd.h

#define WIDGET_ERR_BASE     (WINCEPRIV_BASE+0x0180) // See Private\shellw\apps\common\inc\widgeterrorcodes.h

#endif//_WINCEERRPRIV_
