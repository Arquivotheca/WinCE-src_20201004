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


#ifndef __INTRRUPT_CMD_LINE_H__
#define __INTRRUPT_CMD_LINE_H__

/* these are inclusive */
#define TEST_IRQ_BEGIN (0)
#define TEST_IRQ_END (1024)


/* arguments for the tests */
#define ARG_STR_IRQ_BEGIN (_T("-irqBegin"))
#define ARG_STR_IRQ_END (_T("-irqEnd"))

#define ARG_STR_MAX_IRQ_PER_SYSINTRS (_T("-maxIrqPerSysintr"))

/* for IOCTL_HAL_REQUEST_IRQ */
#define ARG_STR_DEV_LOC (_T("-dev"))

void
printDisclaimer ();

void
printUsage ();

BOOL
handleCmdLineIRQRange (DWORD * pdwBegin, DWORD * pdwEnd);



#endif

