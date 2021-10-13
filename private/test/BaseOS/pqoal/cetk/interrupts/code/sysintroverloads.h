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


#ifndef __SYS_INTR_OVERLOADS_H__
#define __SYS_INTR_OVERLOADS_H__

#include <windows.h>


/*
 * Allows the test code to easily call the ioctl.  This is the most
 * general entry point.  Supports all of the allocation methods.
 *
 * dwRequestType is the request type (see above).  Next comes the irq
 * array and its length.  Finally, the sysintr value is returned
 * through pdwSysIntr.
 *
 * returns true on success and false otherwise.
 */
BOOL
getSysIntr (DWORD dwRequestType, const DWORD *const dwIRQs, DWORD dwCount, 
        DWORD * pdwSysIntr);

/* Standard or old version of the call.  Supports only one irq. */
BOOL
getSysIntr(DWORD dwIRQ, DWORD * pdwSysIntr);

/* release a sysintr */
BOOL
releaseSysIntr(DWORD dwSysIntr);

/* make sure that a sysintr value is within range */
BOOL
checkSysIntrRange (DWORD dwSysIntr);

#endif

