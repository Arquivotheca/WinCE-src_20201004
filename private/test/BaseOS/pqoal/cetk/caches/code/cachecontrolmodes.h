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
 * cacheControlModes.h
 */

/*
 * This file stores the modes both returned by the cache control
 * functions and used by the tux function table to specify the tests.
 */

#ifndef __CACHE_CONTROL_MODES_H
#define __CACHE_CONTROL_MODES_H

#define CT_IGNORED          0x00000000  /* this param is not used */

#define CT_ARRAY_SIZE_MASK  0x00000007
#define CT_CACHE_SIZE       0x00000001  /* use cache size */
#define CT_TWICE_CACHE_SIZE 0x00000002  /* use twice case size */
#define CT_USER_DEFINED     0x00000004  /* take size from cmd line */

#define CT_WRITE_MODE_MASK  0x00000038
#define CT_WRITE_THROUGH    0x00000008
#define CT_WRITE_BACK       0x00000010
#define CT_DISABLED         0x00000020

#define CT_ALLOC_PHYS_MEM   0x00000040

#endif

