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
#include <corecrt.h>
#include <sect_attribs.h>


_CRTALLOC(".CRT$XIA") _PVFV __xi_a[] = { NULL };

_CRTALLOC(".CRT$XIZ") _PVFV __xi_z[] = { NULL };

_CRTALLOC(".CRT$XCA") _PVFV __xc_a[] = { NULL };

_CRTALLOC(".CRT$XCZ") _PVFV __xc_z[] = { NULL };

_CRTALLOC(".CRT$XPA") _PVFV __xp_a[] = { NULL };

_CRTALLOC(".CRT$XPZ") _PVFV __xp_z[] = { NULL };

_CRTALLOC(".CRT$XTA") _PVFV __xt_a[] = { NULL };

_CRTALLOC(".CRT$XTZ") _PVFV __xt_z[] = { NULL };

#pragma comment(linker, "/merge:.CRT=" CRTMERGESECT)

