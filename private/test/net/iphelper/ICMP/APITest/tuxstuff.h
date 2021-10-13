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
#ifndef __TUXSTUFF_H__
#define __TUXSTUFF_H__

#include "IcmpTest.h"

#define USE_IPv6                       0x40000000
#define USE_ASYNC                       0x80000000

// Test function prototypes (TestProc's)
TESTPROCAPI IcmpAPITest                        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

#define ICMP_SEND_ECHO        0

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every 
// test case within the entire team to be uniquely identified.

#define BASE 0x00010000


#endif // __TUXSTUFF_H__
