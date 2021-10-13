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
*share.h - defines file sharing modes for sopen
*
*
*Purpose:
*   This file defines the file sharing modes for sopen().
*
*       [Public]
*
****/

#ifndef _INC_SHARE
#define _INC_SHARE

#define _SH_DENYRW  0x10    /* deny read/write mode */
#define _SH_DENYWR  0x20    /* deny write mode */
#define _SH_DENYRD  0x30    /* deny read mode */
#define _SH_DENYNO  0x40    /* deny none mode */
#define _SH_SECURE  0x80    /* secure mode */

#endif  /* _INC_SHARE */

