//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*share.h - defines file sharing modes for sopen
*
*
*Purpose:
*	This file defines the file sharing modes for sopen().
*
*       [Public]
*
****/

#ifndef _INC_SHARE
#define _INC_SHARE

#define _SH_DENYRW	0x10	/* deny read/write mode */
#define _SH_DENYWR	0x20	/* deny write mode */
#define _SH_DENYRD	0x30	/* deny read mode */
#define _SH_DENYNO	0x40	/* deny none mode */

#endif	/* _INC_SHARE */

