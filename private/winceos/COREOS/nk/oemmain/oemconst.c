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
/*
 *              NK Kernel loader code
 *
 *
 * Module Name:
 *
 *              oemsconst.c
 *
 * Abstract:
 *
 *              This file implements stubs for constants that OEM can replace, before OEMInit is called
 *
 */
#include "kernel.h"
#ifdef MIPS
const DWORD dwOEMArchOverride = MIPS_FLAG_NO_OVERRIDE;
#endif

#ifdef SHx
const DWORD dwSHxIntEventCodeLength = SH4_INTEVT_LENGTH;
#endif

// Can be overwritten as FIXUPVARs in config.bib, eg:
//    nk.exe:LoaderPoolTarget             00000000 00600000 FIXUPVAR
//    nk.exe:LoaderPoolMaximum            00000000 00c00000 FIXUPVAR
const volatile DWORD LoaderPoolTarget              =   0x00300000;   // 3 Mb

const volatile DWORD FilePoolTarget                =  1*1024*1024;   // 1MB

// PageOutLow: start trimmer thread when page free count is below this value
const volatile DWORD PageOutLow                    = PAGEOUT_LOW;

// PageOutHigh: stop trimmer thread when page free count is above this value
const volatile DWORD PageOutHigh                   = PAGEOUT_HIGH;

