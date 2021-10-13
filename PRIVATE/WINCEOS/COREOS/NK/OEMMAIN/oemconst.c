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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
